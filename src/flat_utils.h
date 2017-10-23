#include "acorns/utils.h"
#include "acorns/array.h"
#include <stdio.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <stdarg.h>

#define die printf("%s:%i: error: ", __FILE__, __LINE__),_die
static void _die(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fflush(stderr);
  abort();
}

#define gl_ok_or_die _gl_ok_or_die(__FILE__, __LINE__)
static void _gl_ok_or_die(const char* file, int line) {
  GLenum error_code;
  const char* error;

  error_code = glGetError();

  if (error_code == GL_NO_ERROR) return;

  switch (error_code) {
    case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
    case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
    case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
    case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
    case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
    case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
    case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
    default: error = "unknown error";
  };
  fprintf(stderr, "GL error at %s:%u: (%u) %s\n", file, line, error_code, error);
  exit(1);
}

typedef enum Button {
  BUTTON_NULL,
  BUTTON_A,
  BUTTON_B,
  BUTTON_X,
  BUTTON_Y,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_START,
  BUTTON_SELECT,
  BUTTON_COUNT
} Button;

#define ENUM_FOREACH(i, name) for (i = name##_NULL+1; i < name##_COUNT; ++i)
#define ENUM_CHECK(name, value) ((value <= name##_NULL || value >= name##_COUNT) ? die("Enum " #name " out of range (%i)\n", value), 0 : 0)

typedef struct Input {
  char was_pressed[BUTTON_COUNT], is_down[BUTTON_COUNT];
  float lx, ly, rx, ry;
} Input;

typedef struct Glyph {
  unsigned short x0, y0, x1, y1; /* Position in image */
  float offset_x, offset_y, advance; /* Glyph offset info */
} Glyph;

typedef void (*LoadImageTextureFromFile)(const char* filename, GLuint* result, int* w, int* h);
typedef void (*LoadFontFromFile)(const char* filename, GLuint gl_texture, int w, int h, unsigned char first_char, unsigned char last_char, float height, Glyph *out_glyphs);
typedef struct Funs {
  LoadImageTextureFromFile load_image_texture_from_file;
  LoadFontFromFile load_font_from_file;
} Funs;

typedef int (*MainLoop)(void* memory, long ms, Input input);
typedef int (*Init)(void* memory, int memory_size, Funs function_ptrs);
