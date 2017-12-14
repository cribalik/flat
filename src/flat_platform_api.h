/********************/
/*** PLATFORM API ***/
/********************/
typedef struct Funs {
	void *dummy;
} Funs;

#define GAME_MAIN_LOOP(name) int name(void* memory, long ms, Input input, Renderer *renderer)
typedef GAME_MAIN_LOOP((*MainLoop));

#define GAME_INIT(name) int name(void* memory, int memory_size, Funs function_ptrs)
typedef GAME_INIT((*Init));
