#version 410 core

in vec2 fragUV;
layout(location = 0) out vec4 fragWeights;

uniform sampler2D u_Edges;
uniform vec2 u_InvResolution;

float Edge( vec2 iUV, int iComponent )
{
  return texture(u_Edges, iUV)[iComponent];
}

float SearchLength( vec2 iUV, vec2 iStep, int iComponent )
{
  float length = 0.0;
  for ( int i = 1; i <= 16; ++i )
  {
    if ( Edge(iUV + float(i) * iStep, iComponent) < 0.5 )
      break;
    length += 1.0;
  }
  return length;
}

float AreaWeight( float iFirstLength, float iSecondLength )
{
  // Long, continuous edges receive stronger coverage than isolated samples.
  float span = iFirstLength + iSecondLength + 1.0;
  return 0.5 * ( 1.0 - 1.0 / ( span + 1.0 ) );
}

void main()
{
  vec2 xStep = vec2(u_InvResolution.x, 0.0);
  vec2 yStep = vec2(0.0, u_InvResolution.y);
  vec2 rightUV = fragUV + xStep;
  vec2 bottomUV = fragUV + yStep;

  float left = ( Edge(fragUV, 0) > 0.5 ) ? AreaWeight(SearchLength(fragUV, yStep, 0), SearchLength(fragUV, -yStep, 0)) : 0.0;
  float right = ( Edge(rightUV, 0) > 0.5 ) ? AreaWeight(SearchLength(rightUV, yStep, 0), SearchLength(rightUV, -yStep, 0)) : 0.0;
  float top = ( Edge(fragUV, 1) > 0.5 ) ? AreaWeight(SearchLength(fragUV, xStep, 1), SearchLength(fragUV, -xStep, 1)) : 0.0;
  float bottom = ( Edge(bottomUV, 1) > 0.5 ) ? AreaWeight(SearchLength(bottomUV, xStep, 1), SearchLength(bottomUV, -xStep, 1)) : 0.0;
  fragWeights = vec4(left, right, top, bottom);
}
