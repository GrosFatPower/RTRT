#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_Texture;
uniform vec3      u_Resolution;
uniform vec4      u_Mouse;
uniform float     u_Time;
uniform float     u_TimeDelta;

void RenderToTexture()
{
  fragColor = vec4((cos(u_Time + fragUV.x) + 1.) * 0.5, (sin(u_Time + fragUV.y) + 1.) * 0.5, (cos(u_Time + fragUV.x + fragUV.y) + 1.) * 0.5, 1.);
}

void main()
{
  RenderToTexture();
}
