#version 330 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec2 tpos;

out vec2 f_tpos;
uniform vec3 camera;

void main() {
  // calculate distance between position and camera
  const float FAR_Z = -10;
  const float NEAR_Z = -0.5;
  // TODO: change near size
  // const float NEAR_VIEW_WIDTH = 1;
  // const float NEAR_VIEW_HEIGHT = 1;

  vec3 p = pos - camera;

  float scale = NEAR_Z / p.z;
  p.x *= scale;
  p.y *= scale;

  // normalize z to -1,1
  p.z = (p.z - NEAR_Z) / (FAR_Z - NEAR_Z) * 2 - 1;

  gl_Position = vec4(p, 1);
  f_tpos = tpos;
}
