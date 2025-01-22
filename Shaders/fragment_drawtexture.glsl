#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ImageTexture;

void main()
{
  //vec2 textUV = { fragUV.x, 1.0 - fragUV.y };

  fragColor = texture(u_ImageTexture, fragUV);
}
