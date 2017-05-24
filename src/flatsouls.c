#include "flatsouls_platform.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include "flatsouls_logging.h"

#define arrcount(a) (sizeof(a)/sizeof(*a))

typedef struct {
  char *curr, *end;
  char* mem;
} Arena;

typedef struct {
  char* data;
  int w,h;
} Bitmap;

#define arena_push(arena, type) arena_push_block(arena, sizeof(type))
static void* arena_push_block(Arena *a, int size) {
  assert(a->curr + size <= a->end);
  a->curr += size;
  return a->curr - size;
}

static GLuint compile_shader(const char* vertex_filename, const char* fragment_filename) {
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
  s = shader_src; s += fread(s, 1, sizeof(shader_src), vertex_file); *s++ = 0;
  if (!feof(vertex_file)) LOG_AND_ABORT(LOG_ERROR, "File larger than buffer\n");
  else if (ferror(vertex_file)) LOG_AND_ABORT(LOG_ERROR, "While reading vertex shader %s: %s\n", vertex_filename, strerror(errno));

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &shader_src_list, 0);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, sizeof(info_log), 0, info_log);
    LOG_AND_ABORT(LOG_ERROR, "Could not compile vertex shader: %s\n", info_log);
  }

  fragment_file = fopen(fragment_filename, "r");
  if (!fragment_file) LOG_AND_ABORT(LOG_ERROR, "Could not open fragment shader file: %s\n");
  s = shader_src; s += fread(s, 1, sizeof(shader_src), fragment_file); *s++ = 0;
  if (!feof(fragment_file)) LOG_AND_ABORT(LOG_ERROR, "File larger than buffer\n");
  else if (ferror(fragment_file)) LOG_AND_ABORT(LOG_ERROR, "While reading fragment shader %s: %s\n", fragment_filename, strerror(errno));

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &shader_src_list, 0);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, sizeof(info_log), 0, info_log);
    LOG_AND_ABORT(LOG_ERROR, "Could not compile fragment shader: %s\n", info_log);
  }

  result = glCreateProgram();
  glAttachShader(result, vertex_shader);
  glAttachShader(result, fragment_shader);
  glLinkProgram(result);
  glGetProgramiv(result, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(result, 512, 0, info_log);
    LOG_AND_ABORT(LOG_ERROR, "Could not link shader: %s\n", info_log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  fclose(vertex_file);
  fclose(fragment_file);
  glOKORDIE;
  return result;
}

typedef struct {
  float x,y,z,w;
} v4;

typedef struct {
  float x,y,z;
} v3;

typedef struct {
  float x,y;
} v2;

typedef struct {
  int x,y;
} v2i;

static v2 v2_create(float x, float y) {v2 r; r.x = x; r.y = y; return r;}
static v3 v3_create(float x, float y, float z) {v3 r; r.x=x; r.y=y; r.z=z; return r;}
static v4 v4_create(float x, float y, float z, float w) {v4 r; r.x=x; r.y=y; r.z=z; r.w=w; return r;}
static v2 v3_xy(v3 v) {return v2_create(v.x, v.y);}

static v2 v2i_to_v2(v2i v) {return v2_create(v.x, v.y);}

typedef enum {
  ENTITY_NULL,
  ENTITY_PLAYER
} EntityType;

typedef struct {
  EntityType type;
  v3 pos;
} Entity;

typedef struct {
  GLuint id;
  v2i size;
} Texture;

typedef struct {
  v3 camera_pos;
  /* sprites */
  GLuint
    sprites_vertex_array, sprites_vertex_buffer,
    sprite_element_buffer,
    sprite_shader, sprite_view_location;
  Texture sprite_sheet;
  v2 sprites[256];
  int num_sprites;
} Renderer;

typedef struct {
  Entity entities[256];
  int num_entities;
  Renderer renderer;
  Arena arena;
  char arena_data[128*1024*1024];
} Memory;

#define isset(flags, flag) (((flags) & (flag)) != 0)

static void render_sprite(Renderer* r, v3 pos, v2 size) {
  /* TODO: get x and y positions of sprite in sprite sheet */
  assert(r->sprite_sheet.id);
  assert(r->num_sprites + 4 < (int) arrcount(r->sprites));
  r->sprites[r->num_sprites++] = v2_create(pos.x, pos.y);
  r->sprites[r->num_sprites++] = v2_create(pos.x + size.x, pos.y);
  r->sprites[r->num_sprites++] = v2_create(pos.x, pos.y + size.y);
  r->sprites[r->num_sprites++] = v2_create(pos.x + size.x, pos.y + size.y);
}

static LoadTextureFromFile load_texture_from_file;

void init(void* mem, int mem_size, Funs dfuns) {
  Memory *m = mem;
  assert(mem_size >= (int)sizeof(Memory));
  memset(m, 0, sizeof(Memory));

  /* Init function pointers */
  load_texture_from_file = dfuns.load_texture_from_file;

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
      const GLshort elements[] = {0, 1, 2, 2, 1, 3};
      glGenVertexArrays(1, &r->sprites_vertex_array);
      glBindVertexArray(r->sprites_vertex_array);
      glOKORDIE;
      glGenBuffers(1, &r->sprites_vertex_buffer);
      glBindBuffer(GL_ARRAY_BUFFER, r->sprites_vertex_buffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(r->sprites), 0, GL_DYNAMIC_DRAW);
      glOKORDIE;

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*) 0);
      glOKORDIE;

      glGenBuffers(1, &r->sprite_element_buffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->sprite_element_buffer);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);
      glOKORDIE;
    }

    /* Load images into textures */
    load_texture_from_file("assets/spritesheet.png", &r->sprite_sheet.id, &r->sprite_sheet.size.x, &r->sprite_sheet.size.y);

    /* Compile shaders */
    r->sprite_shader = compile_shader("assets/shaders/sprite_vertex.glsl", "assets/shaders/sprite_fragment.glsl");
    glOKORDIE;

    /* Get/set uniforms */
    #if 0
    {
      glUseProgram(r->sprite_shader);
      glOKORDIE;
      glActiveTexture(GL_TEXTURE0);
      glOKORDIE;
      glBindTexture(GL_TEXTURE_2D, r->sprite_sheet.id);
      glOKORDIE;
      glUniform1i(glGetUniformLocation(r->sprite_shader, "u_texture"), 0);
      glOKORDIE;
      r->sprite_view_location = glGetUniformLocation(r->sprite_shader, "u_view");
      glOKORDIE;
    }
    #endif
  }

  /* Create player */
  {
    Entity e = {0};
    e.type = ENTITY_PLAYER;
    e.pos = v3_create(-0.5, -0.5, 0.0);
    m->entities[m->num_entities++] = e;
    e.pos = v3_create(-0.5, 0.1, 0.0);
    m->entities[m->num_entities++] = e;
  }
}

int main_loop(void* mem, long ms, Input input) {
  Memory* m = mem;
  Renderer* renderer = &m->renderer;

  /* clear */
  glClearColor(0.5 + 0.5*sin(ms/200.0), 1.0, 0.5 + 0.5*sin(ms/200.0), 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glOKORDIE;
  renderer->num_sprites = 0;

  /* Update entities */
  {
    Entity* e;
    for (e = m->entities; e < m->entities + m->num_entities; ++e) {
      switch (e->type) {
        case ENTITY_NULL:
          fprintf(stderr, "Unknown entity type found\n");
          abort();
          break;
        case ENTITY_PLAYER:
          #define PLAYER_SPEED 0.01
          e->pos.x += PLAYER_SPEED*input.is_down[BUTTON_RIGHT];
          e->pos.x -= PLAYER_SPEED*input.is_down[BUTTON_LEFT];
          e->pos.y += PLAYER_SPEED*input.is_down[BUTTON_UP];
          e->pos.y -= PLAYER_SPEED*input.is_down[BUTTON_DOWN];
          render_sprite(&m->renderer, e->pos, v2_create(0.1, 0.1));
          break;
      }
    }
  }

  render_sprite(&m->renderer, v3_create(0.1, 0.1, 0), v2_create(0.1, 0.1));
  render_sprite(&m->renderer, v3_create(-0.3, 0.3, 0), v2_create(0.1, 0.1));
  render_sprite(&m->renderer, v3_create(0.3, -0.3, 0), v2_create(0.1, 0.1));
  render_sprite(&m->renderer, v3_create(-0.3, -0.3, 0), v2_create(0.1, 0.1));

  /* draw sprites */
  fprintf(stderr, "%i\n", renderer->num_sprites);
  glUseProgram(renderer->sprite_shader);
  glBindVertexArray(renderer->sprites_vertex_array);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_sprites*sizeof(renderer->sprites[0]), renderer->sprites);
  glDrawElements(GL_TRIANGLES, 6*renderer->num_sprites, GL_UNSIGNED_SHORT, 0);
  glOKORDIE;

  return input.was_pressed[BUTTON_START];
}
