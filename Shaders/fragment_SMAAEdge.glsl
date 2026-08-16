#version 410 core

in vec2 fragUV;
layout(location = 0) out vec2 fragEdges;

uniform sampler2D u_Input;
uniform vec2 u_InvResolution;

float Luma( vec3 iColor )
{
  return dot(iColor, vec3(0.299, 0.587, 0.114));
}

void main()
{
  float center = Luma(texture(u_Input, fragUV).rgb);
  float left = Luma(texture(u_Input, fragUV - vec2(u_InvResolution.x, 0.0)).rgb);
  float top = Luma(texture(u_Input, fragUV - vec2(0.0, u_InvResolution.y)).rgb);
  const float threshold = 0.04;
  fragEdges = vec2(step(threshold, abs(center - left)), step(threshold, abs(center - top)));
}
