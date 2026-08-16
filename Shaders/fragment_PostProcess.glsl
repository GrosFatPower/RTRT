#version 410 core

#include Globals.glsl
#include ToneMapping.glsl
#include FXAA.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_ScreenTexture;
uniform vec2      u_RenderRes;
uniform int       u_FXAA;

void main()
{
  vec4 pixelValue = texture(u_ScreenTexture, fragUV);

  if ( 1 == u_FXAA )
  {
    vec2 fragCoord = fragUV * u_RenderRes;
    vec2 invRes = 1.f / u_RenderRes.xy;
    pixelValue = ApplyFXAA( u_ScreenTexture, fragCoord, invRes );
  }

  vec3 color = pixelValue.xyz;
  float alpha = pixelValue.w;

  if ( ( u_DebugMode & 0x8000 ) != 0 )
    color = vec3(0.5) + vec3(color.r, color.g, 0.0) * 4.0;

  if ( ( 0 != u_ToneMapping ) && ( 0 == ( u_DebugMode & ( 0x1000 | 0x2000 | 0x4000 | 0x8000 | 0x10000 | 0x20000 ) ) ) )
  {
    color = ReinhardToneMapping_Luminance( color );
    color = GammaCorrection( color );
  }

  fragColor = vec4( color , alpha);
}
