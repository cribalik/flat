#include "flatsouls_utils.h"
#include "flatsouls_math.h"

#define MEM_IMPLEMENTATION
#include "acorns/mem.h"
#undef MEM_IMPLEMENTATION

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <math.h>

typedef struct {
  char* data;
  int w,h;
} Bitmap;

static void print(const char* fmt, ...);

static GLuint compile_shader(const char* vertex_filename, const char* fragment_filename) {
  GLuint result = 0;
  FILE *vertex_file = 0, *fragment_file = 0;
  char shader_src[2048];
  const char * const shader_src_list = shader_src;
  char info_log[512];
  int num_read;
  GLint success;
  GLuint vertex_shader, fragment_shader;

  vertex_file = fopen(vertex_filename, "r");
  if (!vertex_file) die("Could not open vertex shader file %s: %s\n", vertex_filename, strerror(errno));
  num_read = fread(shader_src, 1, sizeof(shader_src), vertex_file);
  shader_src[num_read] = 0;
  if (!feof(vertex_file)) die("File larger than buffer\n");
  else if (ferror(vertex_file)) die("While reading vertex shader %s: %s\n", vertex_filename, strerror(errno));

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &shader_src_list, 0);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, sizeof(info_log), 0, info_log);
    die("Could not compile vertex shader: %s\n", info_log);
  }

  fragment_file = fopen(fragment_filename, "r");
  if (!fragment_file) die("Could not open fragment shader file %s: %s\n", fragment_filename, strerror(errno));
  num_read = fread(shader_src, 1, sizeof(shader_src), fragment_file);
  shader_src[num_read] = 0;
  if (!feof(fragment_file)) die("File larger than buffer\n");
  else if (ferror(fragment_file)) die("While reading fragment shader %s: %s\n", fragment_filename, strerror(errno));

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &shader_src_list, 0);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, sizeof(info_log), 0, info_log);
    die("Could not compile fragment shader: %s\n", info_log);
  }

  result = glCreateProgram();
  glAttachShader(result, vertex_shader);
  glAttachShader(result, fragment_shader);
  glLinkProgram(result);
  glGetProgramiv(result, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(result, 512, 0, info_log);
    die("Could not link shader: %s\n", info_log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  fclose(vertex_file);
  fclose(fragment_file);
  gl_ok_or_die;
  return result;
}

typedef enum {
  ENTITY_TYPE_NULL,
  ENTITY_TYPE_PLAYER,
  ENTITY_TYPE_WALL,
  ENTITY_TYPE_MONSTER,
  ENTITY_TYPE_DERPER,
  ENTITY_TYPE_THING,
  ENTITY_TYPE_COUNT
} EntityType;

static const char* entity_type_names[] = {
  "Null",
  "Player",
  "Wall",
  "Monster",
  "Derper",
  "Thing"
};
STATIC_ASSERT(ARRAY_LEN(entity_type_names) == ENTITY_TYPE_COUNT, all_entity_names_entered);

typedef struct {
  EntityType type;
  v3 pos;
  v3 vel;

  /* physics */
  Rect hitbox;

  /* animation */
  unsigned int animation_time;

  /* Monster stuff */
  v2 target;
} Entity;

static float physics_collide_ray(Line a, Line b) {
  float rx = a.x1 - a.x0;
  float ry = a.y1 - a.y0;
  float sx = b.x1 - b.x0;
  float sy = b.y1 - b.y0;
  float d = rx*sy - ry*sx;
  float cx = b.x0 - a.x0;
  float cy = b.y0 - a.y0;

  if (fabs(d) >= 0.001f) {
    float t = (cx*sy - cy*sx)/d;
    float u = (cx*ry - cy*rx)/d;
    if (t >= 0 && t <= 1 && u > 0 && u <= 1) {
      print("%l %l %f %f %f\n", &a, &b, t, u, d);
      return at_leastf(t - 0.001f, 0.0f);
    }
  }
  return -1.0f;
}

static float physics_collide_line_rect(Line l, Rect r) {
  float f;
  Line b;

  /* top */
  b.x0 = r.x0;
  b.x1 = r.x1;
  b.y0 = b.y1 = r.y1;
  if ((f = physics_collide_ray(l, b)) != -1.0f)
    return f;

  /* bottom */
  b.x0 = r.x0;
  b.x1 = r.x1;
  b.y0 = b.y1 = r.y0;
  if ((f = physics_collide_ray(l, b)) != -1.0f)
    return f;

  /* left */
  b.x0 = b.x1 = r.x0;
  b.y0 = r.y0;
  b.y1 = r.y1;
  if ((f = physics_collide_ray(l, b)) != -1.0f)
    return f;

  /* right */
  b.x0 = b.x1 = r.x1;
  b.y0 = r.y0;
  b.y1 = r.y1;
  if ((f = physics_collide_ray(l, b)) != -1.0f)
    return f;

  return -1.0f;
}

static int physics_rect_collide(Rect a, Rect b) {
  return !(a.x1 < b.x0 || a.x0 > b.x1 || a.y0 > b.y1 || a.y1 < b.y1);
}

static Entity* physics_find_collision(Entity *e, float dt, Entity *entities, int num_entities, float *t_out) {
  int i;
  Line l;
  Rect r;

  r = rect_offset(e->hitbox, v3_xy(e->pos));
  rect_mid(r, &l.x0, &l.y0);
  l.x1 = l.x0 + (e->vel.x * dt);
  l.y1 = l.y0 + (e->vel.y * dt);

  for (i = 0; i < num_entities; ++i) {
    Entity *other;
    float t;
    Rect expanded_hitbox;

    other = entities + i;

    if (other == e)
      continue;

    expanded_hitbox = rect_expand(other->hitbox, r);
    expanded_hitbox = rect_offset(expanded_hitbox, v3_xy(other->pos));
    t = physics_collide_line_rect(l, expanded_hitbox);
    if (t == -1.0f)
      continue;

    *t_out = t;
    return other;
  }

  return 0;
}

static void physics_vel_update(Entity *e, float dt) {
  if (fabs(e->vel.x * dt) > 0.0001f)
    e->pos.x += e->vel.x * dt;
  if (fabs(e->vel.y * dt) > 0.0001f)
    e->pos.y += e->vel.y * dt;
}

typedef struct {
  GLuint id;
  v2i size;
} Texture;

typedef struct {
  v3 pos;
  v2 tpos;
} SpriteVertex;

static SpriteVertex spritevertex_create(v3 pos, v2 tpos) {
  SpriteVertex result;
  result.pos = pos;
  result.tpos = tpos;
  return result;
}
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
  Stack stack;
  char stack_data[128*1024*1024];
} Memory;

static int entity_push(Memory *mem, Entity e) {
  if (mem->num_entities >= ARRAY_LEN(mem->entities))
    return 1;

  mem->entities[mem->num_entities++] = e;
  return 0;
}

static Glyph glyph_get(Renderer *r, char c) {
  return r->glyphs[c - RENDERER_FIRST_CHAR];
}

typedef enum {
  ANIMATION_STATE_NULL,
  ANIMATION_STATE_PLAYER,
  ANIMATION_STATE_COUNT
} AnimationState;

static Rect tex_pos[] = {
  {0, 0, 0.1, 0.1}
};

static Rect get_tex_pos(AnimationState which, unsigned int time) {
  (void)time;
  ENUM_CHECK(ANIMATION_STATE, which);
  return tex_pos[which];
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

static void render_sprite(Renderer *r, v3 pos, v2 size, Rect tex, int center) {
  /* TODO: get x and y positions of sprite in sprite sheet */
  v3 a,b,c,d;
  v2 ta,tb,tc,td;
  SpriteVertex *v;

  if (center) pos = v3_add(pos, -size.x/2, -size.y/2, 0.0f);
  assert(r->sprite_atlas.id);
  assert(r->num_sprites + 6 < (int)ARRAY_LEN(r->sprite_vertices));

  a = pos;
  b = v3_add(pos, size.x, 0,      0);
  c = v3_add(pos, 0,      size.y, 0);
  d = v3_add(pos, size.x, size.y, 0);
  ta = rect_min(tex);
  tb = v2_create(tex.x1, tex.y0);
  tc = v2_create(tex.x0, tex.y1);
  td = rect_max(tex);
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
  gl_ok_or_die;
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

  /* Init stack */
  stack_init(&m->stack, m->stack_data, sizeof(m->stack_data));

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
      gl_ok_or_die;
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
      gl_ok_or_die;
      glGenBuffers(1, &r->sprite_vertex_buffer);
      glBindBuffer(GL_ARRAY_BUFFER, r->sprite_vertex_buffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(r->sprite_vertices), 0, GL_DYNAMIC_DRAW);
      gl_ok_or_die;

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) 0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) offsetof(SpriteVertex, tpos));
    }

    /* Allocate text buffer */
    {
      glGenVertexArrays(1, &r->text_vertex_array);
      glBindVertexArray(r->text_vertex_array);
      gl_ok_or_die;
      glGenBuffers(1, &r->text_vertex_buffer);
      glBindBuffer(GL_ARRAY_BUFFER, r->text_vertex_buffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(r->text_vertices), 0, GL_DYNAMIC_DRAW);
      gl_ok_or_die;

      glEnableVertexAttribArray(0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) 0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) offsetof(SpriteVertex, tpos));
    }

    /* Compile shaders */
    r->sprite_shader = compile_shader("assets/shaders/sprite_vertex.glsl", "assets/shaders/sprite_fragment.glsl");
    gl_ok_or_die;

    /* Get/set uniforms */
    glUseProgram(r->sprite_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas.id);
    glUniform1i(glGetUniformLocation(r->sprite_shader, "u_texture"), 0);
    r->sprite_view_location = glGetUniformLocation(r->sprite_shader, "camera");
    gl_ok_or_die;
  }

  /* Create player */
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_PLAYER;
    e.pos = v3_create(0, 0, 0);
    e.vel = v3_create(0, 0, 0);
    e.hitbox = rect_create(-0.5, 0.5, -0.5, 0.5);
    entity_push(m, e);
  }

  /* Create walls */
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_WALL;
    e.pos = v3_create(0, -10, 0);
    e.hitbox = rect_create(-10, 10, -1, 1);
    entity_push(m, e);
  }
}

static void print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  while (*fmt) {
    if (*fmt != '%') {putchar(*fmt++); continue;}
    ++fmt;
    switch (*fmt++) {
      case '%': {
        putchar('%');
        putchar('%');
      } break;

      case 'v': {
        switch (*fmt++) {
          case '2': {
            v2 *v = va_arg(args, v2*);
            printf("(%f,%f)", v->x, v->y);
          } break;
          case '3': {
            v3 *v = va_arg(args, v3*);
            printf("(%f,%f,%f)", v->x, v->y, v->z);
          } break;
        }
      } break;

      case 'e': {
        Entity* e = va_arg(args, Entity*);
        print("%s: pos: %v3 hitbox: %r", entity_type_names[e->type], &e->pos, &e->hitbox);
      } break;

      case 's': {
        const char *s = va_arg(args, char*);
        printf("%s", s);
      } break;

      case 'r': {
        Rect *r = va_arg(args, Rect*);
        printf("(%f,%f,%f,%f)", r->x0, r->x1, r->y0, r->y1);
      } break;

      case 'i': {
        int i = va_arg(args, int);
        printf("%i", i);
      } break;

      case 'f': {
        double f = va_arg(args, double);
        printf("%f", f);
      } break;

      case 'l': {
        Line *l = va_arg(args, Line*);
        printf("(%f,%f) -> (%f,%f)", l->x0, l->y0, l->x1, l->y1);
      } break;
    }
  }
  va_end(args);
}

int main_loop(void* mem, long ms, Input input) {
  static long last_ms;
  Memory* m;
  Renderer* renderer;
  float dt;
  int i;

  m = (Memory*)mem;
  renderer = &m->renderer;
  dt = (ms - last_ms) / 1000.0f;
  dt = dt > 0.05f ? 0.05f : dt;

  last_ms = ms;

  /* clear */
  render_clear(renderer);

  /* Update entities */
  for (i = 0; i < m->num_entities; ++i) {
    Entity *e = m->entities + i;

    ENUM_CHECK(ENTITY_TYPE, e->type);

    switch (e->type) {
      case ENTITY_TYPE_NULL:

      case ENTITY_TYPE_COUNT:
        break;

      case ENTITY_TYPE_PLAYER: {
        const float PLAYER_ACC = 3;
        Entity *target;
        float t;

        e->vel.x += dt*PLAYER_ACC*input.is_down[BUTTON_RIGHT];
        e->vel.x -= dt*PLAYER_ACC*input.is_down[BUTTON_LEFT];
        e->vel.y += dt*PLAYER_ACC*input.is_down[BUTTON_UP];
        e->vel.y -= dt*PLAYER_ACC*input.is_down[BUTTON_DOWN];

        e->animation_time += dt;

        /*e->vel.y -= dt * 10.0f;*/
        
        for (target = m->entities;; ++target) {
          target = physics_find_collision(e, dt, target, m->num_entities, &t);
          if (!target)
            break;

          print("collision: %f with: %e\n", t, target);
          if (target->type == ENTITY_TYPE_WALL) {
            e->vel.x *= t;
            e->vel.y *= t;
          }
        }

        physics_vel_update(e, dt);

        render_sprite(&m->renderer, e->pos, v2_create(1, 1), get_tex_pos(ANIMATION_STATE_PLAYER, e->animation_time), 1);
        render_text(&m->renderer, entity_type_names[e->type], e->pos, 0.1f, 1);
        m->renderer.camera_pos = v3_add(e->pos, 0, 0, RENDERER_CAMERA_HEIGHT);
      } break;

      case ENTITY_TYPE_WALL:
        render_sprite(&m->renderer, e->pos, rect_size(e->hitbox), get_tex_pos(ANIMATION_STATE_PLAYER, e->animation_time), 1);
        break;

      case ENTITY_TYPE_MONSTER:
        break;

      case ENTITY_TYPE_DERPER:
        break;

      case ENTITY_TYPE_THING:
        break;
    }
  }

  {
    static unsigned int t;
    t += dt;
    render_sprite(&m->renderer, v3_create(0.1, 0.1, 0), v2_create(1, 1), get_tex_pos(ANIMATION_STATE_PLAYER, t), 1);
    render_sprite(&m->renderer, v3_create(-0.3, 0.3, 0), v2_create(1, 1), get_tex_pos(ANIMATION_STATE_PLAYER, t), 1);
    render_sprite(&m->renderer, v3_create(0.3, -0.3, 0), v2_create(1, 1), get_tex_pos(ANIMATION_STATE_PLAYER, t), 1);
    render_sprite(&m->renderer, v3_create(-0.3, -0.3, 0), v2_create(1, 1), get_tex_pos(ANIMATION_STATE_PLAYER, t), 1);
  }

  #if 0
    puts("********* Entities *********");
    for (i = 0; i < m->num_entities; ++i)
      print("%e\n", &m->entities[i]);

    puts("********* Sprite Vertices *********");
    for (i = 0; i < renderer->num_sprites; ++i)
      printf("%f %f %f\n", renderer->sprite_vertices[i].pos.x, renderer->sprite_vertices[i].pos.y, renderer->sprite_vertices[i].pos.z);

    puts("********* Text Vertices *********");
    for (i = 0; i < renderer->num_text_vertices; ++i)
      printf("%f %f %f\n", renderer->text_vertices[i].pos.x, renderer->text_vertices[i].pos.y, renderer->text_vertices[i].pos.z);
  #endif

  glUseProgram(renderer->sprite_shader);

  glUniform3f(renderer->sprite_view_location, renderer->camera_pos.x, renderer->camera_pos.y, renderer->camera_pos.z);
  gl_ok_or_die;

  /* draw sprites */
  glBindVertexArray(renderer->sprites_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->sprite_vertex_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_sprites*sizeof(*renderer->sprite_vertices), renderer->sprite_vertices);
  glBindTexture(GL_TEXTURE_2D, renderer->sprite_atlas.id);
  glDrawArrays(GL_TRIANGLES, 0, renderer->num_sprites);
  gl_ok_or_die;

  /* draw text */
  glBindVertexArray(renderer->text_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->text_vertex_buffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_text_vertices*sizeof(*renderer->text_vertices), renderer->text_vertices);
  glBindTexture(GL_TEXTURE_2D, renderer->text_atlas.id);
  glDrawArrays(GL_TRIANGLES, 0, renderer->num_text_vertices);
  gl_ok_or_die;

  return input.was_pressed[BUTTON_START];
}
