GLuint compileShader(const char* vertexShaderSrc, const char* fragmentShaderSrc) {
  GLuint vertexShader, fragmentShader, shaderProgram;
  vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSrc, 0);
  glCompileShader(vertexShader);
  {
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(vertexShader, 512, 0, infoLog);
      fprintf(stderr, "Failed to compile vertex shader: %s\n", infoLog);
      abort();
    }
  }

  fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSrc, 0);
  glCompileShader(fragmentShader);
  {
    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(fragmentShader, 512, 0, infoLog);
      fprintf(stderr, "Failed to compile fragment shader: %s\n", infoLog);
      abort();
    }
  }

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, fragmentShader);
  glAttachShader(shaderProgram, vertexShader);
  glLinkProgram(shaderProgram);

  {
    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shaderProgram, 512, 0, infoLog);
      fprintf(stderr, "Failed to compile shader: %s\n", infoLog);
      abort();
    }
  }

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
  glOKORDIE;
  return shaderProgram;
}
