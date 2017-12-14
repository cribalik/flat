#define _POSIX_C_SOURCE 200112L
#include "flat_utils.c"
#include "flat_render.h"
#include "flat_platform_api.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SOIL/SOIL.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "flat_ttf.c"

/* ======= Platform api ======= */

#define GL_FUN(ret, name, par) ret (*name) par;
#include "flat_glfuns.incl"
#undef GL_FUN

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


void load_image_texture_from_file(const char* filename, GLuint* result, int* w, int* h) {
  glGenTextures(1, result);
  glBindTexture(GL_TEXTURE_2D, *result);
  gl_ok_or_die;

  {
    unsigned char* image = SOIL_load_image(filename, w, h, 0, SOIL_LOAD_RGBA);
    if (!image) die("Failed to load image %s: %s\n", filename, strerror(errno));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    SOIL_free_image_data(image);
    gl_ok_or_die;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gl_ok_or_die;
}

void load_font_from_file(const char* filename, GLuint gl_texture, int tex_w, int tex_h, unsigned char first_char, unsigned char last_char, float height, Glyph *out_glyphs) {
  #define BUFFER_SIZE 1024*1024
  unsigned char* ttf_mem;
  unsigned char* bitmap;
  FILE* f;
  int res;

  ttf_mem = malloc(BUFFER_SIZE);
  bitmap = malloc(tex_w * tex_h);
  if (!ttf_mem || !bitmap) die("Failed to allocate memory for font: %s\n", strerror(errno));

  f = fopen(filename, "rb");
  if (!f) die("Failed to open ttf file %s: %s\n", filename, strerror(errno));
  fread(ttf_mem, 1, BUFFER_SIZE, f);

  res = stbtt_BakeFontBitmap(ttf_mem, 0, height, bitmap, tex_w, tex_h, first_char, last_char - first_char, (stbtt_bakedchar*) out_glyphs);
  if (res <= 0) die("Failed to bake font: %i\n", res);

  glBindTexture(GL_TEXTURE_2D, gl_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tex_w, tex_h, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_ok_or_die;

  fclose(f);
  free(ttf_mem);
  free(bitmap);
}

/* ======= Internals ======= */

static Button key_to_button(Sint32 key) {
  switch (key) {
    case SDLK_RETURN: return BUTTON_START;
    case SDLK_t: return BUTTON_A;
    case SDLK_UP: return BUTTON_UP;
    case SDLK_DOWN: return BUTTON_DOWN;
    case SDLK_LEFT: return BUTTON_LEFT;
    case SDLK_RIGHT: return BUTTON_RIGHT;
  }
  return 0;
}

static i32 key_codes[] = {
  0,
  SDLK_t,
  SDLK_y,
  SDLK_r,
  SDLK_e,
  SDLK_UP,
  SDLK_DOWN,
  SDLK_LEFT,
  SDLK_RIGHT,
  SDLK_RETURN,
  SDLK_RSHIFT
};
STATIC_ASSERT(ARRAY_LEN(key_codes) == BUTTON_COUNT, assign_all_keycodes);

#define sdl_try(stmt) ((stmt) && (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__,SDL_GetError()), abort(),0))
#define sdl_abort() (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__, SDL_GetError()), abort())

int main(int argc, const char** argv) {
  int err;

  /* dynamic linking to game */
  MainLoop main_loop;
  Init init;

  /* SDL stuff */
  int screen_w = 1280, screen_h = 720;
  SDL_Window *window;
  SDL_GLContext gl_context = 0;

  /* Api to game */
  #define MEMORY_SIZE 512*1024*1024
  char* memory;
  Renderer *renderer;
  Input input = {0};

  /* unused */
  (void)argc, (void)argv;

  /* Alloc memory */
  memory = malloc(MEMORY_SIZE);
  if (!memory)
    die("Not enough memory");

  /* Alloc renderer */
  renderer = (Renderer*)memory;
  memory += sizeof(*renderer);

  /* Fix for some builds of SDL 2.0.4, see https://bugs.gentoo.org/show_bug.cgi?id=610326 */
  setenv("XMODIFIERS", "@im=none", 1);

  /* init SDL */
  sdl_try(SDL_Init(SDL_INIT_EVERYTHING));
  atexit(SDL_Quit);

  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));

  window = SDL_CreateWindow("Hello world",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            screen_w, screen_h,
                            SDL_WINDOW_OPENGL);
  if (!window)
    sdl_abort();

  gl_context = SDL_GL_CreateContext(window);
  if (!gl_context)
    sdl_abort();

  /* Load gl functions */
  {
    #define GL_FUN(ret, name, par) *(void**) (&name) = (void*)SDL_GL_GetProcAddress(#name);
    #include "flat_glfuns.incl"
    #undef GL_FUN
  }

  glViewport(0, 0, screen_w, screen_h);

  /* Init renderer */
  {
    Renderer *r = renderer;

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

  /* load game loop */
  {
    void* obj = SDL_LoadObject("flat.so");
    if (!obj) sdl_abort();
    *(void**)(&main_loop) = SDL_LoadFunction(obj, "main_loop");
    if (!main_loop) sdl_abort();
    *(void**)(&init) = SDL_LoadFunction(obj, "init");
    if (!init) sdl_abort();
  }

  /* call init */
  {
    Funs dfuns;
    init(memory, MEMORY_SIZE, dfuns);
  }

  /* main loop */
  while (1) {
    int i;
    SDL_Event event;

    for (i = 0; i < ARRAY_LEN(input.was_pressed); ++i)
      input.was_pressed[i] = 0;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_WINDOWEVENT:
          if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            return 0;
          break;

        case SDL_KEYDOWN:
          if (event.key.repeat)
            break;

          ENUM_FOREACH(i, BUTTON) {
            if (event.key.keysym.sym != key_codes[i])
              continue;

            input.was_pressed[i] = 1;
            input.is_down[i] = 1;
            break;
          }
          break;

        case SDL_KEYUP:
          if (event.key.repeat)
            break;
          ENUM_FOREACH(i, BUTTON) {
            if (event.key.keysym.sym == key_codes[i]) {
              input.is_down[i] = 0;
              break;
            }
          }
          break;
      }
    }
    err = main_loop(memory, SDL_GetTicks(), input, renderer);
    if (err) return 0;

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

    SDL_GL_SwapWindow(window);
  }

  return 0;
}
