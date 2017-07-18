#include "flatsouls_platform.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>
#include "flatsouls_logging.h"

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

/* @math */
int equalf(float a, float b) {return abs(a-b) < 0.00001;}

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
static int v2_equal(v2 a, v2 b) {return equalf(a.x, b.x) && equalf(a.y, b.y);}
static v2 v2_mult(v2 v, float x) {return v2_create(v.x*x, v.y*x);}
static v3 v3_add(v3 v, float x, float y, float z) {return v3_create(v.x+x, v.y+y, v.z+z);}

static v2 v2i_to_v2(v2i v) {return v2_create(v.x, v.y);}

typedef enum {
  ENTITY_TYPE_NULL,
  ENTITY_TYPE_PLAYER,
  ENTITY_TYPE_MONSTER,
  ENTITY_TYPE_DERPER,
  ENTITY_TYPE_COUNT
} EntityType;

static const char* entity_type_names[] = {
  "Null",
  "Player",
  "Monster",
  "Derper"
};
STATIC_ASSERT(ARRAY_LEN(entity_type_names) == ENTITY_TYPE_COUNT, all_entity_names_entered);

typedef struct {
  EntityType type;
  v3 pos;

  /* Monster stuff */
  v2 target;
} Entity;

typedef struct {
  GLuint id;
  v2i size;
} Texture;

typedef struct {
  v3 pos;
  v2 tpos;
} SpriteVertex;

SpriteVertex spritevertex_create(v3 pos, v2 tpos) {SpriteVertex result; result.pos = pos; result.tpos = tpos; return result;}
typedef SpriteVertex TextVertex;

typedef struct {
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
  TextVertex text_vertices[256];
  int num_text_vertices;
  Texture text_atlas;
  Glyph glyphs[RENDERER_LAST_CHAR - RENDERER_FIRST_CHAR];

  /* camera */
  #define RENDERER_CAMERA_HEIGHT 2
  v3 camera_pos;
} Renderer;

typedef struct {
  Entity entities[256];
  int num_entities;
  Renderer renderer;
  Arena arena;
  char arena_data[128*1024*1024];
} Memory;

static Glyph glyph_get(Renderer *r, char c) {
  return r->glyphs[c - RENDERER_FIRST_CHAR];
}

static float calc_string_width(Renderer *r, const char *str) {
  float result = 0.0f;

  for (; *str; ++str)
    result += glyph_get(r, *str).advance;
  return result;
}

static void render_text(Renderer *r, const char *str, v3 pos, float height, int center) {
  Glyph g;
  float dx,dy, scale, ipw,iph;
  v3 p, a,b,c,d;
  v2 ta,tb,tc,td;
  SpriteVertex *v;

  scale = height / RENDERER_FONT_SIZE;
  ipw = 1.0f / r->text_atlas.size.x;
  iph = 1.0f / r->text_atlas.size.y;

  if (r->num_text_vertices + strlen(str) >= ARRAY_LEN(r->text_vertices)) return;

  if (center) {
    pos.x -= calc_string_width(r, str) * scale / 2;
    /*pos.y -= height/2.0f;*/ /* Why isn't this working? */
  }

  for (; *str && r->num_text_vertices + 6 < (int)ARRAY_LEN(r->text_vertices); ++str) {
    g = glyph_get(r, *str);
    p = v3_add(pos, g.offset_x*scale, -g.offset_y*scale, 0);
    dx = (g.s1-g.s0)*scale;
    dy = -(g.t1-g.t0)*scale;
    a = p;
    b = v3_add(p, dx, 0, 0);
    c = v3_add(p, 0, dy, 0);
    d = v3_add(p, dx, dy, 0);
    ta = v2_create(g.s0 * ipw, g.t0 * iph);
    tb = v2_create(g.s1 * ipw, g.t0 * iph);
    tc = v2_create(g.s0 * ipw, g.t1 * iph);
    td = v2_create(g.s1 * ipw, g.t1 * iph);
    v = r->text_vertices + r->num_text_vertices;

    *v++ = spritevertex_create(a, ta);
    *v++ = spritevertex_create(b, tb);
    *v++ = spritevertex_create(c, tc);
    *v++ = spritevertex_create(c, tc);
    *v++ = spritevertex_create(b, tb);
    *v++ = spritevertex_create(d, td);

    r->num_text_vertices += 6;
    pos.x += g.advance*scale;
  }
}

static void render_sprite(Renderer *r, v3 p, v2 size, int center) {
  /* TODO: get x and y positions of sprite in sprite sheet */
  v3 a,b,c,d;
  v2 ta,tb,tc,td;
  SpriteVertex *v;

  if (center) p = v3_add(p, -size.x/2, -size.y/2, 0.0f);
  assert(r->sprite_atlas.id);
  assert(r->num_sprites + 6 < (int)ARRAY_LEN(r->sprite_vertices));

  a = p;
  b = v3_add(p, size.x, 0,      0);
  c = v3_add(p, 0,      size.y, 0);
  d = v3_add(p, size.x, size.y, 0);
  ta = v2_create(0, 1);
  tb = v2_create(1, 1);
  tc = v2_create(0, 0);
  td = v2_create(1, 0);
  v = r->sprite_vertices + r->num_sprites;
  *v++ = spritevertex_create(a, ta);
  *v++ = spritevertex_create(b, tb);
  *v++ = spritevertex_create(c, tc);
  *v++ = spritevertex_create(c, tc);
  *v++ = spritevertex_create(b, tb);
  *v++ = spritevertex_create(d, td);
  r->num_sprites += 6;
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
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) 0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) offsetof(SpriteVertex, tpos));
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
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) 0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) offsetof(SpriteVertex, tpos));
    }

    /* Compile shaders */
    r->sprite_shader = compile_shader("assets/shaders/sprite_vertex.glsl", "assets/shaders/sprite_fragment.glsl");
    glOKORDIE;

    /* Get/set uniforms */
    glUseProgram(r->sprite_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas.id);
    glUniform1i(glGetUniformLocation(r->sprite_shader, "u_texture"), 0);
    r->sprite_view_location = glGetUniformLocation(r->sprite_shader, "camera");
    glOKORDIE;

  }

  /* Create player */
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_PLAYER;
    e.pos = v3_create(0, 0, 0);
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
      case 'i': {
        int i = va_arg(args, int);
        printf("%i", i);
      }
      case 'f': {
        double f = va_arg(args, double);
        printf("%f", f);
      }
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

  /* clear */
  render_clear(renderer);

  /* Update entities */
  for (i = 0; i < m->num_entities; ++i) {
    Entity *e = m->entities + i;
    switch (e->type) {
      case ENTITY_TYPE_COUNT:
      case ENTITY_TYPE_NULL:
        fprintf(stderr, "Unknown entity type found\n");
        abort();
        break;
      case ENTITY_TYPE_PLAYER: {
        const float PLAYER_SPEED = 0.01;
        e->pos.x += dt*PLAYER_SPEED*input.is_down[BUTTON_RIGHT];
        e->pos.x -= dt*PLAYER_SPEED*input.is_down[BUTTON_LEFT];
        e->pos.y += dt*PLAYER_SPEED*input.is_down[BUTTON_UP];
        e->pos.y -= dt*PLAYER_SPEED*input.is_down[BUTTON_DOWN];
        render_sprite(&m->renderer, e->pos, v2_create(1, 1), 1);
        render_text(&m->renderer, entity_type_names[e->type], e->pos, 0.1f, 1);
        m->renderer.camera_pos = v3_add(e->pos, 0, 0, RENDERER_CAMERA_HEIGHT);
      } break;
      case ENTITY_TYPE_MONSTER: break;
      case ENTITY_TYPE_DERPER: break;
    }
  }

  render_sprite(&m->renderer, v3_create(0.1, 0.1, -1), v2_create(1, 1), 1);
  render_sprite(&m->renderer, v3_create(-0.3, 0.3, -1), v2_create(1, 1), 1);
  render_sprite(&m->renderer, v3_create(0.3, -0.3, -1), v2_create(1, 1), 1);
  render_sprite(&m->renderer, v3_create(-0.3, -0.3, -1), v2_create(1, 1), 1);

  #if 1
  {
    int i;
    puts("********* Sprite Vertices *********");
    for (i = 0; i < renderer->num_sprites; ++i) {
      printf("%f %f %f\n", renderer->sprite_vertices[i].pos.x, renderer->sprite_vertices[i].pos.y, renderer->sprite_vertices[i].pos.z);
    }
    puts("********* Sprite Vertices *********");
    puts("********* Text Vertices *********");
    for (i = 0; i < renderer->num_text_vertices; ++i) {
      printf("%f %f %f\n", renderer->text_vertices[i].pos.x, renderer->text_vertices[i].pos.y, renderer->text_vertices[i].pos.z);
    }
    puts("********* Text Vertices *********");
  }
  #endif
  glUseProgram(renderer->sprite_shader);

  glUniform3f(renderer->sprite_view_location, renderer->camera_pos.x, renderer->camera_pos.y, renderer->camera_pos.z);
  glOKORDIE;

  /* draw sprites */
  glBindVertexArray(renderer->sprites_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->sprite_vertex_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_sprites*sizeof(*renderer->sprite_vertices), renderer->sprite_vertices);
  glBindTexture(GL_TEXTURE_2D, renderer->sprite_atlas.id);
  glDrawArrays(GL_TRIANGLES, 0, renderer->num_sprites);
  glOKORDIE;

  /* draw text */
  glBindVertexArray(renderer->text_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->text_vertex_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_text_vertices*sizeof(*renderer->text_vertices), renderer->text_vertices);
  glBindTexture(GL_TEXTURE_2D, renderer->text_atlas.id);
  glDrawArrays(GL_TRIANGLES, 0, renderer->num_text_vertices);
  glOKORDIE;

  return input.was_pressed[BUTTON_START];
}
