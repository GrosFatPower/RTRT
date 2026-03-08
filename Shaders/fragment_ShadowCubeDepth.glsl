#version 410 core

in vec3 fragWorldPos;

uniform vec3  u_LightPos;
uniform float u_FarPlane;

void main()
{
  float lightDistance = length(fragWorldPos - u_LightPos);
  gl_FragDepth = min(lightDistance / u_FarPlane, 1.0);
}
