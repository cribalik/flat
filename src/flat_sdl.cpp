#define _POSIX_C_SOURCE 200112L
#include "flat_math.h"
#include "flat_utils.cpp"
#include "flat_platform_api.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/* ======= Platform api ======= */

#define GL_FUN(ret, name, par) ret (GLAPI *name) par;
#include "flat_glfuns.incl"
#undef GL_FUN

static GLuint compile_shader(const char* vertex_filename, const char* fragment_filename) {
  GLuint result = 0;
  FILE *vertex_file = 0, *fragment_file = 0;
  char shader_src[2048];
  const char * const shader_src_list = shader_src;
  char info_log[512];
  int num_read;
  GLint success;
  GLuint vertex_shader, fragment_shader;

  vertex_file = flat_fopen(vertex_filename, "r");
  if (!vertex_file) die("Could not open vertex shader file %s: %s\n", vertex_filename, flat_strerror(errno));
  num_read = fread(shader_src, 1, sizeof(shader_src), vertex_file);
  shader_src[num_read] = 0;
  if (!feof(vertex_file)) die("File larger than buffer\n");
  else if (ferror(vertex_file)) die("While reading vertex shader %s: %s\n", vertex_filename, flat_strerror(errno));

  vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &shader_src_list, 0);
  glCompileShader(vertex_shader);
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(vertex_shader, sizeof(info_log), 0, info_log);
    die("Could not compile vertex shader: %s\n", info_log);
  }

  fragment_file = flat_fopen(fragment_filename, "r");
  if (!fragment_file) die("Could not open fragment shader file %s: %s\n", fragment_filename, flat_strerror(errno));
  num_read = fread(shader_src, 1, sizeof(shader_src), fragment_file);
  shader_src[num_read] = 0;
  if (!feof(fragment_file)) die("File larger than buffer\n");
  else if (ferror(fragment_file)) die("While reading fragment shader %s: %s\n", fragment_filename, flat_strerror(errno));

  fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &shader_src_list, 0);
  glCompileShader(fragment_shader);
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(fragment_shader, sizeof(info_log), 0, info_log);
    die("Could not compile fragment shader: %s\n", info_log);
  }

  result = glCreateProgram();
  glAttachShader(result, vertex_shader);
  glAttachShader(result, fragment_shader);
  glLinkProgram(result);
  glGetProgramiv(result, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramInfoLog(result, 512, 0, info_log);
    die("Could not link shader: %s\n", info_log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  fclose(vertex_file);
  fclose(fragment_file);
  gl_ok_or_die;
  return result;
}


void load_image_texture_from_file(const char* filename, GLuint* result, int* w, int* h) {
  glGenTextures(1, result);
  glBindTexture(GL_TEXTURE_2D, *result);
  gl_ok_or_die;

  {
    unsigned char *data;
    stbi_set_flip_vertically_on_load(1);
    data = stbi_load(filename, w, h, 0, 4);
    if (!data) die("Failed to load image %s: %s\n", filename, flat_strerror(errno));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    gl_ok_or_die;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gl_ok_or_die;
}

void load_font_from_file(const char* filename, GLuint gl_texture, int tex_w, int tex_h, unsigned char first_char, unsigned char last_char, float height, Glyph *out_glyphs) {
  #define BUFFER_SIZE 1024*1024
  unsigned char* ttf_mem;
  unsigned char* bitmap;
  FILE* f;
  int res;

  ttf_mem = (unsigned char*)malloc(BUFFER_SIZE);
  bitmap = (unsigned char*)malloc(tex_w * tex_h);
  if (!ttf_mem || !bitmap) die("Failed to allocate memory for font: %s\n", flat_strerror(errno));

  f = flat_fopen(filename, "rb");
  if (!f) die("Failed to open ttf file %s: %s\n", filename, flat_strerror(errno));
  fread(ttf_mem, 1, BUFFER_SIZE, f);

  res = stbtt_BakeFontBitmap(ttf_mem, 0, height, bitmap, tex_w, tex_h, first_char, last_char - first_char, (stbtt_bakedchar*) out_glyphs);
  if (res <= 0) die("Failed to bake font: %i\n", res);

  glBindTexture(GL_TEXTURE_2D, gl_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, tex_w, tex_h, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_ok_or_die;

  fclose(f);
  free(ttf_mem);
  free(bitmap);
}

/* ======= Internals ======= */

static Button key_to_button(Sint32 key) {
  switch (key) {
    case SDLK_RETURN: return BUTTON_START;
    case SDLK_t: return BUTTON_A;
    case SDLK_y: return BUTTON_B;
    case SDLK_UP: return BUTTON_UP;
    case SDLK_DOWN: return BUTTON_DOWN;
    case SDLK_LEFT: return BUTTON_LEFT;
    case SDLK_RIGHT: return BUTTON_RIGHT;
  }
  return BUTTON_NULL;
}

static i32 key_codes[] = {
  0,
  SDLK_t,
  SDLK_y,
  SDLK_r,
  SDLK_e,
  SDLK_UP,
  SDLK_DOWN,
  SDLK_LEFT,
  SDLK_RIGHT,
  SDLK_RETURN,
  SDLK_RSHIFT
};
STATIC_ASSERT(ARRAY_LEN(key_codes) == BUTTON_COUNT, assign_all_keycodes);

#define sdl_try(stmt) ((stmt) && (die("%s\n", SDL_GetError()),0))
#define sdl_abort() die("%s\n", SDL_GetError())

#ifdef OS_WINDOWS
static const char *dll_file = "flat.dll";
static const char *dll_tmp_file = "flat_tmp.dll";
#else
static const char *dll_file = "flat.so";
static const char *dll_tmp_file = "flat_tmp.so";
#endif

static void *dll_obj;

static bool copy_file(const char *filename, const char *new_filename) {
#ifdef OS_WINDOWS
  return CopyFile(filename, new_filename, false);
#else
  char buf[1024];

  FILE *a = fopen(filenmame, "rb");
  if (!a) goto err;

  FILE *b = fopen(new_filename, "wb");
  if (!b) goto err;

  while ((bytes = fread(buf, 1, sizeof(buf), a)) > 0)
    if (fwrite(buf, 1, bytes, b))
      goto err;

  fclose(a);
  fclose(b);
  return true;

  err:
  if (a) fclose(a);
  if (b) fclose(b);
  return false;
#endif
}

static void gamedll_load(MainLoop *main_loop, Init *init) {
  if (dll_obj)
    SDL_UnloadObject(dll_obj);

  copy_file(dll_file, dll_tmp_file);

  dll_obj = SDL_LoadObject(dll_tmp_file);
  if (!dll_obj)
    sdl_abort();
  *(void**)(main_loop) = SDL_LoadFunction(dll_obj, "main_loop");
  if (!main_loop)
    sdl_abort();
  *(void**)(init) = SDL_LoadFunction(dll_obj, "init");
  if (!init)
    sdl_abort();
}

static bool gamedll_has_changed() {
  #ifdef OS_WINDOWS
    static bool has_done_once;
    static FILETIME last_time;

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(!GetFileAttributesEx(dll_file, GetFileExInfoStandard, &Data))
      return false;

    if (!has_done_once) {
      has_done_once = true;
      last_time = Data.ftLastWriteTime;
      return false;
    }

    bool result = CompareFileTime(&last_time, &Data.ftLastWriteTime);
    last_time = Data.ftLastWriteTime;
    return result;
  #else
    #error "not implemented"
  #endif
}

#ifdef OS_WINDOWS
  int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
#else
  int main(int, const **char)
#endif
{
  int err;

  /* Api to game */
  #define MEMORY_SIZE 512*1024*1024
  Input input = {};

  /* Alloc memory */
  char *memory = (char*)malloc(MEMORY_SIZE);
  if (memory == NULL)
    die("Not enough memory");

  /* Alloc renderer */
  Renderer *renderer = (Renderer*)memory;
  memory += sizeof(*renderer);

  /* Fix for some builds of SDL 2.0.4, see https://bugs.gentoo.org/show_bug.cgi?id=610326 */
  #ifdef OS_LINUX
    setenv("XMODIFIERS", "@im=none", 1);
  #endif

  /* init SDL */
  sdl_try(SDL_Init(SDL_INIT_EVERYTHING));
  atexit(SDL_Quit);

  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3));
  sdl_try(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3));

  /* SDL stuff */
  const int screen_w = 1280, screen_h = 720;
  SDL_Window *window = SDL_CreateWindow("Hello world",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            screen_w, screen_h,
                            SDL_WINDOW_OPENGL);
  if (!window)
    sdl_abort();

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context)
    sdl_abort();

  /* Load gl functions */
  {
    #define GL_FUN(ret, name, par) \
      *(void**) (&name) = (void*)SDL_GL_GetProcAddress(#name); \
      if (!name) die("Couldn't load gl function "#name);
    #include "flat_glfuns.incl"
    #undef GL_FUN
  }

  glViewport(0, 0, screen_w, screen_h);

  /* Init renderer */
  {
    glEnable(GL_BLEND);
    gl_ok_or_die;
    glEnable(GL_DEPTH_TEST);
    gl_ok_or_die;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl_ok_or_die;

    /* Allocate sprite buffer */
    glGenVertexArrays(1, &renderer->sprites_vertex_array);
    glGenBuffers(1, &renderer->sprite_vertex_buffer);
    glBindVertexArray(renderer->sprites_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->sprite_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(renderer->vertices), 0, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*) 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*) offsetof(SpriteVertex, tex));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*) offsetof(SpriteVertex, normal));

    /* Allocate text buffer */
    glGenVertexArrays(1, &renderer->text_vertex_array);
    glGenBuffers(1, &renderer->text_vertex_buffer);
    glBindVertexArray(renderer->text_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->text_vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(renderer->text_vertices), 0, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(*renderer->text_vertices), (void*) 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(*renderer->text_vertices), (void*) offsetof(SpriteVertex, tex));

    /* Compile shaders */
    renderer->sprite_shader = compile_shader("../../assets/shaders/sprite_vertex.glsl", "../../assets/shaders/sprite_fragment.glsl");
    gl_ok_or_die;

    /* Get/set uniforms */
    glUseProgram(renderer->sprite_shader);
    gl_ok_or_die;
    glActiveTexture(GL_TEXTURE0);
    gl_ok_or_die;
    glBindTexture(GL_TEXTURE_2D, renderer->sprite_atlas.id);
    gl_ok_or_die;
    glUniform1i(glGetUniformLocation(renderer->sprite_shader, "u_texture"), 0);
    gl_ok_or_die;
    renderer->sprite_camera_loc = glGetUniformLocation(renderer->sprite_shader, "camera");
    renderer->sprite_far_z_loc = glGetUniformLocation(renderer->sprite_shader, "far_z");
    renderer->sprite_near_z_loc = glGetUniformLocation(renderer->sprite_shader, "near_z");
    renderer->sprite_nearsize_loc = glGetUniformLocation(renderer->sprite_shader, "nearsize");
    gl_ok_or_die;

    /* Load images into textures */
    load_image_texture_from_file( "../../assets/spritesheet.png", &renderer->sprite_atlas.id, &renderer->sprite_atlas.size.x, &renderer->sprite_atlas.size.y);
    glGenerateMipmap(GL_TEXTURE_2D);

    /* Create font texture and read in font from file */
    {
      const int w = 512, h = 512;
      glGenTextures(1, &renderer->text_atlas.id);
      glBindTexture(GL_TEXTURE_2D, renderer->text_atlas.id);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl_ok_or_die;
      renderer->text_atlas.size.x = w;
      renderer->text_atlas.size.y = h;

      load_font_from_file( "../../assets/Roboto-Regular.ttf", renderer->text_atlas.id, renderer->text_atlas.size.x, renderer->text_atlas.size.y, RENDERER_FIRST_CHAR, RENDERER_LAST_CHAR, RENDERER_FONT_SIZE, renderer->glyphs);
      glBindTexture(GL_TEXTURE_2D, renderer->text_atlas.id);
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  }


  /* load game loop */
  MainLoop main_loop;
  Init init;
  gamedll_load(&main_loop, &init);

  /* call init */
  {
    Funs dfuns = {};
    init(memory, MEMORY_SIZE, dfuns, renderer);
  }


  /* main loop */
  unsigned int loop_index = 0;
  for (;; ++loop_index) {
    int i;
    SDL_Event event;

    for (i = 0; i < ARRAY_LEN(input.was_pressed); ++i)
      input.was_pressed[i] = false;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_WINDOWEVENT:
          if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            return 0;
          break;

        case SDL_KEYDOWN: {
          if (event.key.repeat)
            break;
          Button b = key_to_button(event.key.keysym.sym);
          input.was_pressed[b] = true;
          input.is_down[b] = true;
        } break;

        case SDL_KEYUP: {
          if (event.key.repeat)
            break;
          input.is_down[key_to_button(event.key.keysym.sym)] = false;
        } break;
      }
    }

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl_ok_or_die;

    static int last_time;
    if ((loop_index%100) == 0 && gamedll_has_changed())
      gamedll_load(&main_loop, &init);
    err = main_loop(memory, SDL_GetTicks(), input, renderer);
    if (err) return 0;


    /* send camera position to shader */
    {
      float w,h, far_z,near_z, fov;
      fov = PI/3.0f;
      far_z = -50.0f;
      near_z = -2.0f;
      w = - 2.0f * near_z * (float)tan(fov/2.0f);
      h = w * 9.0f / 16.0f;
      glUniform3f(renderer->sprite_camera_loc, GET3(renderer->camera_pos));
      glUniform1f(renderer->sprite_far_z_loc, far_z);
      glUniform1f(renderer->sprite_near_z_loc, near_z);
      glUniform2f(renderer->sprite_nearsize_loc, w, h);
    }
    gl_ok_or_die;

    /* draw sprites */
    glBindVertexArray(renderer->sprites_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->sprite_vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_vertices*sizeof(*renderer->vertices), renderer->vertices);
    glBindTexture(GL_TEXTURE_2D, renderer->sprite_atlas.id);
    glDrawArrays(GL_TRIANGLES, 0, renderer->num_vertices);
    gl_ok_or_die;

    /* draw text */
    glBindVertexArray(renderer->text_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->text_vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, renderer->num_text_vertices*sizeof(*renderer->text_vertices), renderer->text_vertices);
    glBindTexture(GL_TEXTURE_2D, renderer->text_atlas.id);
    glDrawArrays(GL_TRIANGLES, 0, renderer->num_text_vertices);
    gl_ok_or_die;

    SDL_GL_SwapWindow(window);
  }
}
