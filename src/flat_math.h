#ifndef FLAT_MATH_H
#define FLAT_MATH_H

template<class T>
T max(T a, T b) {
  return a < b ? b : a;
}

template<class T>
T min(T a, T b) {
  return b < a ? b : a;
}

#include <math.h>

#define PI 3.14159265f

static unsigned int randint(unsigned int *r) {
  return *r = 1103515245 * *r + 12345;
}

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

static bool equalf(float a, float b) {
  return fabs(a-b) < 0.00001;
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

static float length(float dx, float dy, float dz) {
  return sqrt(dx*dx + dy*dy + dz*dz);
}

static float sign(float x) {
  return x < 0.0f ? -1.0f : 1.0f;
}

static float length_inv(float dx, float dy) {
  return sqrt_inv(dx*dx + dy*dy);
}

static void normalize(float *x, float *y) {
  float l;
  if (*x == 0.0f && *y == 0.0f)
    return;
  l = length(*x, *y);
  *x /= l;
  *y /= l;
}

static void normalize(float *x, float *y, float *z) {
  float l;
  if (*x == 0.0f && *y == 0.0f && *z == 0.0f)
    return;
  l = length(*x, *y, *z);
  *x /= l;
  *y /= l;
  *z /= l;
}

#define line_length(l) length((l).x1 - (l).x0, (l).y1 - (l).y0)

#define at_least(a,b) max(a,b)

struct v2 {
  float x,y;

};

union v3 {
  struct {
    float x,y,z;
  };
  struct {
    v2 xy;
    float _;
  };
};

union v4 {
  struct {
    float x,y,z,w;
  };
  struct {
    v3 xyz;
    float _;
  };
};

union v2i {
  int x,y;
};

static v3 operator-(v3 a, v3 b) {
  return {a.x-b.x, a.y-b.y, a.z-b.z};
}

static v3 normalize(v3 v) {
  normalize(&v.x, &v.y, &v.z);
  return v;
}

static v3 cross(v3 a, v3 b) {
  v3 r = {
    a.y*b.z - a.z*b.y,
    a.z*b.x - a.x*b.z,
    a.x*b.y - a.y*b.x
  };
  return r;
}

static float length(v2 v) {
  return length(v.x, v.y);
}

static float lensq(v2 v) {
  return v.x*v.x + v.y*v.y;
}

static v2 add(v2 v, float x, float y) {
  return {v.x+x, v.y+y};
}

static v2 operator+(v2 a, v2 b) {
  return {a.x+b.x, a.y+b.y};
}

static v2 operator-(v2 a, v2 b) {
  return {a.x-b.x, a.y-b.y};
}

static bool operator==(v2 a, v2 b) {
  return equalf(a.x, b.x) && equalf(a.y, b.y);
}

static v2 operator*(v2 v, float x) {
  return {v.x*x, v.y*x};
}
static v2 operator*(float x, v2 v) {
  return v*x;
}

static v2 operator/(v2 v, float x) {
  return {v.x/x, v.y/x};
}

static v2 normalize(v2 v) {
  return v/length(v);
}

static v3 operator+(v3 a, v3 b) {
  return {a.x+b.x, a.y+b.y, a.z+b.z};
}

static v3 add(v3 v, float x, float y, float z) {
  return {v.x+x, v.y+y, v.z+z};
}

static float length(v3 v) {
  return length(v.x, v.y, v.z);
}

static v2 v2i_to_v2(v2i v) {
  return {(float)v.x, (float)v.y};
}

struct Rect {
  float x0,y0,x1,y1;
};
typedef Rect Line;

struct Cube {
  float x0,y0,z0,x1,y1,z1;
};

static Cube cube_create(float x0, float y0, float z0, float x1, float y1, float z1) {
  Cube c = {x0, y0, z0, x1, y1, z1};
  return c;
}

static v2 rect_mid(Rect r) {
  return {(r.x0 + r.x1)/2.0f, (r.y0 + r.y1)/2.0f};
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
#define GET2P(name) &((name).x), &((name).y)
#define GET3(name) (name).x, (name).y, (name).z
#define GET_LINE(name) (name).x0, (name).y0, (name).x1, (name).y1
#define GET_LINEP(name) &(name).x0, &(name).y0, &(name).x1, &(name).y1
#define GET_RECT(name) GET_LINE(name)
#define GET_RECTP(name) GET_LINEP(name)

#define SET2(name, x, y) (((name).x=(x)), ((name).y=(y)))
#define SET3(name, x, y, z) (((name).x=(x)), ((name).y=(y)), ((name).z=(z)))

#endif /* FLAT_MATH_H */