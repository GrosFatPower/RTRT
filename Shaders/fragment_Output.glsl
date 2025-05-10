#version 430 core

#include Globals.glsl
#include ToneMapping.glsl
#include FXAA.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_Texture;

void main()
{
  vec4 pixelValue = texture(u_Texture, fragUV);

  vec3 color = pixelValue.xyz;
  float alpha = pixelValue.w;

  if ( 0 != u_ToneMapping )
  {
    color = ReinhardToneMapping_Luminance( color );
    color = GammaCorrection( color );
  }

  fragColor = vec4( color , alpha);
}
