#include "flatsouls_platform.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "flatsouls_logging.h"
#include "flatsouls_gl.h"

#define arrsize(a) (sizeof(a)/sizeof(*a))

typedef struct {
  char *curr, *end;
  char* mem;
} Arena;

typedef struct {
  char* data;
  int w,h;
} Bitmap;

#define arena_push(arena, type) arena_push_block(arena, sizeof(type))
void* arena_push_block(Arena *a, int size) {
  assert(a->curr + size <= a->end);
  a->curr += size;
  return a->curr - size;
}

GLuint compile_shader(const char* vertex_filename, const char* fragment_filename) {
  GLuint result = 0;
  FILE *vertex_file = 0, *fragment_file = 0;
  char shader_src[2048];
  const char * const shader_src_list = shader_src;
  char info_log[512];
  char* s = shader_src;
  GLint success;
  GLuint vertex_shader, fragment_shader;

  vertex_file = fopen(vertex_filename, "r");
  if (!vertex_file) LOG_AND_ABORT(LOG_ERROR, "Could not open vertex shader file: %s\n");
  s = shader_src; s += fread(s, 1, 2048, vertex_file); *s++ = 0;
  if (!feof(vertex_file) || ferror(vertex_file)) LOG_AND_ABORT(LOG_ERROR, "Could not read vertex shader: %s\n", strerror(errno));

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &shader_src_list, 0);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, 512, 0, info_log);
    LOG_AND_ABORT(LOG_ERROR, "Could not compile vertex shader: %s\n", info_log);
  }

  fragment_file = fopen(fragment_filename, "r");
  if (!fragment_file) LOG_AND_ABORT(LOG_ERROR, "Could not open fragment shader file: %s\n");
  s = shader_src; s += fread(s, 1, 2048, fragment_file); *s++ = 0;
  if (!feof(fragment_file) || ferror(fragment_file)) LOG_AND_ABORT(LOG_ERROR, "Could not read fragment shader: %s\n", strerror(errno));

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &shader_src_list, 0);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, 512, 0, info_log);
    LOG_AND_ABORT(LOG_ERROR, "Could not compile fragment shader: %s\n", info_log);
  }

  result = glCreateProgram();
  glAttachShader(result, vertex_shader);
  glAttachShader(result, fragment_shader);
  glLinkProgram(result);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  fclose(vertex_file);
  fclose(fragment_file);
  glOKORDIE;
  return result;
}

Bitmap load_bitmap(Arena *arena, const char *filename) {
  int err;
  Bitmap out;
  FILE *file;

  /* BMP header */
  unsigned int bfOffBits;
  unsigned int biSize;
  int biWidth;
  int biHeight;
  /* End BMP header */

  file = fopen(filename, "rb");
  if (!file) {fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno)); abort();}

  fseek(file, 10, SEEK_SET);
  fread(&bfOffBits, 4, 1, file);
  fread(&biSize, 4, 1, file);
  fread(&biWidth, 4, 1, file);
  fread(&biHeight, 4, 1, file);

  out.w = biWidth;
  out.h = biHeight;
  out.data = arena_push_block(arena, biWidth * biHeight * 4);

  fseek(file, bfOffBits, SEEK_SET);
  err = fread(out.data, 4, out.w * out.h, file);
  if (err != out.w * out.h) {fprintf(stderr, "Failed to read the bitmap pixels from %s: %s\n", filename, strerror(errno)); abort();}

  fclose(file);
  return out;
}

typedef struct {
  float x,y,z,w;
} vec4;

typedef struct {
  float x,y,z;
} vec3;

typedef struct {
  float x,y;
} vec2;

static vec4 vec4_create(float x, float y, float z, float w) {
  vec4 result;
  result.x = x; result.y = y; result.z = z; result.w = w;
  return result;
}

typedef enum {
  ENTITY_NULL,
  ENTITY_PLAYER
} EntityType;

typedef struct {
  EntityType type;
  vec3 pos;
} Entity;

typedef struct {
  vec3 camera_pos;
  GLuint spritesVAO, spritesVBO;
  GLuint sprite_shader;
  Bitmap bitmaps[256];
  struct {GLfloat x1,x2,y1,y2,u1,u2,v1,v2;} sprites[256];
  int num_sprites;
} Renderer;

typedef struct {
  Entity entities[256];
  int num_entities;
  Renderer renderer;
  Arena arena;
  char arena_data[128*1024*1024];
} Memory;

typedef enum {
  SPRITE_NULL,
  SPRITE_PLAYER_STANDING,
  SPRITE_NUM
} Sprite;

const char* sprite_files[SPRITE_NUM] = {
  "",
  "assets/player_standing.bmp",
};

static int isset(int flags, int flag) {
  return (flags & flag) != 0;
}

void render_sprite(Renderer* r, Sprite sprite, vec3 pos) {
  assert(r->num_sprites < (int) arrsize(r->sprites));
  r->sprites[r->num_sprites].x1 = pos.x;
  r->sprites[r->num_sprites].x2 = pos.x + r->bitmaps[sprite].w;
  r->sprites[r->num_sprites].y1 = pos.y;
  r->sprites[r->num_sprites].y2 = pos.y + r->bitmaps[sprite].w;
  r->sprites[r->num_sprites].u1 = 0.0;
  r->sprites[r->num_sprites].u2 = 1.0;
  r->sprites[r->num_sprites].v1 = 0.0;
  r->sprites[r->num_sprites].v2 = 1.0;
  ++r->num_sprites;
}

void init(void* mem, int mem_size) {
  Memory *m = mem;
  assert(mem_size >= (int)sizeof(Memory));
  memset(m, 0, sizeof(Memory));

  /* Init arena */
  {
    Arena* a = &m->arena;
    a->curr = m->arena_data;
    a->end = m->arena_data + sizeof(m->arena_data);
  }

  /* Init renderer */
  {
    Renderer* r = &m->renderer;
    /* Allocate sprite buffer */
    {
      int stride = sizeof(r->sprites[0]);
      glGenVertexArrays(1, &r->spritesVAO);
      glBindVertexArray(r->spritesVAO);
      glGenBuffers(1, &r->spritesVBO);
      glBindBuffer(GL_ARRAY_BUFFER, r->spritesVBO);
      glBufferData(GL_ARRAY_BUFFER, sizeof(r->sprites), 0, GL_DYNAMIC_DRAW);
      glOKORDIE;

      glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*) 0);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (GLvoid*) (4 * sizeof(GLfloat)));
      glEnableVertexAttribArray(1);

      glOKORDIE;
    }

    /* Load bitmaps */
    r->bitmaps[SPRITE_PLAYER_STANDING] = load_bitmap(&m->arena, sprite_files[SPRITE_PLAYER_STANDING]);

    /* Compile shaders */
    r->sprite_shader = compile_shader("assets/shaders/sprite_vertex.glsl", "assets/shaders/sprite_fragment.glsl");
  }

  /* Create player */
  {
    Entity e = {0};
    e.type = ENTITY_PLAYER;
    m->entities[m->num_entities++] = e;
  }
}

int main_loop(void* mem, long ms, Input input) {
  Memory* m = mem;
  float alpha = ms/128.0;
  Renderer* renderer = &m->renderer;
  Entity* e;

  glClearColor(sin(alpha)/2 + 0.5, sin(alpha*1.2)/2 + 0.5, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT);
  renderer->num_sprites = 0;
  
  for (e = m->entities; e < m->entities + m->num_entities; ++e) {
    switch (e->type) {
      case ENTITY_NULL:
        fprintf(stderr, "Unknown entity type found\n");
        abort();
        break;
      case ENTITY_PLAYER:
        #define PLAYER_SPEED 0.3
        e->pos.x += PLAYER_SPEED*isset(input.was_pressed, BUTTON_RIGHT);
        e->pos.x -= PLAYER_SPEED*isset(input.was_pressed, BUTTON_LEFT);
        e->pos.y += PLAYER_SPEED*isset(input.was_pressed, BUTTON_DOWN);
        e->pos.y -= PLAYER_SPEED*isset(input.was_pressed, BUTTON_UP);
        render_sprite(&m->renderer, SPRITE_PLAYER_STANDING, e->pos);
        break;
    }
  }

  return input.was_pressed & BUTTON_START;
}

