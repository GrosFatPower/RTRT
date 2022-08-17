#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform bool u_DirectOutputPass;

void RenderToTexture()
{
  //fragColor = vec4(fragUV.x, fragUV.y, 0.0, 1.0);
  fragColor = texture(u_Texture, fragUV);
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
