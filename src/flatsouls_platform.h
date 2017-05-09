typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long i64;

typedef struct {
  int w,h;
  unsigned char* buf;
} Bitmap;

typedef enum {
  BUTTON_A =      1<<0,
  BUTTON_B =      1<<1,
  BUTTON_X =      1<<2,
  BUTTON_Y =      1<<3,
  BUTTON_START =  1<<4,
  BUTTON_SELECT = 1<<5,
} Button;

typedef struct {
  u8 was_pressed;
  u8 is_down;
  float lx, ly, rx, ry;
} Input;