#include "flat_math.c"
#include "flat_utils.c"
#include "flat_platform_api.h"

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

typedef enum {
  DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT
} Direction;

typedef enum {
  PRIORITY_UNIMPORTANT = -1,
  PRIORITY_MAP = 0,
  PRIORITY_PLAYER = 1
} EntityPriority;

typedef struct {
  EntityType type;
  v3 pos;
  v2 vel;
  EntityPriority priority;

  /* physics */
  Rect hitbox;

  /* animation */
  float animation_time;
  Direction last_direction;

  /* Monster stuff */
  v2 target;
} Entity;

typedef struct {
  Entity entities[256];
  int num_entities;
  Stack stack;
  char stack_data[128*1024*1024];
} State;

static void print(const char* fmt, ...);
#ifdef DEBUG
  #define debug print
#else
  #define debug
#endif

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

static void entity_evict(Entity *e) {
  switch (e->type) {
    default: debug("evicting entity %e\n", e);
  }
}

static int entity_push(State *state, Entity e) {
  Entity *dest;
  /* if full, find one with less priority */
  if (state->num_entities < ARRAY_LEN(state->entities))
    dest = &state->entities[state->num_entities++];
  else {
    int i;
    dest = &state->entities[0];
    for (i = 1; i < state->num_entities; ++i)
      if (state->entities[i].priority < dest->priority)
        dest = &state->entities[i];

    if (dest->priority >= e.priority)
      return 1;
    entity_evict(dest);
  }

  *dest = e;
  return 0;
}

static Glyph glyph_get(Renderer *r, char c) {
  return r->glyphs[c - RENDERER_FIRST_CHAR];
}

typedef enum {
  ANIMATION_STATE_NULL,
  ANIMATION_STATE_PLAYER_STANDING_LEFT,
  ANIMATION_STATE_PLAYER_STANDING_RIGHT,
  ANIMATION_STATE_PLAYER_WALKING_LEFT,
  ANIMATION_STATE_PLAYER_WALKING_RIGHT,
  ANIMATION_STATE_COUNT
} AnimationState;

typedef struct {
  float x, y, w, h, dx, dy;
  int columns, num;
  float time;
} SpriteSheetAnimation;

static SpriteSheetAnimation spriteanim[] = {
  {0},
  {0.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 4, 1, 0.5f},
  {0.25f, 0.0f, 0.25f, 0.25f, 0.25f, 0.25f, 4, 1, 0.5f},
  {0.0f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 4, 4, 0.2f},
  {0.0f, 0.0f, 0.25f, 0.25f, 0.25f, 0.25f, 4, 4, 0.2f}
};

STATIC_ASSERT(ANIMATION_STATE_COUNT == ARRAY_LEN(spriteanim), all_tex_pos_defined);

static Rect get_anim_tex(AnimationState which, float time) {
  SpriteSheetAnimation s;
  int n;
  Rect r;

  ENUM_CHECK(ANIMATION_STATE, which);

  s = spriteanim[which];
  n = fmod(time, (s.num*s.time)) / s.time;
  r.x0 = s.x + (n % s.columns)*s.dx;
  r.y0 = s.y - (n / s.columns)*s.dy;
  r.x1 = r.x0+s.w;
  r.y1 = r.y0+s.h;

  return r;
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

  if (center) {
    x -= w/2;
    y -= h/2;
  }

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

static void render_anim_sprite(Renderer *r, float x, float y, float z, float w, float h, AnimationState anim_state, float anim_time, int center) {
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

GAME_INIT(init) {
  State *s = memory;
  assert(memory_size >= (int)sizeof(State));
  memset(s, 0, sizeof(State));

  (void)function_ptrs;

  /* Init stack */
  stack_init(&s->stack, s->stack_data, sizeof(s->stack_data));

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
    e.priority = 1;
    e.type = ENTITY_TYPE_WALL;
    e.pos = v3_create(0, 4, 0);
    e.hitbox = rect_create(-4, -0.1f, 4, 0.1f);
    entity_push(s, e);
  }
  return 0;
}

GAME_MAIN_LOOP(main_loop) {
  static long last_ms;
  State* s;
  float dt;
  int i;

  s = (State*)memory;
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

      case ENTITY_TYPE_PLAYER: {
        #define PLAYER_ACC 15
        #define PLAYER_MAXSPEED 3.0f
        #define PLAYER_SKID 7.0f

        float speed;

        if (!input.is_down[BUTTON_RIGHT] && e->vel.x > 0.0f)
          e->vel.x -= fmin(PLAYER_SKID * dt, e->vel.x);
        if (!input.is_down[BUTTON_LEFT] && e->vel.x < 0.0f)
          e->vel.x += fmin(PLAYER_SKID * dt, -e->vel.x);
        if (!input.is_down[BUTTON_UP] && e->vel.y > 0.0f)
          e->vel.y -= fmin(PLAYER_SKID * dt, e->vel.y);
        if (!input.is_down[BUTTON_DOWN] && e->vel.y < 0.0f)
          e->vel.y += fmin(PLAYER_SKID * dt, -e->vel.y);

        if (!input.is_down[BUTTON_RIGHT] && !input.is_down[BUTTON_LEFT])
          e->vel.x -= sign(e->vel.x) * fmin(dt*PLAYER_SKID, fabs(e->vel.x));
        if (!input.is_down[BUTTON_UP] && !input.is_down[BUTTON_DOWN])
          e->vel.y -= sign(e->vel.y) * fmin(dt*PLAYER_SKID, fabs(e->vel.y));

        e->vel.x += dt*PLAYER_ACC * input.is_down[BUTTON_RIGHT];
        e->vel.x -= dt*PLAYER_ACC * input.is_down[BUTTON_LEFT];
        e->vel.y += dt*PLAYER_ACC * input.is_down[BUTTON_UP];
        e->vel.y -= dt*PLAYER_ACC * input.is_down[BUTTON_DOWN];

        if (e->vel.x > 0)
          e->last_direction = DIR_RIGHT;
        if (e->vel.x < 0)
          e->last_direction = DIR_LEFT;

        speed = length(e->vel.x, e->vel.y);
        if (speed > PLAYER_MAXSPEED) {
          e->vel.x = e->vel.x * PLAYER_MAXSPEED / speed;
          e->vel.y = e->vel.y * PLAYER_MAXSPEED / speed;
        }

        handle_collision(s, e, dt);

        {
          AnimationState as = ANIMATION_STATE_PLAYER_STANDING_LEFT;
          if (fabs(speed) < 0.001f)
            as = e->last_direction == DIR_LEFT ? ANIMATION_STATE_PLAYER_STANDING_LEFT : ANIMATION_STATE_PLAYER_STANDING_RIGHT;
          else
            as = e->last_direction == DIR_LEFT ? ANIMATION_STATE_PLAYER_WALKING_LEFT : ANIMATION_STATE_PLAYER_WALKING_RIGHT;
          render_anim_sprite(renderer, GET3(e->pos), 1, 1, as, e->animation_time, 1);
        }


        render_text(renderer, entity_type_names[e->type], GET3(e->pos), 0.1f, 1);
        renderer->camera_pos = e->pos;
        renderer->camera_pos.z += RENDERER_CAMERA_HEIGHT;

        #undef PLAYER_ACC
        #undef PLAYER_MAXSPEED
        #undef PLAYER_SKID
      } break;

      case ENTITY_TYPE_WALL:
        render_anim_sprite(renderer, GET3(e->pos), GET2(rect_size(e->hitbox)), ANIMATION_STATE_PLAYER_WALKING_LEFT, e->animation_time, 1);
        break;

      case ENTITY_TYPE_MONSTER:
        break;

      case ENTITY_TYPE_DERPER:
        break;

      case ENTITY_TYPE_THING:
        break;
    }

    e->animation_time += dt;
  }

  {
    static unsigned int t;
    t += dt;
    render_anim_sprite(renderer, 0.1, 0.1, 0, 1, 1, ANIMATION_STATE_PLAYER_WALKING_LEFT, t, 1);
    render_anim_sprite(renderer, -0.3, 0.3, 0, 1, 1, ANIMATION_STATE_PLAYER_WALKING_LEFT, t, 1);
    render_anim_sprite(renderer, 0.3, -0.3, 0, 1, 1, ANIMATION_STATE_PLAYER_WALKING_LEFT, t, 1);
    render_anim_sprite(renderer, -0.3, -0.3, 0, 1, 1, ANIMATION_STATE_PLAYER_WALKING_LEFT, t, 1);
  }

  #if 0
    puts("********* Entities *********");
    for (i = 0; i < s->num_entities; ++i)
      print("%e\n", &s->entities[i]);

    puts("********* Sprite Vertices *********");
    for (i = 0; i < renderer->num_sprites; ++i)
      print("%v2\n", &renderer->sprite_vertices[i].tex);

    puts("********* Text Vertices *********");
    for (i = 0; i < renderer->num_text_vertices; ++i)
      printf("%f %f %f\n", renderer->text_vertices[i].pos.x, renderer->text_vertices[i].pos.y, renderer->text_vertices[i].pos.z);
  #endif
  return input.was_pressed[BUTTON_START];
}
