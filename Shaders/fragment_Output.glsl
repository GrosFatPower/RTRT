#version 430 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform int       u_AccumulatedFrames;

void main()
{
  fragColor = texture(u_ScreenTexture, fragUV);

  if ( u_AccumulatedFrames > 1 )
  {
    float multiplier = 1. / u_AccumulatedFrames;
    fragColor.x *= multiplier;
    fragColor.y *= multiplier;
    fragColor.z *= multiplier;
  }
}
