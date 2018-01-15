#ifdef _MSC_VER
  #define OS_WINDOWS 1
#else
  #define OS_LINUX 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "flat_gl.h"

#include <stdarg.h>





/**************/
/*** @ARRAY ***/
/**************/


/* For annotating array types, since they just look like pointers otherwise */
#define Array(type) type*

#define array_insert(a, i, x) (array_resize((a), array_len(a)+1), memmove((a)+(i)+1, (a)+(i), (array__n(a) - (i)) * sizeof(*(a))), (a)[i] = (x))
#define array_insert_n(a, i, n) (array_resize((a), array_len(a)+(n)), memmove((a)+(i)+(n), (a)+(i), (array__n(a)-(n)-(i)) * sizeof(*(a))))
#define array_insert_a(a, i, val, n) (array_resize((a), array_len(a)+(n)), memmove((a)+(i)+(n), (a)+(i), (array__n(a)-(n)-(i)) * sizeof(*(a))), memcpy((a)+(i), val, (n)*sizeof(*(val))))
#define array_len(a) ((a) ? array__n(a) : 0)
#define array_len_get(a) (array__n(a))
#define array_push(a, val) ((!(a) || array__n(a) == array__c(a) ? (a)=array__grow(a, sizeof(*(a)), 1) : 0), (a)[array__n(a)++] = val)
#define array_push_a(a, val, n) (array_resize(a, array_len(a)+(n)), memcpy((a)+array_len(a)-(n), val, n * sizeof(*(val))))
#define array_push_n(a, n) ((!(a) || array__n(a)+(n) >= array__c(a) ? (a)=array__grow(a, sizeof(*(a)), (n)) : 0), array__n(a) += (n))
#define array_last(a) (!(a) ? 0 : (a)+array__n(a)-1)
#define array_free(a) (((a) ? free(&array__n(a)),0 : 0), (a) = 0)
#define array_end(a) (!(a) ? 0 : (a)+array__n(a))
#define array_cap(a) ((a) ? array__c(a) : 0)
#define array_resize(a, n) ((n) > array_len(a) ? array_push_n(a, (n) - array_len(a)) : ((a) ? (array__n(a) = (n)) : 0))
#define array_reserve(a, n) ((n) > array_len(a) ? array_resize((a), (n)) : 0)
/* Preserves ordering */
#define array_remove_slow(a, i) ((a) && array__n(a) > 0 ? memmove((a)+(i), (a)+(i)+1, sizeof(*(a)) * (array__n(a)-i-1)), --array__n(a) : 0)
#define array_remove_slow_n(a, i, n) ((a) && array__n(a) > 0 ? memmove((a)+(i), (a)+(i)+(n), sizeof(*(a)) * (array__n(a)-i-(n))), array__n(a)-=(n) : 0)
/* Swaps in the last element */
#define array_remove_fast(a, i) ((a) && array__n(a) > 0 ? (a)[i] = (a)[--array__n(a)],0 : 0)

#define array_foreach(a, ptr) for ((ptr) = (a); (ptr) && (ptr) < (a)+array_len(a); ++(ptr))
#define array_find(a, ptr, expr) {for ((ptr) = (a); (ptr) && (ptr) < (a)+array_len(a); ++(ptr)) {if (expr) break;} if ((ptr) == (a)+array_len(a)) {(ptr) = 0;}}

/* Internals */
#define array__c(a) ((int*)(a))[-1]
#define array__n(a) ((int*)(a))[-2]

static void* array__grow(void* a, int size, int num) {
  /* TODO: ensure we are always power of 2 */
  const int INITIAL_SIZE = 4;
  int newc = a ? (num + array__n(a) > array__c(a)*2 ? num + array__n(a) : array__c(a)*2) : (num > INITIAL_SIZE ? num : INITIAL_SIZE);
  int n = a ? array__n(a) : 0;
  a = (int*)realloc(a ? &array__n(a) : 0, newc*size + 2*sizeof(int)) + 2;
  array__n(a) = n;
  array__c(a) = newc;
  return a;
}





/**************/
/*** @UTILS ***/
/**************/
#define STATIC_ASSERT(expr, name) typedef char static_assert_##name[expr?1:-1]
#define ALIGN(x, val) ALIGN_MASK(x, val-1)
#define ALIGN_MASK(x, mask) (((x)+(mask)) & ~(mask))
#define CONTAINEROF(ptr, type, member) (((type)*)((char*)ptr - offsetof(type, member)))
#define ALIGNOF(type) offsetof(struct {char a; type b;}, b)
#define ARRAY_LEN(a) ((int)(sizeof(a)/sizeof(*a)))
#define ENUM_FOREACH(i, name) for (i = name##_NULL+1; i < name##_COUNT; ++i)
#define ENUM_CHECK(name, value) ((value <= name##_NULL || value >= name##_COUNT) ? die("Enum " #name " out of range (%i)\n", value), 0 : 0)



/*************************/
/*** @FIXED WIDTH TYPES ***/
/*************************/
#ifdef OS_WINDOWS
  typedef __int8 i8;
  typedef __int16 i16;
  typedef __int32 i32;
  typedef __int64 i64;
  typedef unsigned __int8 u8;
  typedef unsigned __int16 u16;
  typedef unsigned __int32 u32;
  typedef unsigned __int64 u64;
#else
  /* let's hope stdint has us covered */
  #include <stdint.h>
  typedef int8_t i8;
  typedef int16_t i16;
  typedef int32_t i32;
  typedef int64_t i64;
  typedef uint8_t u8;
  typedef uint16_t u16;
  typedef uint32_t u32;
  typedef uint64_t u64;
#endif

typedef union {i64 a; u64 b; double c;} aligned_t;


STATIC_ASSERT(sizeof(i8) == 1, i8_is_1_byte);
STATIC_ASSERT(sizeof(i16) == 2, i16_is_2_bytes);
STATIC_ASSERT(sizeof(i32) == 4, i32_is_4_bytes);
STATIC_ASSERT(sizeof(i64) == 8, i64_is_8_bytes);
STATIC_ASSERT(sizeof(u8) == 1, u8_is_1_byte);
STATIC_ASSERT(sizeof(u16) == 2, u16_is_2_bytes);
STATIC_ASSERT(sizeof(u32) == 4, u32_is_4_bytes);
STATIC_ASSERT(sizeof(u64) == 8, u64_is_8_bytes);



/*******************/
/*** @ALLOCATION ***/
/*******************/
/**
 * Which memory allocator should you use?
 *
 *   [Bound on number of items]  --no->   [Same size items?]  --no->   [Out of order free?]  --yes->   (malloc)
 *                | yes                          | yes                         | no
 *                |                              v                             v
 *                |                          (LBlock)                       (LStack)
 *                |
 *        [Same size items]  --no->  (Stack)
 *                | yes
 *                v
 *             (Block)                   
 */


/**
 * Stack allocator
 *
 * A static-size stack. Supports push and pops
 *
 * order-constraint: stack
 * ptr-validity: yes
 */

struct Stack {
  unsigned char *begin, *end, *curr;
};

static int stack_init(Stack *stack, void *mem, long size);
#define stack_clear(stack) ((stack)->curr = (stack)->begin)
#define stack_push(stack, type) (stack_push_ex(stack, sizeof(type), alignof(type)))
#define stack_push_val(stack, type, val) (stack__has_size(stack, type) ? (*(type*)stack_push(stack, type)=(val), (stack)->curr - sizeof(type)) : 0)
#define stack_pop(stack, to) ((stack)->curr = (unsigned char*)(to))



/**
 * LStack - a linked list of Stack allocators
 *
 * Supports the same operations and guarantees as a stack
 * Uses malloc for new stacks
 */

struct LStack {
  Stack stack;
  long block_size;
};

/* The size is the stack size, NOT the size of the blocks in the linked list.
 * If you for example want blocks of size 4096,
 * use lstack_size_from_blocksize when calling lstack_init */
static int lstack_init(LStack *ls, long size);
static int lstack__newblock(LStack *ls);
#define lstack_push(ls, type) (lstack_push_ex(ls, sizeof(type), alignof(type)))
static void lstack_pop(LStack *ls, void *to);
static void lstack_clear(LStack *ls);

#define lstack_size_from_blocksize(size) ((size)-sizeof(Stack))



/**
 * Block allocator
 *
 * Divides a fixed-size memory area into fixed-size blocks
 *
 * order-constraint: none
 * ptr_validity: yes
 *
 * constraints:
 *   Because the next-pointers are stored in the unused items, the item size must be at least sizeof(void*),
 *   and the memory given is properly aligned for both the intended type, and void*
 */

struct Block {
  unsigned char *next;
  long item_size;
};

static int block_init(Block *block, void *mem, long num_items, long item_size);
static int block_add_block(Block *block, void *mem, long num_items);
static void* block_get(Block *block);
static void block_put(Block *block, void *at);


/**
 * LBlock - a linked list of Block allocators
 *
 * Supports the same operations and guarantees as a block allocator
 * Uses malloc for new list items
 */

struct LBlock {
  Block block;
  long num_items;
};

/* The size is the stack size, NOT the size of the blocks in the linked list.
 * If you for example want blocks of size 4096,
 * use lblock_size_from_blocksize when calling lblock_init */
static int lblock_init(LBlock *lb, long num_items, long item_size);
static void* lblock_get(LBlock *lb);
static void lblock_put(LBlock *lb, void *at);

#define lblock_size_from_blocksize(size) ((size)-sizeof(Stack))


/**
 * errors
 */

extern int mem_errno;
enum {
  MEM_FULL = 1,
  MEM_INVALID_ALIGN,
  MEM_INVALID_ARG
};


#define stack__has_size(stack, type) ((char*)mem__align((stack)->curr, alignof(type)) + sizeof(type) <= (char*)(stack)->end)
static void* stack_push_ex(Stack *stack, long size, int align);
static void* stack__push_val(Stack *stack, long size, int align, void *ptr);
static void stack__pop(Stack *stack, long size, int align);

static void* lstack_push_ex(LStack *stack, long size, int align);
static int lstack_num_blocks(LStack *stack);

static void* lblock_push_ex(LBlock *lb, long size, int align);

int mem_errno;

#define mem__align(x, val) (void*)(((long)(x)+((val)-1)) & ~((val)-1))

/* STACK IMPLEMENTATION */

static int stack_init(Stack *stack, void *mem, long size) {
  if (!stack || !mem || !size)
    return mem_errno = MEM_INVALID_ARG;

  stack->begin = stack->curr = (unsigned char*)mem;
  stack->end = stack->begin + size;

  return 0;
}

static void* stack_push_ex(Stack *stack, long size, int align) {
  stack->curr = (unsigned char*)mem__align(stack->curr, align);
  if (stack->curr + size > stack->end) {
    mem_errno = MEM_FULL;
    return 0;
  }

  stack->curr += size;

  return stack->curr - size;
}

static void* stack__push_val(Stack *stack, long size, int align, void *ptr) {
  unsigned char *p;

  p = (unsigned char*)stack_push_ex(stack, size, align);
  if (!p)
    return 0;

  memcpy(p, ptr, size);
  return p;
}

/* LSTACK IMPLEMENTATION */

static int lstack_init(LStack *ls, long size) {
  ls->block_size = size + sizeof(Stack);

  memset(&ls->stack, 0, sizeof(ls->stack));
  return lstack__newblock(ls);
}


static int lstack__newblock(LStack *ls) {
  void *m;
  int err;
  Stack s;

  s = ls->stack;

  m = malloc(ls->block_size);
  if (!m)
    return mem_errno = MEM_FULL;

  err = stack_init(&ls->stack, m, ls->block_size);
  if (err)
    return err;

  if (!stack_push_val(&ls->stack, Stack, s))
    return mem_errno = MEM_INVALID_ARG;

  return 0;
}

static void* lstack_push_ex(LStack *ls, long size, int align) {
  void *p;

  p = stack_push_ex(&ls->stack, size, align);
  if (p)
    return p;
  if (mem_errno != MEM_FULL)
    return 0;

  if (!lstack__newblock(ls))
    return 0;

  return p;
}

static void lstack_pop(LStack *ls, void *to) {
  Stack *s;
  unsigned char *t;

  s = &ls->stack;
  t = (unsigned char*)to;
  while (t < s->begin || t >= s->end) {
    unsigned char *tmp;

    tmp = s->begin;
    *s = *(Stack*)s->begin;
    free(tmp);
  }

  s->end = (unsigned char*)to;
}

static void lstack_clear(LStack *ls) {
  lstack_pop(ls, ls->stack.begin);
}

static int lstack_num_blocks(LStack *ls) {
  Stack *s;
  int c;

  s = &ls->stack;
  for (c = 0; s->begin; ++c)
    s = (Stack*)s->begin;

  return c;
}

/* BLOCK IMPLEMENTATION */

static int block_init(Block *block, void *mem, long num_items, long item_size) {
  int err;

  if (item_size < (long)sizeof(void*))
    return mem_errno = MEM_INVALID_ARG;
  block->item_size = item_size;
  block->next = 0;

  err = block_add_block(block, mem, num_items);
  if (err)
    return err;

  return 0;
}

static void* block_get(Block *block) {
  void *p;

  p = block->next;
  if (!p) {
    mem_errno = MEM_FULL;
    return 0;
  }

  block->next = (unsigned char*)*(void**)p;
  return p;
}

static void block_put(Block *block, void *at) {
  *(void**)at = block->next;
  block->next = (unsigned char*)at;
}

static int block_add_block(Block *block, void *mem, long num_items) {
  int i;
  unsigned char *p;

  if ((long)mem & (alignof(void*)-1))
    return mem_errno = MEM_INVALID_ALIGN;

  /* set up chain pointers */
  for (p = (unsigned char*)mem, i = 0; i < num_items-1; ++i, p += block->item_size)
    *(void**)p = p+block->item_size;
  *(void**)p = block->next;

  block->next = (unsigned char*)mem;
  return 0;
}

/* LBLOCK IMPLEMENTATION */

static int lblock_init(LBlock *lb, long num_items, long item_size) {
  void *m;

  lb->num_items = num_items;

  m = malloc(lb->num_items * item_size);
  if (!m)
    return mem_errno = MEM_FULL;

  return block_init(&lb->block, m, num_items, item_size);
}

static void* lblock_get(LBlock *lb) {
  void *p;
  int err;

  p = block_get(&lb->block);
  if (p)
    return p;
  if (mem_errno != MEM_FULL)
    return 0;

  p = malloc(lb->num_items * lb->block.item_size);
  if (!p) {
    mem_errno = MEM_FULL;
    return 0;
  }

  err = block_add_block(&lb->block, p, lb->num_items);
  if (err)
    return 0;

  return block_get(&lb->block);
}








/***************/
/*** @LOGGING ***/
/***************/
static FILE* get_log_file() {
  static FILE *f;
  if (!f)
    f = fopen("log.txt", "a");
  if (!f)
    abort();
  return f;
}
#define die fprintf(get_log_file(), "%s:%i: error: ", __FILE__, __LINE__),_die
static void _die(const char* fmt, ...) {
  FILE *f = get_log_file();

  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
  fflush(f);
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
  die("GL error at %s:%u: (%u) %s\n", file, line, error_code, error);
}





