#version 410 core

out vec4 fragColor;

uniform vec3 u_WireColor = vec3(1., 0., 0.);

void main()
{
  fragColor = vec4(u_WireColor, 1.0);
}
