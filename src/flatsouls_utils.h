#include <stdio.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <stdarg.h>

#define STATIC_ASSERT(expr, name) typedef char static_assert_##name[expr?1:-1]

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

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long i64;

STATIC_ASSERT(sizeof(u8) == 1, u8_is_8_bits);
STATIC_ASSERT(sizeof(u16) == 2, u16_is_16_bits);
STATIC_ASSERT(sizeof(u32) == 4, u32_is_32_bits);
STATIC_ASSERT(sizeof(u64) == 8, u64_is_64_bits);
STATIC_ASSERT(sizeof(long) == sizeof(void*), ptr_is_long);

#define ARRAY_LEN(a) (sizeof(a)/sizeof(*a))

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

#define FOREACH_ENUM(i, name) for (i = (&i == (int*)0)+1; i < name##_COUNT; ++i)
#define CHECK_ENUM(name, value) ((value <= name##_NULL || value >= name##_COUNT) ? die("Enum " #name " out of range (%i)\n", value), 0 : 0)

typedef struct Input {
  char was_pressed[BUTTON_COUNT], is_down[BUTTON_COUNT];
  float lx, ly, rx, ry;
} Input;

typedef struct Glyph {
  unsigned short s0, t0, s1, t1; /* Position in image */
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
