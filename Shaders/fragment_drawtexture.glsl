#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ImageTexture;
uniform vec3      u_Resolution;

void RenderToTexture()
{
  //vec2 textUV = { fragUV.x, 1.0 - fragUV.y };
 
  fragColor = texture(u_ImageTexture, fragUV);
}

void main()
{
  RenderToTexture();
}
