#version 330 core

layout (location = 0) in vec4 pos;
layout (location = 1) in vec4 texCoord;

out VERTEX_DATA {
  vec4 texCoord;
} myOutput;

uniform vec2 uView;

void main()
{
  gl_Position = pos + vec4(uView, 0, 0);
  myOutput.texCoord = texCoord;
}
