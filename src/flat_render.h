struct Texture {
  GLuint id;
  v2i size;
};

struct SpriteVertex {
  v3 pos;
  v2 tex;
  v3 normal;
};

static SpriteVertex spritevertex_create(float x, float y, float z, float tx, float ty) {
  SpriteVertex result;
  result.pos.x = x;
  result.pos.y = y;
  result.pos.z = z;
  result.tex.x = tx;
  result.tex.y = ty;
  return result;
}
typedef SpriteVertex TextVertex;

struct Glyph {
  unsigned short x0, y0, x1, y1; /* Position in image */
  float offset_x, offset_y, advance; /* Glyph offset info */
};

struct Renderer {
  /* sprites */
  GLuint
    sprites_vertex_array, sprite_vertex_buffer,
    sprite_shader,
    /* locations */
    sprite_camera_loc,
    sprite_far_z_loc,
    sprite_near_z_loc,
    sprite_nearsize_loc
    ;
  Texture sprite_atlas;

  SpriteVertex vertices[256];
  int num_vertices;

  /* text */
  #define RENDERER_FIRST_CHAR 32
  #define RENDERER_LAST_CHAR 128
  #define RENDERER_FONT_SIZE 32.0f
  GLuint text_vertex_array, text_vertex_buffer;
  TextVertex text_vertices[1024];
  int num_text_vertices;
  Texture text_atlas;
  Glyph glyphs[RENDERER_LAST_CHAR - RENDERER_FIRST_CHAR];

  /* camera */
  #define RENDERER_CAMERA_HEIGHT 5
  v3 camera_pos;
};

