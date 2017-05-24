#version 330 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tpos;

out vec2 f_tpos;

void main() {
  gl_Position = vec4(pos, 0, 1);
  f_tpos = tpos;
}
