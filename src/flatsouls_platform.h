#include <stdio.h>
#include <stdlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include "GL/glext.h"
#if 0
#define GLFUN(name, proto) extern proto;
#include "flatsouls_gl.incl"
#undef GLFUN
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long i64;

typedef enum {
  BUTTON_A =      0,
  BUTTON_B =      1,
  BUTTON_X =      2,
  BUTTON_Y =      3,
  BUTTON_UP =     4,
  BUTTON_DOWN =   5,
  BUTTON_LEFT =   6,
  BUTTON_RIGHT =  7,
  BUTTON_START =  8,
  BUTTON_SELECT = 9
} Button;
#define NUM_BUTTONS 10

typedef struct {
  char was_pressed[NUM_BUTTONS], is_down[NUM_BUTTONS];
  float lx, ly, rx, ry;
} Input;

typedef struct {
  /* Position in image */
  unsigned short s0, t0, s1, t1;
  /* Glyph offset info */
  float offset_x, offset_y, advance;
} Glyph;

typedef void (*LoadImageTextureFromFile)(const char* filename, GLuint* result, int* w, int* h);
typedef void (*LoadFontFromFile)(const char* filename, GLuint gl_texture, int w, int h, Glyph *out_glyphs);
typedef struct {
  LoadImageTextureFromFile load_image_texture_from_file;
  LoadFontFromFile load_font_from_file;
} Funs;

typedef int (*MainLoop)(void* memory, long ms, Input input);
typedef int (*Init)(void* memory, int memory_size, Funs function_ptrs);
