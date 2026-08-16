#version 410 core

#include FXAA.glsl

in vec2 fragUV;
layout(location = 0) out vec4 fragColor;

uniform sampler2D u_Input;
uniform vec2 u_Resolution;

void main()
{
  fragColor = ApplyFXAA(u_Input, fragUV * u_Resolution, 1.0 / u_Resolution);
}
