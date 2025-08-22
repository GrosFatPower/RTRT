#version 410 core

#include Globals.glsl
#include ToneMapping.glsl
#include FXAA.glsl

in vec2 fragUV;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragNormal;
layout(location = 2) out vec4 fragPosition;

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
uniform sampler2D u_NewFrameNormals;
uniform sampler2D u_NewFramePos;

void main()
{
  fragColor = texture(u_NewFrame, fragUV);
  fragNormal = texture(u_NewFrameNormals, fragUV);
  fragPosition = texture(u_NewFramePos, fragUV);

  if ( ( 1 == u_Accumulate ) && ( u_NbCompleteFrames > 0 ) )
  {
    vec2 realFragUV = fragUV;
    if( 1 == u_TiledRendering )
      realFragUV = mix(u_TileOffset, u_TileOffset + u_InvNbTiles, fragUV);

    float multiplier = 1. / ( u_NbCompleteFrames + 1 );
    fragColor.xyz = ( texture(u_PreviousFrame, realFragUV).xyz * u_NbCompleteFrames + fragColor.xyz ) * multiplier;
  }

  if ( ( 1 == u_DebugMode ) && ( 1 == u_TiledRendering ) )
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
