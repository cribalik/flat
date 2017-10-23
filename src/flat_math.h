#define MAX(a,b) ((a) < (b) ? (b) : (a))
#include <math.h>

static int next_pow2(int x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
}

static int equalf(float a, float b) {
  return abs(a-b) < 0.00001;
}

static float sqrt_inv(float number) {
  long i;
  float x2, y;
  const float threehalfs = 1.5F;

  x2 = number * 0.5F;
  y  = number;
  i  = *(long*)&y;
  i  = 0x5f3759df - (i>>1);
  y  = *(float*)&i;
  y  = y*(threehalfs - (x2*y*y));

  return y;
}

static float lensq(float x0, float y0, float x1, float y1) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  return dx*dx + dy*dy;
}

static float length(float dx, float dy) {
  return sqrt(dx*dx + dy*dy);
}

static float length_inv(float dx, float dy) {
  return sqrt_inv(dx*dx + dy*dy);
}

static void normalize(float *x, float *y) {
  float l = length(*x, *y);
  *x /= l;
  *y /= l;
}

#define line_length(l) length((l).x1 - (l).x0, (l).y1 - (l).y0)

static float at_leastf(float x, float low) {
  return x < low ? low : x;
}

typedef struct {
  float x,y,z,w;
} v4;

typedef struct {
  float x,y,z;
} v3;

typedef struct {
  float x,y;
} v2;

typedef struct {
  int x,y;
} v2i;

static v2 v2_create(float x, float y) {
  v2 r;
  r.x = x;
  r.y = y;
  return r;
}
static v3 v3_create(float x, float y, float z) {
  v3 r;
  r.x=x;
  r.y=y;
  r.z=z;
  return r;
}

static v4 v4_create(float x, float y, float z, float w) {
  v4 r;
  r.x=x;
  r.y=y;
  r.z=z;
  r.w=w;
  return r;
}

static v2 v3_xy(v3 v) {
  return v2_create(v.x, v.y);
}

static float v2_len(v2 v) {
  return length(v.x, v.y);
}

static float v2_lensq(v2 v) {
  return v.x*v.x + v.y*v.y;
}

static v2 v2_add(v2 v, float x, float y) {
  return v2_create(v.x+x, v.y+y);
}

static v2 v2_subv(v2 v, v2 x) {
  return v2_create(v.x-x.x, v.y-x.y);
}

static int v2_equal(v2 a, v2 b) {
  return equalf(a.x, b.x) && equalf(a.y, b.y);
}

static v2 v2_mult(v2 v, float x) {
  return v2_create(v.x*x, v.y*x);
}

static v2 v2_div(v2 v, float x) {
  return v2_create(v.x/x, v.y/x);
}

static v2 v2_normalize(v2 v) {
  return v2_div(v, v2_len(v));
}

static v3 v3_add(v3 v, float x, float y, float z) {
  return v3_create(v.x+x, v.y+y, v.z+z);
}

static v2 v2i_to_v2(v2i v) {
  return v2_create(v.x, v.y);
}

typedef struct {
  float x0,y0,x1,y1;
} Rect;
typedef Rect Line;

static v2 rect_mid(Rect r) {
  return v2_create((r.x0 + r.x1)/2.0f, (r.y0 + r.y1)/2.0f);
}

static v2 rect_size(Rect r) {
  v2 v;
  v.x = r.x1 - r.x0;
  v.y = r.y1 - r.y0;
  return v;
}

static Rect rect_create(float x0, float y0, float x1, float y1) {
  Rect r;
  r.x0 = x0;
  r.y0 = y0;
  r.x1 = x1;
  r.y1 = y1;
  return r;
}

#define line_create(x0, y0, x1, y1) rect_create(x0, y0, x1, y1)

static Rect rect_offset(Rect r, v2 off) {
  r.x0 += off.x;
  r.x1 += off.x;
  r.y0 += off.y;
  r.y1 += off.y;
  return r;
}

static Rect rect_expand(Rect a, v2 size) {
  a.x0 -= size.x;
  a.x1 += size.x;
  a.y0 -= size.y;
  a.y1 += size.y;
  return a;
}

#define rect_min(r) (*(v2*)&(r).x0)
#define rect_max(r) (*(v2*)&(r).x1)

#define GET2(name) (name).x, (name).y
#define GET3(name) (name).x, (name).y, (name).z
#define GET_LINE(name) (name).x0, (name).y0, (name).x1, (name).y1
#define GET_LINEP(name) &(name).x0, &(name).y0, &(name).x1, &(name).y1
#define GET_RECT(name) GET_LINE(name)
#define GET_RECTP(name) GET_LINEP(name)

#define SET2(name, x, y) (((name).x=(x)), ((name).y=(y)))
#define SET3(name, x, y, z) (((name).x=(x)), ((name).y=(y)), ((name).z=(z)))
