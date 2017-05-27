#include "flatsouls_platform.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include "flatsouls_logging.h"

#define arrcount(a) (sizeof(a)/sizeof(*a))
#define isset(flags, flag) (((flags) & (flag)) != 0)
void bitfield_set(unsigned char* arr, int i) {arr[i >> 3] |= 1 << i;}
void bitfield_clear(unsigned char* arr, int i) {arr[i >> 3] &= ~(1 << i);}

/* some static asserts */
typedef char assert_char_is_8[sizeof(char)==1?1:-1];
typedef char assert_short_is_16[sizeof(short)==2?1:-1];
typedef char assert_int_is_32[sizeof(int)==4?1:-1];
typedef char assert_long_is_64[sizeof(long)==8?1:-1];
typedef char assert_ptr_is_long[sizeof(long)==sizeof(void*)?1:-1];

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
static v2 v2_add(v2 v, float x, float y) {return v2_create(v.x+x, v.y+y);}
static v2 v2_mult(v2 v, float x) {return v2_create(v.x*x, v.y*x);}

static v2 v2i_to_v2(v2i v) {return v2_create(v.x, v.y);}

typedef enum {
  ENTITY_NULL,
  ENTITY_PLAYER
} EntityType;

static const char* entity_type_names[] = {
  "Null",
  "Player"
};

typedef struct {
  EntityType type;
  v3 pos;
} Entity;

typedef struct {
  GLuint id;
  v2i size;
} Texture;

typedef struct {
  v2 pos, tpos;
} SpriteVertex;

typedef struct {
  v3 camera_pos;
  /* sprites */
  GLuint
    sprites_vertex_array, sprite_vertex_buffer,
    sprite_shader, sprite_view_location;
  Texture sprite_atlas;
  SpriteVertex sprite_vertices[256];
  int num_sprites;

  /* text */
  #define RENDERER_FIRST_CHAR 32
  #define RENDERER_LAST_CHAR 128
  #define RENDERER_FONT_SIZE 32.0f
  GLuint text_vertex_array, text_vertex_buffer;
  SpriteVertex text_vertices[256];
  int num_text_vertices;
  Texture text_atlas;
  Glyph glyphs[RENDERER_LAST_CHAR - RENDERER_FIRST_CHAR];
} Renderer;

typedef struct {
  Entity entities[256];
  int num_entities;
  Renderer renderer;
  Arena arena;
  char arena_data[128*1024*1024];
} Memory;


static float calc_string_width(Renderer *r, const char *str, float height) {
  const char *c;
  float result = 0.0f;
  for (c = str; *c; ++c) {
    result += r->glyphs[*c - RENDERER_FIRST_CHAR].advance * height / RENDERER_FONT_SIZE;
  }
  return result;
}

static void render_text(Renderer *r, const char *str, v2 pos, float height, int center) {
  const char *c;
  float scale = height / RENDERER_FONT_SIZE;
  float ipw = 1.0f / r->text_atlas.size.x;
  float iph = 1.0f / r->text_atlas.size.y;

  if (r->num_text_vertices + strlen(str) >= arrcount(r->text_vertices))
    return;

  if (center) {
    pos.x -= calc_string_width(r, str, height);
    pos.y -= height/2.0f;
  }

  for (c = str; *c && r->num_text_vertices + 6 < (int)arrcount(r->text_vertices); ++c) {
    Glyph g = r->glyphs[*c - RENDERER_FIRST_CHAR];
    float x0 = pos.x + g.offset_x*scale;
    float y0 = pos.y - g.offset_y*scale;
    float x1 = pos.x + g.offset_x*scale + (g.s1-g.s0)*scale;
    float y1 = pos.y - g.offset_y*scale - (g.t1-g.t0)*scale;
    float s0 = (float) g.s0 * ipw;
    float s1 = (float) g.s1 * ipw;
    float t0 = (float) g.t0 * iph;
    float t1 = (float) g.t1 * iph;

    v4 a = v4_create(x0, y0, s0, t0);
    v4 b = v4_create(x1, y0, s1, t0);
    v4 c = v4_create(x0, y1, s0, t1);
    v4 d = v4_create(x1, y1, s1, t1);

    v4 *s = (v4*) r->text_vertices+r->num_text_vertices;

    *s++ = a;
    *s++ = b;
    *s++ = c;
    *s++ = c;
    *s++ = b;
    *s++ = d;
    r->num_text_vertices += 6;
    pos.x += g.advance*scale;
  }
}

static void render_sprite(Renderer *r, v3 p, v2 size) {
  /* TODO: get x and y positions of sprite in sprite sheet */
  v2 p2 = v3_xy(p);
  p2 = v2_add(p2, -size.x/2, -size.y/2);
  assert(r->sprite_atlas.id);
  assert(r->num_sprites + 6 < (int)arrcount(r->sprite_vertices));

  {
    v4 a = v4_create(p2.x, p2.y, 0, 1);
    v4 b = v4_create(p2.x+size.x, p2.y, 1, 1);
    v4 c = v4_create(p2.x, p2.y+size.y, 0, 0);
    v4 d = v4_create(p2.x+size.x, p2.y+size.y, 1, 0);
    v4 *p = (v4*) r->sprite_vertices+r->num_sprites;
    *p++ = a;
    *p++ = b;
    *p++ = c;
    *p++ = c;
    *p++ = b;
    *p++ = d;
    r->num_sprites += 6;
  }
}

static void render_clear(Renderer *r) {
  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glOKORDIE;
  r->num_sprites = 0;
  r->num_text_vertices = 0;
}

static LoadImageTextureFromFile load_image_texture_from_file;
static LoadFontFromFile load_font_from_file;

void init(void* mem, int mem_size, Funs dfuns) {
  Memory *m = mem;
  assert(mem_size >= (int)sizeof(Memory));
  memset(m, 0, sizeof(Memory));

  /* Init function pointers */
  load_image_texture_from_file = dfuns.load_image_texture_from_file;
  load_font_from_file = dfuns.load_font_from_file;

  /* Init arena */
  {
    Arena* a = &m->arena;
    a->curr = m->arena_data;
    a->end = m->arena_data + sizeof(m->arena_data);
  }

  /* Init renderer */
  {
    Renderer* r = &m->renderer;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Load images into textures */
    load_image_texture_from_file("assets/spritesheet.png", &r->sprite_atlas.id, &r->sprite_atlas.size.x, &r->sprite_atlas.size.y);
    glGenerateMipmap(GL_TEXTURE_2D);

    /* Create font texture and read in font from file */
    {
      const int w = 512, h = 512;
      glGenTextures(1, &r->text_atlas.id);
      glBindTexture(GL_TEXTURE_2D, r->text_atlas.id);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glOKORDIE;
      r->text_atlas.size.x = w;
      r->text_atlas.size.y = h;

      load_font_from_file("assets/Roboto-Regular.ttf", r->text_atlas.id, r->text_atlas.size.x, r->text_atlas.size.y, RENDERER_FIRST_CHAR, RENDERER_LAST_CHAR, RENDERER_FONT_SIZE, r->glyphs);
      glBindTexture(GL_TEXTURE_2D, r->text_atlas.id);
      glGenerateMipmap(GL_TEXTURE_2D);
    }

    /* Allocate sprite buffer */
    {
      glGenVertexArrays(1, &r->sprites_vertex_array);
      glBindVertexArray(r->sprites_vertex_array);
      glOKORDIE;
      glGenBuffers(1, &r->sprite_vertex_buffer);
      glBindBuffer(GL_ARRAY_BUFFER, r->sprite_vertex_buffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(r->sprite_vertices), 0, GL_DYNAMIC_DRAW);
      glOKORDIE;

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) 0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) (2*sizeof(float)));
    }

    /* Allocate text buffer */
    {
      glGenVertexArrays(1, &r->text_vertex_array);
      glBindVertexArray(r->text_vertex_array);
      glOKORDIE;
      glGenBuffers(1, &r->text_vertex_buffer);
      glBindBuffer(GL_ARRAY_BUFFER, r->text_vertex_buffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(r->text_vertices), 0, GL_DYNAMIC_DRAW);
      glOKORDIE;

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) 0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) (2*sizeof(float)));
    }

    /* Compile shaders */
    r->sprite_shader = compile_shader("assets/shaders/sprite_vertex.glsl", "assets/shaders/sprite_fragment.glsl");
    glOKORDIE;

    /* Get/set uniforms */
    glUseProgram(r->sprite_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas.id);
    glUniform1i(glGetUniformLocation(r->sprite_shader, "u_texture"), 0);
    r->sprite_view_location = glGetUniformLocation(r->sprite_shader, "u_view");
    glOKORDIE;

  }

  /* Create player */
  {
    Entity e = {0};
    e.type = ENTITY_PLAYER;
    e.pos = v3_create(-0.5, -0.5, 0.0);
    m->entities[m->num_entities++] = e;
    e.pos = v3_create(0.5, 0.5, 0.0);
    m->entities[m->num_entities++] = e;
  }
}

void print(const char* fmt, ...) {
  const char *c = fmt;
  va_list args;
  va_start(args, fmt);
  while (*c) {
    if (*c != '%') {putchar(*c++); continue;}
    ++c;
    switch (*c++) {
      case '%': {
        putchar('%');
        putchar('%');
      } break;
      case 'e': {
        Entity* e = va_arg(args, Entity*);
        printf("%s: x: %f y: %f z: %f\n", entity_type_names[e->type], e->pos.x, e->pos.y, e->pos.z);
      } break;
    }
  }
  va_end(args);
}

int main_loop(void* mem, long ms, Input input) {
  static long last_ms = 0;
  Memory* m = mem;
  Renderer* renderer = &m->renderer;
  float dt = (ms - last_ms) * 0.06;
  int i;
  last_ms = ms;
  printf("%li %f\n", ms ,dt);

  /* clear */
  render_clear(renderer);

  /* Update entities */
  for (i = 0; i < m->num_entities; ++i) {
    Entity *e = m->entities + i;
    switch (e->type) {
      case ENTITY_NULL:
        fprintf(stderr, "Unknown entity type found\n");
        abort();
        break;
      case ENTITY_PLAYER: {
        const float PLAYER_SPEED = 0.01;
        e->pos.x += dt*PLAYER_SPEED*input.is_down[BUTTON_RIGHT];
        e->pos.x -= dt*PLAYER_SPEED*input.is_down[BUTTON_LEFT];
        e->pos.y += dt*PLAYER_SPEED*input.is_down[BUTTON_UP];
        e->pos.y -= dt*PLAYER_SPEED*input.is_down[BUTTON_DOWN];
        render_sprite(&m->renderer, e->pos, v2_create(0.1, 0.1));
        render_text(&m->renderer, entity_type_names[e->type], v2_add(v3_xy(e->pos), 0.05f, 0.15f), 0.1f, 1);
        print("%e\n", e);
      } break;
    }
  }

  render_sprite(&m->renderer, v3_create(0.1, 0.1, 0), v2_create(0.1, 0.1));
  render_sprite(&m->renderer, v3_create(-0.3, 0.3, 0), v2_create(0.1, 0.1));
  render_sprite(&m->renderer, v3_create(0.3, -0.3, 0), v2_create(0.1, 0.1));
  render_sprite(&m->renderer, v3_create(-0.3, -0.3, 0), v2_create(0.1, 0.1));

  {
    int i;
    puts("********* Sprite Vertices *********");
    for (i = 0; i < renderer->num_sprites; ++i) {
      printf("%f %f\n", renderer->sprite_vertices[i].pos.x, renderer->sprite_vertices[i].pos.y);
    }
    puts("********* Sprite Vertices *********");
    puts("********* Text Vertices *********");
    for (i = 0; i < renderer->num_text_vertices; ++i) {
      printf("%f %f\n", renderer->text_vertices[i].pos.x, renderer->text_vertices[i].pos.y);
    }
    puts("********* Text Vertices *********");
  }
  glUseProgram(renderer->sprite_shader);

  /* draw sprites */
  glBindVertexArray(renderer->sprites_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->sprite_vertex_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_sprites*sizeof(*renderer->sprite_vertices), renderer->sprite_vertices);
  glBindTexture(GL_TEXTURE_2D, renderer->sprite_atlas.id);
  glDrawArrays(GL_TRIANGLES, 0, renderer->num_sprites);
  printf("num_sprites: %i\n", renderer->num_sprites);
  glOKORDIE;

  /* draw text */
  glBindVertexArray(renderer->text_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->text_vertex_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_text_vertices*sizeof(*renderer->text_vertices), renderer->text_vertices);
  glBindTexture(GL_TEXTURE_2D, renderer->text_atlas.id);
  glDrawArrays(GL_TRIANGLES, 0, renderer->num_text_vertices);
  printf("num_text_vertices: %i\n", renderer->num_text_vertices);
  glOKORDIE;

  return input.was_pressed[BUTTON_START];
}
