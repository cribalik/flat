#include "flat_utils.c"
#include "flat_math.c"

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

typedef enum {
  ENTITY_TYPE_NULL,
  ENTITY_TYPE_PLAYER,
  ENTITY_TYPE_WALL,
  ENTITY_TYPE_MONSTER,
  ENTITY_TYPE_DERPER,
  ENTITY_TYPE_THING,
  ENTITY_TYPE_COUNT
} EntityType;

typedef struct {
  EntityType type;
  v3 pos;
  v2 vel;

  /* physics */
  Rect hitbox;

  /* animation */
  unsigned int animation_time;

  /* Monster stuff */
  v2 target;
} Entity;


typedef struct {
  GLuint id;
  v2i size;
} Texture;

typedef struct {
  v3 pos;
  v2 tex;
} SpriteVertex;

static SpriteVertex spritevertex_create(float x, float y, float z, float tx, float ty) {
  SpriteVertex result;
  result.pos = v3_create(x,y,z);
  result.tex = v2_create(tx,ty);
  return result;
}
typedef SpriteVertex TextVertex;

typedef struct {
  /* sprites */
  GLuint
    sprites_vertex_array, sprite_vertex_buffer,
    sprite_shader,
    /* locations */
    sprite_camera_loc,
    sprite_far_z_loc,
    sprite_near_z_loc,
    sprite_nearsize_loc
    ;
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
  #define RENDERER_CAMERA_HEIGHT 5
  v3 camera_pos;
} Renderer;

typedef struct {
  Entity entities[256];
  int num_entities;
  Renderer renderer;
  Stack stack;
  char stack_data[128*1024*1024];
} State;


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

static const char* entity_type_names[] = {
  "Null",
  "Player",
  "Wall",
  "Monster",
  "Derper",
  "Thing"
};
STATIC_ASSERT(ARRAY_LEN(entity_type_names) == ENTITY_TYPE_COUNT, all_entity_names_entered);

static void wall_test(float x0, float y0, float x1, float y1, float wx0, float wy0, float wx1, float wy1, float *t_out, float *nx_out, float *ny_out) {
  float ux = x1 - x0;
  float uy = y1 - y0;
  float vx = wx1 - wx0;
  float vy = wy1 - wy0;
  float d = ux*vy - uy*vx;
  float wx = wx0 - x0;
  float wy = wy0 - y0;
  float t,s;

  if (fabs(d) < 0.0001f)
    return;

  s = (wx*uy - wy*ux)/d;
  t = (wx*vy - wy*vx)/d;
  if (t < 0 || t > 1 || s < 0 || s > 1)
    return;

  if (t < *t_out) {
    *t_out = t;
    *nx_out = -vy;
    *ny_out = vx;
  }
  /*printf("%f %f (%f,%f)->(%f,%f) (%f,%f)\n", *t_out, s, x0, y0, x1, y1, wx0, wy0);*/
}

static int physics_rect_collide(Rect a, Rect b) {
  return !(a.x1 < b.x0 || a.x0 > b.x1 || a.y0 > b.y1 || a.y1 < b.y1);
}

static void handle_collision(State *s, Entity *e, float dt) {
  int i,j;
  float w,h;

  w = (e->hitbox.x1 - e->hitbox.x0)/2.0f;
  h = (e->hitbox.y1 - e->hitbox.y0)/2.0f;

  for (i = 0; i < 4; ++i) {
    float t, x0,y0,x1,y1, nx, ny;
    Entity *hit;

    hit = 0;
    t = 2.0f;
    x0 = e->hitbox.x0 + e->pos.x + w;
    y0 = e->hitbox.y0 + e->pos.y + h;
    x1 = x0 + e->vel.x * dt;
    y1 = y0 + e->vel.y * dt;

    for (j = 0; j < s->num_entities; ++j) {
      float wx0,wy0,wx1,wy1, nx_tmp,ny_tmp, t_tmp;
      Entity *target;

      target = s->entities + j;
      if (target == e)
        continue;

      /* expand hitbox */
      wx0 = target->hitbox.x0 - w + target->pos.x;
      wx1 = target->hitbox.x1 + w + target->pos.x;
      wy0 = target->hitbox.y0 - h + target->pos.y;
      wy1 = target->hitbox.y1 + h + target->pos.y;

      /* does line hit the box ? */
      t_tmp = 2.0f;
      /* lines need to be clockwise oriented to get correct normals */
      wall_test(x0, y0, x1, y1, wx0, wy1, wx1, wy1, &t_tmp, &nx_tmp, &ny_tmp); /* top */
      wall_test(x0, y0, x1, y1, wx1, wy0, wx0, wy0, &t_tmp, &nx_tmp, &ny_tmp); /* bottom */
      wall_test(x0, y0, x1, y1, wx0, wy0, wx0, wy1, &t_tmp, &nx_tmp, &ny_tmp); /* left */
      wall_test(x0, y0, x1, y1, wx1, wy1, wx1, wy0, &t_tmp, &nx_tmp, &ny_tmp); /* right */

      if (t_tmp == 2.0f)
        continue;

      if (t_tmp < t) {
        hit = target;
        t = t_tmp;
        nx = nx_tmp;
        ny = ny_tmp;
      }
    }

    if (!hit)
      break;

    normalize(&nx, &ny);

    if (hit->type == ENTITY_TYPE_WALL) {
      float vx,vy, ax,ay, bx,by, dot;
      /**
       * Glide along the wall
       *
       * v is the movement vector
       * a is the part that goes up to the wall
       * b is the part that goes beyond the wall
       */
      vx = x1 - x0;
      vy = y1 - y0;
      dot = vx*nx + vy*ny;

      /* go up against the wall */
      ax = nx * dot * t;
      ay = ny * dot * t;
      /* back off a bit */
      ax += nx * 0.0001f;
      ay += ny * 0.0001f;
      e->pos.x = x0 + ax;
      e->pos.y = y0 + ay;

      /* remove the part that goes into the wall, and glide the rest */
      bx = vx - dot * nx;
      by = vy - dot * ny;
      e->vel = v2_create(bx/dt, by/dt);

      /*printf("(%f,%f) (%f,%f) (%f,%f) (%f,%f)\n", nx, ny, vx, vy, ax, ay, bx, by);*/
    }
    else {
      /* TODO: handle collision with non-wall type */
      continue;
    }
  }

  e->pos.x += e->vel.x*dt;
  e->pos.y += e->vel.y*dt;
}

static int entity_push(State *state, Entity e) {
  if (state->num_entities >= ARRAY_LEN(state->entities))
    return 1;

  state->entities[state->num_entities++] = e;
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

static Rect get_anim_tex(AnimationState which, unsigned int time) {
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

static void render_text(Renderer *r, const char *str, float pos_x, float pos_y, float pos_z, float height, int center) {
  float h,w, scale, ipw,iph, x,y,z, tx0,ty0,tx1,ty1;
  SpriteVertex *v;

  scale = height / RENDERER_FONT_SIZE;
  ipw = 1.0f / r->text_atlas.size.x;
  iph = 1.0f / r->text_atlas.size.y;

  if (r->num_text_vertices + strlen(str) >= ARRAY_LEN(r->text_vertices))
    return;

  if (center) {
    pos_x -= calc_string_width(r, str) * scale / 2;
    /*pos.y -= height/2.0f;*/ /* Why isn't this working? */
  }

  for (; *str && r->num_text_vertices + 6 < (int)ARRAY_LEN(r->text_vertices); ++str) {
    Glyph g = glyph_get(r, *str);

    x = pos_x + g.offset_x*scale;
    y = pos_y - g.offset_y*scale;
    z = pos_z;
    w = (g.x1 - g.x0)*scale;
    h = -(g.y1 - g.y0)*scale;

    /* scale texture to atlas */
    tx0 = g.x0 * ipw,
    tx1 = g.x1 * ipw;
    ty0 = g.y0 * iph;
    ty1 = g.y1 * iph;

    v = r->text_vertices + r->num_text_vertices;

    *v++ = spritevertex_create(x, y, z, tx0, ty0);
    *v++ = spritevertex_create(x + w, y, z, tx1, ty0);
    *v++ = spritevertex_create(x, y + h, z, tx0, ty1);
    *v++ = spritevertex_create(x, y + h, z, tx0, ty1);
    *v++ = spritevertex_create(x + w, y, z, tx1, ty0);
    *v++ = spritevertex_create(x + w, y + h, z, tx1, ty1);

    r->num_text_vertices += 6;
    pos_x += g.advance * scale;
  }
}

static void render_sprite(Renderer *r, float x, float y, float z, float w, float h, float tx0, float ty0, float tx1, float ty1, int center) {
  /* TODO: get x and y positions of sprite in sprite sheet */
  SpriteVertex *v;

  if (center)
    x -= w/2,
    y -= h/2;

  assert(r->sprite_atlas.id);
  assert(r->num_sprites + 6 < (int)ARRAY_LEN(r->sprite_vertices));

  v = r->sprite_vertices + r->num_sprites;
  *v++ = spritevertex_create(x, y, z, tx0, ty0);
  *v++ = spritevertex_create(x + w, y, z, tx1, ty0);
  *v++ = spritevertex_create(x, y + h, z, tx0, ty1);
  *v++ = spritevertex_create(x, y + h, z, tx0, ty1);
  *v++ = spritevertex_create(x + w, y, z, tx1, ty0);
  *v++ = spritevertex_create(x + w, y + h, z, tx1, ty1);
  r->num_sprites += 6;
}

static void render_anim_sprite(Renderer *r, float x, float y, float z, float w, float h, AnimationState anim_state, unsigned int anim_time, int center) {
  Rect tex = get_anim_tex(anim_state, anim_time);
  render_sprite(r, x, y, z, w, h, GET_LINE(tex), center);
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

GAME_INIT(init) {
  State *s = memory;
  assert(memory_size >= (int)sizeof(State));
  memset(s, 0, sizeof(State));

  /* Init function pointers */
  load_image_texture_from_file = function_ptrs.load_image_texture_from_file;
  load_font_from_file = function_ptrs.load_font_from_file;

  /* Init stack */
  stack_init(&s->stack, s->stack_data, sizeof(s->stack_data));

  /* Init renderer */
  {
    Renderer* r = &s->renderer;
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
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->sprite_vertices), (void*) offsetof(SpriteVertex, tex));
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
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*r->text_vertices), (void*) offsetof(SpriteVertex, tex));
    }

    /* Compile shaders */
    r->sprite_shader = compile_shader("assets/shaders/sprite_vertex.glsl", "assets/shaders/sprite_fragment.glsl");
    gl_ok_or_die;

    /* Get/set uniforms */
    glUseProgram(r->sprite_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->sprite_atlas.id);
    glUniform1i(glGetUniformLocation(r->sprite_shader, "u_texture"), 0);
    r->sprite_camera_loc = glGetUniformLocation(r->sprite_shader, "camera");
    r->sprite_far_z_loc = glGetUniformLocation(r->sprite_shader, "far_z");
    r->sprite_near_z_loc = glGetUniformLocation(r->sprite_shader, "near_z");
    r->sprite_nearsize_loc = glGetUniformLocation(r->sprite_shader, "nearsize");
    gl_ok_or_die;
  }

  /* Create player */
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_PLAYER;
    e.hitbox = line_create(-0.5, -0.5, 0.5, 0.5);
    entity_push(s, e);
  }

  /* Create walls */
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_WALL;
    e.pos = v3_create(0, -4, 0);
    e.hitbox = rect_create(-4, -0.1f, 4, 0.1f);
    entity_push(s, e);
  }
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_WALL;
    e.pos = v3_create(-4, 0, 0);
    e.hitbox = rect_create(-0.1f, -4, 0.1f, 4);
    entity_push(s, e);
  }
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_WALL;
    e.pos = v3_create(4, 0, 0);
    e.hitbox = rect_create(-0.1f, -4, 0.1f, 4);
    entity_push(s, e);
  }
  {
    Entity e = {0};
    e.type = ENTITY_TYPE_WALL;
    e.pos = v3_create(0, 4, 0);
    e.hitbox = rect_create(-4, -0.1f, 4, 0.1f);
    entity_push(s, e);
  }
}

static void print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  while (*fmt) {
    if (*fmt != '%') {
      putchar(*fmt++);
      continue;
    }

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

GAME_MAIN_LOOP(main_loop) {
  static long last_ms;
  State* s;
  Renderer* renderer;
  float dt;
  int i;

  s = (State*)memory;
  renderer = &s->renderer;
  dt = (ms - last_ms) / 1000.0f;
  dt = dt > 0.05f ? 0.05f : dt;
  last_ms = ms;

  /* clear */
  render_clear(renderer);

  /* Update entities */
  for (i = 0; i < s->num_entities; ++i) {
    Entity *e = s->entities + i;

    ENUM_CHECK(ENTITY_TYPE, e->type);

    switch (e->type) {
      case ENTITY_TYPE_NULL:
      case ENTITY_TYPE_COUNT:
        break;

      case ENTITY_TYPE_PLAYER:
        #define PLAYER_ACC 10

        e->vel.x += dt*PLAYER_ACC*input.is_down[BUTTON_RIGHT];
        e->vel.x -= dt*PLAYER_ACC*input.is_down[BUTTON_LEFT];
        if (input.was_pressed[BUTTON_UP])
          e->vel.y = 10.0f;

        e->animation_time += dt;

        e->vel.y -= dt * 25.0f;

        handle_collision(s, e, dt);

        render_anim_sprite(&s->renderer, GET3(e->pos), 1, 1, ANIMATION_STATE_PLAYER, e->animation_time, 1);
        render_text(&s->renderer, entity_type_names[e->type], GET3(e->pos), 0.1f, 1);
        s->renderer.camera_pos = e->pos;
        print("%e\n", e);
        s->renderer.camera_pos.z += RENDERER_CAMERA_HEIGHT;

        #undef PLAYER_ACC
        break;

      case ENTITY_TYPE_WALL:
        render_anim_sprite(&s->renderer, GET3(e->pos), GET2(rect_size(e->hitbox)), ANIMATION_STATE_PLAYER, e->animation_time, 1);
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
    render_anim_sprite(&s->renderer, 0.1, 0.1, 0, 1, 1, ANIMATION_STATE_PLAYER, t, 1);
    render_anim_sprite(&s->renderer, -0.3, 0.3, 0, 1, 1, ANIMATION_STATE_PLAYER, t, 1);
    render_anim_sprite(&s->renderer, 0.3, -0.3, 0, 1, 1, ANIMATION_STATE_PLAYER, t, 1);
    render_anim_sprite(&s->renderer, -0.3, -0.3, 0, 1, 1, ANIMATION_STATE_PLAYER, t, 1);
  }

  #if 0
    puts("********* Entities *********");
    for (i = 0; i < s->num_entities; ++i)
      print("%e\n", &s->entities[i]);

    puts("********* Sprite Vertices *********");
    for (i = 0; i < renderer->num_sprites; ++i)
      printf("%f %f %f\n", renderer->sprite_vertices[i].pos.x, renderer->sprite_vertices[i].pos.y, renderer->sprite_vertices[i].pos.z);

    puts("********* Text Vertices *********");
    for (i = 0; i < renderer->num_text_vertices; ++i)
      printf("%f %f %f\n", renderer->text_vertices[i].pos.x, renderer->text_vertices[i].pos.y, renderer->text_vertices[i].pos.z);
  #endif

  glUseProgram(renderer->sprite_shader);

  /* send camera position to shader */
  {
    float w,h, far_z,near_z, fov;
    fov = PI/3.0f;
    far_z = -10.0f;
    near_z = -0.5f;
    w = - 2.0f * near_z * tan(fov/2);
    h = w * 9.0f / 16.0f;
    glUniform3f(renderer->sprite_camera_loc, GET3(renderer->camera_pos));
    glUniform1f(renderer->sprite_far_z_loc, far_z);
    glUniform1f(renderer->sprite_near_z_loc, near_z);
    glUniform2f(renderer->sprite_nearsize_loc, w, h);
  }
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
