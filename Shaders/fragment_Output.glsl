#version 430 core

#include ToneMapping.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform int       u_AccumulatedFrames;
uniform int       u_ToneMapping;

void main()
{
  vec4 pixelValue = texture(u_ScreenTexture, fragUV);

  vec3 color = pixelValue.xyz;
  float alpha = pixelValue.w;

  if ( u_AccumulatedFrames > 1 )
  {
    float multiplier = 1. / u_AccumulatedFrames;
    color.x *= multiplier;
    color.y *= multiplier;
    color.z *= multiplier;
  }

  if ( 0 != u_ToneMapping )
  {
    color = ReinhardToneMapping( color );
    color = GammaCorrection( color );
  }

  fragColor = vec4( color , alpha);
}
