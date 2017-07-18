#include <stdarg.h>

#define LOG_LEVEL_DEBUG 1
#define LOG_LEVEL_RELEASE 2

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#define LOG_DEBUG __FILE__,__LINE__,LOG_LEVEL_DEBUG
#define LOG_ERROR __FILE__,__LINE__,LOG_LEVEL_RELEASE
static void LOG(const char* file, int line, int type, const char* fmt, ...) {
  va_list args;
  if (LOG_LEVEL > type) return;
  va_start(args, fmt);
  fprintf(stderr, "%s:%i %s: ", file, line, type == 1 ? "error" : "debug");
  vfprintf(stderr, fmt, args);
  va_end(args);
}

static void LOG_AND_ABORT(const char* file, int line, int type, const char* fmt, ...) {
  va_list args;
  if (LOG_LEVEL > type) return;
  va_start(args, fmt);
  fprintf(stderr, "%s:%i %s: ", file, line, type == 1 ? "error" : "debug");
  vfprintf(stderr, fmt, args);
  va_end(args);
  fflush(stderr);
  abort();
}

#define glOKORDIE _glOkOrDie(__FILE__, __LINE__)
static void _glOkOrDie(const char* file, int line) {
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
  fprintf(stderr, "GL error at %s:%u: (%u) %s\n", file, line, error_code, error);
  exit(1);
}
