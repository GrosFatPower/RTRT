#version 430 core

#include ToneMapping.glsl
#include FXAA.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_Texture;

void main()
{
  fragColor = texture(u_Texture, fragUV);
}
