#version 410 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform float     u_Time;
uniform float     u_TimeDelta;

float rand2D(in vec2 co)
{
  return fract(sin(dot(co.xy + u_Time / 13.0 ,vec2(12.9898,78.233))) * 43758.5453);
}

vec4 noise(vec2 st)
{
  float r = rand2D(st);
  return vec4(r, r, r, 1.0);
}

void RenderToTexture()
{
  //vec2 textUV;
  //textUV.x = fragUV.x;
  //textUV.y = 1.0 - fragUV.y;
  //fragColor = texture(u_Texture, textUV) * cos(u_Time + fragUV.x) * cos(u_Time + fragUV.y);

  //vec2 norm_fragCoord = fragUV.xy / u_Resolution.xy;
  fragColor = noise(fragUV);
}

void RenderImage()
{
  fragColor = texture(u_ScreenTexture, fragUV);
}

void main()
{
  RenderToTexture();
}
