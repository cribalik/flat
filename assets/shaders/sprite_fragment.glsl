#version 330 core

in vec2 texCoord;

uniform sampler2D uTexture;
uniform vec3 uAmbientLight;

out vec4 color;

void main()
{
  vec4 texColor = texture(uTexture, vec2(texCoord.x/1024, texCoord.y/1024));
  if (texColor.a == 0) {
    discard;
  } else {
    color = vec4(uAmbientLight, 1) * texColor;
  }
}
