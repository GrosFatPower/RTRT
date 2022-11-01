#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;

void main()
{
  fragColor = texture(u_ScreenTexture, fragUV);
}
