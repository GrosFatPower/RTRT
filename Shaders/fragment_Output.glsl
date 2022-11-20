#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform int       u_AccumulatedPasses;

void main()
{
  fragColor = texture(u_ScreenTexture, fragUV);

  if ( u_AccumulatedPasses > 1 )
  {
    float multiplier = 1. / u_AccumulatedPasses;
    fragColor.x *= multiplier;
    fragColor.y *= multiplier;
    fragColor.z *= multiplier;
  }
}
