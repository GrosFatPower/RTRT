#version 410 core

in vec2 fragUV;
out vec4 fragColor;

uniform float u_Time;

void main()
{
  fragColor = vec4((cos(u_Time + fragUV.x) + 1.) * 0.5, (sin(u_Time + fragUV.y) + 1.) * 0.5, (cos(u_Time + fragUV.x + fragUV.y) + 1.) * 0.5, 1.);
}
