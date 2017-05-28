#version 330 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 tpos;

out vec2 f_tpos;
uniform vec3 camera;

void main() {
  // calculate distance between position and camera
  const float VIEW_DISTANCE = 9;
  const float NEAR_DISTANCE = 0.3;
  vec3 p = vec3(pos, 0);
  float NEAR = camera.z - NEAR_DISTANCE;
  float FAR = camera.z - NEAR_DISTANCE - VIEW_DISTANCE;
  float mult = NEAR / (camera.z - p.z);
  p -= camera;
  p.x *= mult;
  p.y *= mult;
  p.z = ((p.z - NEAR) / VIEW_DISTANCE)*2 + 1;

  gl_Position = vec4(p, 1);
  f_tpos = tpos;
}
