#include <stdio.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include "GL/glext.h"

#define STATIC_ASSERT(expr, name) typedef char static_assert_##name[expr?1:-1]

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

typedef enum {
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
#define foreach_button(i) for (i = 1; i < BUTTON_COUNT; ++i)

typedef struct {
  char was_pressed[BUTTON_COUNT], is_down[BUTTON_COUNT];
  float lx, ly, rx, ry;
} Input;

typedef struct {
  unsigned short s0, t0, s1, t1; /* Position in image */
  float offset_x, offset_y, advance; /* Glyph offset info */
} Glyph;

typedef void (*LoadImageTextureFromFile)(const char* filename, GLuint* result, int* w, int* h);
typedef void (*LoadFontFromFile)(const char* filename, GLuint gl_texture, int w, int h, unsigned char first_char, unsigned char last_char, float height, Glyph *out_glyphs);
typedef struct {
  LoadImageTextureFromFile load_image_texture_from_file;
  LoadFontFromFile load_font_from_file;
} Funs;

typedef int (*MainLoop)(void* memory, long ms, Input input);
typedef int (*Init)(void* memory, int memory_size, Funs function_ptrs);
