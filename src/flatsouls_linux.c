#include "flatsouls_platform.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#define sdl_try(stmt) ((stmt) && (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__,SDL_GetError()), abort(),0))
#define sdl_abort() (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__, SDL_GetError()), abort())

static Button key_to_button(Sint32 key) {
  switch (key) {
    case SDLK_RETURN: return BUTTON_START;
    case SDLK_t: return BUTTON_A;
  }
  return 0;
}

int main(int argc, const char** argv) {
  int err;

  /* dynamic linking to shared object */
  int (*main_loop)(void*, Uint64, Input);
  int (*init)(void*, int);

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

  init(memory, MEMORY_SIZE);

  /* main loop */
  while (1) {
    SDL_Event event;
    input.was_pressed = 0;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_WINDOWEVENT:
          switch (event.window.event) {
            case SDL_WINDOWEVENT_CLOSE: return 0;
          }
          break;
        case SDL_KEYDOWN: {
          Button b = key_to_button(event.key.keysym.sym);
          if (!b) break;
          input.was_pressed |= b;
          input.is_down |= b;
        } break;
        case SDL_KEYUP: {
          Button b = key_to_button(event.key.keysym.sym);
          if (!b) break;
          input.was_pressed &= ~b;
          input.is_down &= ~b;
        } break;
      }
    }
    err = main_loop(memory, SDL_GetTicks(), input);
    if (err) return 0;
    SDL_GL_SwapWindow(window);
  }

  return 0;
}
