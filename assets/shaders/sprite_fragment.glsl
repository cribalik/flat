#version 330 core

in vec3 f_normal;
in vec2 f_tpos;
out vec4 color;

uniform sampler2D tex;

void main(){
	color = vec4(f_normal + vec3(0.3,0.3,0.3), 1);
  // color = texture(tex, f_tpos);
}
