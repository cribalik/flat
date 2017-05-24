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
#include <ft2build.h>
#include FT_FREETYPE_H


#define sdl_try(stmt) ((stmt) && (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__,SDL_GetError()), abort(),0))
#define sdl_abort() (fprintf(stderr, "%s:%i error: %s\n", __FILE__, __LINE__, SDL_GetError()), abort())

/* ======= Externals ======= */

void load_image_texture_from_file(const char* filename, GLuint* result, int* w, int* h) {
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

static const char* ft_get_error_message(FT_Error err) {
    #undef __FTERRORS_H__
    #define FT_ERRORDEF( e, v, s )  case e: return s;
    #define FT_ERROR_START_LIST     switch (err) {
    #define FT_ERROR_END_LIST       }
    #include FT_ERRORS_H
    return "(Unknown error)";
}
void load_font_from_file(const char* filename, int height, GLuint* out_tex, Glyph *out_glyphs) {
  #define START_CHAR 32
  #define END_CHAR 127

  // load font file
  int err;
  FT_Library ft;
  FT_Face face;
  int tex_w, tex_h;
  if (err = FT_Init_FreeType(&ft)) LOG_AND_ABORT("Failed to init freetype: %s\n", ft_get_error_message(err));
  if (err = FT_New_Face(ft, filename, 0, &face)) LOG_AND_ABORT("Failed to load font at %s: %s\n", filename, ft_get_error_message(err));
  FT_Set_Pixel_Sizes(face, 0, height);

  // pack glyphs
  {
    const int TEX_WIDTH = 512;
    const int PADDING = 4;
    int
      x = 0,
      y = 0,
      row_height = 0,
      i;
    for (i = START_CHAR; i < END_CHAR; ++i) {
      int w,h;
      Glyph g;
      FT_Load_Char(face, i, FT_LOAD_RENDER);
      w = face->glyph->bitmap.width + 2*PADDING;
      h = face->glyph->bitmap.height + 2*PADDING;
      row_height = MAX(row_height, h);
      if (x + w > TEX_WIDTH) {
        if (x == 0) LOG_AND_ABORT("Glyph somehow wider than texture width (%i and %i)\n", w, TEX_WIDTH);
        y += row_height;
        row_height = 0;
        x = 0;
      }

      g.x = face->glyph->bitmap_left/(float)height;
      g.y = face->glyph->bitmap_top/(float)height;
      g.w = face->glyph->bitmap.width/(float)height;
      g.h = face->glyph->bitmap.height/(float)height;
      g.advance = face->glyph->advance.x/(float)height/64.0f;
      g.tx = x + PADDING;
      g.ty = y + PADDING;
      g.tw = w - 2*PADDING;
      g.th = h - 2*PADDING;
      out_glyphs[i] = g;
    }
    tex_w = next_pow2(TEX_WIDTH);
    tex_h = next_pow2(y + row_height);
    printf("Font texture dimensions: %u %u\n", maxW, maxH);
  }

  /* check if opengl support that size of texture */
  {
    GLint max_texture_size;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    assert(max_texture_size >= tex_w && max_texture_size >= tex_h);
    LOG(LOG_DEBUG, "Font texture size: %i %i Max texture size: %i\n", tex_w, tex_h, max_texture_size);
  }

  /* create texture */
  glActiveTexture(GL_TEXTURE0);
  glOKORDIE;
  glGenTextures(1, out_tex);
  glOKORDIE;
  glBindTexture(GL_TEXTURE_2D, *out_tex);
  glOKORDIE;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glOKORDIE;
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tex_w, tex_h, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
  glOKORDIE;

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  /* load glyphs into texture */
  {
    int i;
    for (i = START_CHAR; i < END_CHAR; ++i) {
      FT_Load_Char(face, i, FT_LOAD_RENDER);
      glTexSubImage2D(GL_TEXTURE_2D, 0, out_glyphs[i].x, out_glyphs[i].y, out_glyphs[i].w, out_glyphs[i].h, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
    }
    glOKORDIE;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);
  return tex;

  #undef START_CHAR
  #undef END_CHAR
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
    dfuns.load_image_texture_from_file = load_image_texture_from_file;
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
