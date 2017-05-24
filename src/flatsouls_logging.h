#include <stdarg.h>
#define LOG_ERROR __FILE__,__LINE__,1
#define LOG_DEBUG __FILE__,__LINE__,2
void LOG(const char* file, int line, int type, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s:%i %s: ", file, line, type == 1 ? "error" : "debug");
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void LOG_AND_ABORT(const char* file, int line, int type, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s:%i %s: ", file, line, type == 1 ? "error" : "debug");
  vfprintf(stderr, fmt, args);
  va_end(args);
  abort();
}


#define glOKORDIE _glOkOrDie(__FILE__, __LINE__)
static void _glOkOrDie(const char* file, int line) {
  GLenum error_code = glGetError();
  if (error_code != GL_NO_ERROR) {
    const char* error;
    switch (error_code)
    {
      case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
      case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
      case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
      case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
      case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
      case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
      case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
      default: error = "unknown error";
    };
    fprintf(stderr, "GL error at %s:%u. Code: %u - %s\n", file, line, error_code, error);
    exit(1);
  }
}