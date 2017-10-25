#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tpos;

out vec2 f_tpos;
uniform vec3 camera;
uniform float near_z;
uniform float far_z;
uniform vec2 nearsize;

void main() {
  // translate to camera
  vec3 p = pos - camera;

  // normalize to frustum
  p.x *= near_z / p.z / nearsize.x * 2;
  p.y *= near_z / p.z / nearsize.y * 2;
  p.z = (p.z - near_z) / (far_z - near_z) * 2 - 1;

  // output
  gl_Position = vec4(p, 1);
  f_tpos = tpos;
}
