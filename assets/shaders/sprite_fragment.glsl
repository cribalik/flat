#version 330 core

in vec2 f_tpos;
out vec4 color;

uniform sampler2D tex;

void main(){
  color = vec4(1,1,0,1);// texture(tex, f_tpos);
}
