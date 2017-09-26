#define MAX(a,b) ((a) < (b) ? (b) : (a))

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

static v2 v2_add(v2 v, float x, float y) {
  return v2_create(v.x+x, v.y+y);
}

static int v2_equal(v2 a, v2 b) {
  return equalf(a.x, b.x) && equalf(a.y, b.y);
}

static v2 v2_mult(v2 v, float x) {
  return v2_create(v.x*x, v.y*x);
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

static void rect_mid(Rect r, float *x, float *y) {
  *x = (r.x0 + r.x1)/2.0f;
  *y = (r.y0 + r.y1)/2.0f;
}

static v2 rect_size(Rect r) {
  v2 v;
  v.x = r.x1 - r.x0;
  v.y = r.y1 - r.y0;
  return v;
}

static Rect rect_create(float x0, float x1, float y0, float y1) {
  Rect r;
  r.x0 = x0;
  r.y0 = y0;
  r.x1 = x1;
  r.y1 = y1;
  return r;
}

static Rect rect_offset(Rect r, v2 off) {
  r.x0 += off.x;
  r.x1 += off.x;
  r.y0 += off.y;
  r.y1 += off.y;
  return r;
}

static Rect rect_expand(Rect a, Rect b) {
  float
    w = (b.x1 - b.x0)/2.0f,
    h = (b.y1 - b.y0)/2.0f;
  a.x0 -= w;
  a.x1 += w;
  a.y0 -= h;
  a.y1 += h;
  return a;
}

#define rect_min(r) (*(v2*)&(r).x0)
#define rect_max(r) (*(v2*)&(r).x1)
