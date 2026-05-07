#ifndef _SHADOWS_GLSL_
#define _SHADOWS_GLSL_

#define MAX_SHADOW_CASTER_COUNT 8

struct ShadowCaster
{
  int   _LightIndex;
  int   _Type;
  int   _Layer;
  float _Far;
  vec3  _Pos;
  vec3  _Dir;
  mat4  _DirectionalViewProj;
  mat4  _CubeViewProj[6];
};

uniform samplerCubeArray u_ShadowCubeMaps;
uniform sampler2DArray   u_Shadow2DMaps;
uniform int              u_EnableShadowMapping = 0;
uniform int              u_NbShadowCasters = 0;
uniform ShadowCaster     u_ShadowCasters[MAX_SHADOW_CASTER_COUNT];
uniform float            u_ShadowBias = 0.02;

int FindShadowCaster( int iLightIndex, int iLightType )
{
  if ( u_EnableShadowMapping == 0 )
    return -1;

  for ( int i = 0; i < u_NbShadowCasters; ++i )
  {
    if ( ( u_ShadowCasters[i]._LightIndex == iLightIndex )
      && ( u_ShadowCasters[i]._Type == iLightType ) )
      return i;
  }

  return -1;
}

float ComputeLocalShadow( int iCasterIndex, vec3 iFragPos )
{
  ShadowCaster caster = u_ShadowCasters[iCasterIndex];
  vec3 fragToLight = iFragPos - caster._Pos;
  float currentDepth = length(fragToLight);
  if ( ( currentDepth <= 0.0 ) || ( currentDepth >= caster._Far ) )
    return 1.0;

  const int SampleCount = 20;
  const vec3 SampleOffsetDirections[20] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
  );

  float diskRadius = max( 0.02 * currentDepth / caster._Far, 0.005 );
  float shadow = 0.0;
  for ( int i = 0; i < SampleCount; ++i )
  {
    float closestDepth = texture( u_ShadowCubeMaps, vec4(fragToLight + SampleOffsetDirections[i] * diskRadius, float(caster._Layer)) ).r;
    closestDepth *= caster._Far;
    if ( currentDepth - u_ShadowBias > closestDepth )
      shadow += 1.0;
  }

  return 1.0 - shadow / float(SampleCount);
}

float ComputeDistantShadow( int iCasterIndex, vec3 iFragPos, vec3 iNormal, vec3 iLightDir )
{
  ShadowCaster caster = u_ShadowCasters[iCasterIndex];
  vec4 shadowPos = caster._DirectionalViewProj * vec4(iFragPos, 1.0);
  vec3 projCoords = shadowPos.xyz / shadowPos.w;

  if ( ( projCoords.x < -1.0 ) || ( projCoords.x > 1.0 )
    || ( projCoords.y < -1.0 ) || ( projCoords.y > 1.0 )
    || ( projCoords.z < -1.0 ) || ( projCoords.z > 1.0 ) )
    return 1.0;

  vec2 uv = projCoords.xy * 0.5 + 0.5;
  float currentDepth = projCoords.z * 0.5 + 0.5;
  float bias = max( u_ShadowBias * ( 1.0 - max(dot(iNormal, iLightDir), 0.0) ), u_ShadowBias * 0.25 );
  vec2 texelSize = 1.0 / vec2(textureSize(u_Shadow2DMaps, 0).xy);

  float shadow = 0.0;
  for ( int y = -1; y <= 1; ++y )
  {
    for ( int x = -1; x <= 1; ++x )
    {
      float closestDepth = texture( u_Shadow2DMaps, vec3(uv + vec2(x, y) * texelSize, float(caster._Layer)) ).r;
      if ( currentDepth - bias > closestDepth )
        shadow += 1.0;
    }
  }

  return 1.0 - shadow / 9.0;
}

float ComputeShadowForLight( int iLightIndex, int iLightType, vec3 iFragPos, vec3 iNormal, vec3 iLightDir, out bool oHasShadow )
{
  int casterIndex = FindShadowCaster(iLightIndex, iLightType);
  oHasShadow = casterIndex >= 0;
  if ( !oHasShadow )
    return 1.0;

  if ( iLightType == DISTANT_LIGHT )
    return ComputeDistantShadow(casterIndex, iFragPos, iNormal, iLightDir);

  return ComputeLocalShadow(casterIndex, iFragPos);
}

#endif // _SHADOWS_GLSL_
