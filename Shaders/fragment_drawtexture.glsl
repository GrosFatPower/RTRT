#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform bool      u_DirectOutputPass;
uniform float     u_Time;
uniform float     u_TimeDelta;

void RenderToTexture()
{
  vec2 textUV;
  textUV.x = fragUV.x;
  textUV.y = 1.0 - fragUV.y;
  fragColor = texture(u_Texture, textUV) * cos(u_Time + fragUV.x) * cos(u_Time + fragUV.y);
}

void RenderImage()
{
  fragColor = texture(u_ScreenTexture, fragUV);
}

void main()
{
  if ( u_DirectOutputPass )
  {
    RenderImage();
  }
  else
  {
    RenderToTexture();
  }
}
