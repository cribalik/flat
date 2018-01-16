#version 330 core

in vec3 f_normal;
in vec2 f_tpos;
out vec4 color;

uniform sampler2D tex;

void main(){
	color = vec4(f_normal, 1) + texture(tex, f_tpos);
  // color = texture(tex, f_tpos);
}
