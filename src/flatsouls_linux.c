#include "flatsouls_platform.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <dlfcn.h>

int main(int argc, const char** argv) {
	int err;
  /* dynamic linking to shared object */
  void* dl_handle;
  int (*main_loop)(Bitmap);
  /* SDL stuff */
  SDL_Window *window;
  SDL_Event event;
  /* Api to game */
  Bitmap bitmap;

  (void)argc, (void)argv;

  /* init SDL */
  err = SDL_Init(SDL_INIT_EVERYTHING);
  if (err) abort();
  window = SDL_CreateWindow("Hello world", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
  if (!window) abort();

  /* dynamically load main loop */
  dl_handle = dlopen("flatsouls.so", RTLD_NOW);
  if (!dl_handle) abort();
  *(void**) (&main_loop) = dlsym(dl_handle, "main_loop");
  if (!main_loop) abort();

  /* init bitmap buffer */
  bitmap.w = 100;
  bitmap.h = 100;
  bitmap.buf = malloc(bitmap.w * bitmap.w * 3);

  /* main loop */
  while (1) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_WINDOWEVENT:
        switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE: return 0;
        }
      }
    }
    main_loop(bitmap);
  }
  return 0;
}
