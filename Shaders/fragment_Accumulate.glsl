#version 430 core

#include ToneMapping.glsl
#include FXAA.glsl

in vec2 fragUV;
out vec4 fragColor;

// ============================================================================
// Uniforms
// ============================================================================

uniform int       u_Accumulate;
uniform int       u_NbCompleteFrames;
uniform int       u_TiledRendering;
uniform vec2      u_TileOffset;
uniform vec2      u_InvNbTiles;
uniform sampler2D u_PreviousFrame;
uniform sampler2D u_NewFrame;

uniform int       u_DebugMode;

void main()
{
  fragColor = texture(u_NewFrame, fragUV);

  if ( ( 1 == u_Accumulate ) && ( u_NbCompleteFrames > 0 ) )
  {
    vec2 realFragUV = fragUV;
    if( 1 == u_TiledRendering )
      realFragUV = mix(u_TileOffset, u_TileOffset + u_InvNbTiles, fragUV);

    float multiplier = 1. / ( u_NbCompleteFrames + 1 );
    fragColor.xyz = ( texture(u_PreviousFrame, realFragUV).xyz * u_NbCompleteFrames + fragColor.xyz ) * multiplier;
  }

  if ( ( 6 == u_DebugMode ) && ( 1 == u_TiledRendering ) )
  {
    if ( ( fragUV.x < 0.01f )         || ( fragUV.y < 0.01f )
      || ( fragUV.x > ( 1.- 0.01f ) ) || ( fragUV.y > ( 1.- 0.01f ) ) )
    {
      fragColor.r = (u_NbCompleteFrames % 3) / 2.f;
      fragColor.g = (u_NbCompleteFrames % 4) / 3.f;
      fragColor.b = (u_NbCompleteFrames % 5) / 4.f;
      fragColor.a = 1.f;
    }
  }
}
