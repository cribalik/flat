#define _POSIX_C_SOURCE 200112L
#include "flatsouls_platform.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <SOIL/SOIL.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "flatsouls_logging.h"

#define sdl_try(stmt) ((stmt) && (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__,SDL_GetError()), abort(),0))
#define sdl_abort() (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__, SDL_GetError()), abort())

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

void load_texture_from_file(const char* filename, GLuint* result, int* w, int* h) {
  glGenTextures(1, result);
  glBindTexture(GL_TEXTURE_2D, *result);
  glOKORDIE;

  {
    unsigned char* image = SOIL_load_image(filename, w, h, 0, SOIL_LOAD_RGBA);
    if (!image) LOG_AND_ABORT(LOG_ERROR, "Failed to load image %s: %s\n", filename, strerror(errno));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    SOIL_free_image_data(image);
    glOKORDIE;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glOKORDIE;
}

static int key_codes[NUM_BUTTONS] = {
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

int main(int argc, const char** argv) {
  int err;

  /* dynamic linking to shared object */
  int (*main_loop)(void*, Uint64, Input);
  int (*init)(void*, int, Funs);

  /* SDL stuff */
  int screen_w = 800, screen_h = 600;
  SDL_Window *window;
  SDL_GLContext gl_context = 0;

  /* Api to game */
  #define MEMORY_SIZE 512*1024*1024
  char* memory = malloc(MEMORY_SIZE);
  Input input = {0};

  /* unused */
  (void)argc, (void)argv;

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
  if (!window) sdl_abort();

  gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) sdl_abort();

  /* Load gl functions */
  #if 0
  {
    #define GLFUN(name, prototype) *(void**) (&name) = SDL_GL_GetProcAddress(#name);
    #include "flatsouls_gl.h"
    #undef GLFUN
  }
  #endif

  glViewport(0, 0, screen_w, screen_h);

  /* load main loop */
  {
    void* obj = SDL_LoadObject("flatsouls.so");
    if (!obj) sdl_abort();
    *(void**)(&main_loop) = SDL_LoadFunction(obj, "main_loop");
    if (!main_loop) sdl_abort();
    *(void**)(&init) = SDL_LoadFunction(obj, "init");
    if (!init) sdl_abort();
  }

  {
    Funs dfuns;
    dfuns.load_texture_from_file = load_texture_from_file;
    init(memory, MEMORY_SIZE, dfuns);
  }

  /* main loop */
  {
    while (1) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        memset(input.was_pressed, 0, sizeof(input.was_pressed));
        switch (event.type) {
          case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) return 0;
            break;
          case SDL_KEYDOWN: {
            int i;
            if (event.key.repeat) break;
            for (i = 0; i < NUM_BUTTONS; ++i) {
              if (event.key.keysym.sym == key_codes[i]) {
                input.was_pressed[i] = !input.is_down[i];
                input.is_down[i] = 1;
                break;
              }
            }
          } break;
          case SDL_KEYUP: {
            int i;
            if (event.key.repeat) break;
            for (i = 0; i < NUM_BUTTONS; ++i) {
              if (event.key.keysym.sym == key_codes[i]) {
                input.is_down[i] = 0;
                break;
              }
            }
          } break;
        }
      }
      err = main_loop(memory, SDL_GetTicks(), input);
      if (err) return 0;
      SDL_GL_SwapWindow(window);
    }
  }

  return 0;
}
