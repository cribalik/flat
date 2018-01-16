#include "flat_render.hpp"

/*************/
/*** INPUT ***/
/*************/
enum Button {
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
};

struct Input {
  bool was_pressed[BUTTON_COUNT], is_down[BUTTON_COUNT];
  float lx, ly, rx, ry;
};


/********************/
/*** PLATFORM API ***/
/********************/
struct Funs {
	void *dummy;
};

#define GAME_MAIN_LOOP(name) int name(void* memory, long ms, Input input, Renderer *renderer)
typedef GAME_MAIN_LOOP((*MainLoop));

#define GAME_INIT(name) int name(void* memory, int memory_size, Funs function_ptrs, Renderer *renderer)
typedef GAME_INIT((*Init));

