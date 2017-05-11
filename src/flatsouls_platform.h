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
	BUTTON_NULL =   0,
  BUTTON_A =      1<<0,
  BUTTON_B =      1<<1,
  BUTTON_X =      1<<2,
  BUTTON_Y =      1<<3,
  BUTTON_UP =     1<<4,
  BUTTON_DOWN =   1<<5,
  BUTTON_LEFT =   1<<6,
  BUTTON_RIGHT =  1<<7,
  BUTTON_START =  1<<8,
  BUTTON_SELECT = 1<<9
} Button;

typedef struct {
  u16 was_pressed;
  u16 is_down;
  float lx, ly, rx, ry;
} Input;
