#version 410 core

#include Constants.glsl
#include Lights.glsl
#include Structures.glsl
#include Material.glsl

in vec3 fragWorldPos;
in vec3 fragNormal;
in vec2 fragUV;
flat in int v_MaterialID;

out vec4 fragColor;

uniform sampler2D   u_GDepth;
uniform samplerCube u_ShadowCubeMap;
uniform sampler2D   u_Shadow2DMap;
uniform sampler2D   u_EnvMap;
uniform sampler2D   u_BRDFLUT;
uniform vec2        u_EnvMapRes = vec2(1.0);
uniform float       u_EnvMapRotation = 0.f;
uniform float       u_EnvMapMipCount = 1.0;
uniform int         u_EnableEnvMap = 0;

uniform Camera      u_Camera;

uniform int   u_EnableShadowMapping = 0;
uniform int   u_ShadowLightIndex    = -1;
uniform int   u_ShadowLightType     = -1;
uniform vec3  u_ShadowLightPos      = vec3(0.0);
uniform vec3  u_ShadowLightDir      = vec3(0.0, 1.0, 0.0);
uniform mat4  u_ShadowLightViewProj = mat4(1.0);
uniform float u_ShadowBias          = 0.02;
uniform float u_ShadowFar           = 25.0;

float ComputeLocalShadow( vec3 iFragPos )
{
  vec3 fragToLight = iFragPos - u_ShadowLightPos;
  float currentDepth = length(fragToLight);
  if ( ( currentDepth <= 0.0 ) || ( currentDepth >= u_ShadowFar ) )
    return 1.0;

  const int SampleCount = 20;
  const vec3 SampleOffsetDirections[20] = vec3[](
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
  );

  float diskRadius = max( 0.02 * currentDepth / u_ShadowFar, 0.005 );
  float shadow = 0.0;
  for ( int i = 0; i < SampleCount; ++i )
  {
    float closestDepth = texture( u_ShadowCubeMap, fragToLight + SampleOffsetDirections[i] * diskRadius ).r;
    closestDepth *= u_ShadowFar;
    if ( currentDepth - u_ShadowBias > closestDepth )
      shadow += 1.0;
  }

  return 1.0 - shadow / float(SampleCount);
}

float ComputeDistantShadow( vec3 iFragPos, vec3 iNormal, vec3 iLightDir )
{
  vec4 shadowPos = u_ShadowLightViewProj * vec4(iFragPos, 1.0);
  vec3 projCoords = shadowPos.xyz / shadowPos.w;

  if ( ( projCoords.x < -1.0 ) || ( projCoords.x > 1.0 )
    || ( projCoords.y < -1.0 ) || ( projCoords.y > 1.0 )
    || ( projCoords.z < -1.0 ) || ( projCoords.z > 1.0 ) )
    return 1.0;

  vec2 uv = projCoords.xy * 0.5 + 0.5;
  float currentDepth = projCoords.z * 0.5 + 0.5;
  float bias = max( u_ShadowBias * ( 1.0 - max(dot(iNormal, iLightDir), 0.0) ), u_ShadowBias * 0.25 );
  vec2 texelSize = 1.0 / vec2(textureSize(u_Shadow2DMap, 0));

  float shadow = 0.0;
  for ( int y = -1; y <= 1; ++y )
  {
    for ( int x = -1; x <= 1; ++x )
    {
      float closestDepth = texture( u_Shadow2DMap, uv + vec2(x, y) * texelSize ).r;
      if ( currentDepth - bias > closestDepth )
        shadow += 1.0;
    }
  }

  return 1.0 - shadow / 9.0;
}

float ComputeShadow( vec3 iFragPos, vec3 iNormal, vec3 iLightDir )
{
  if ( u_EnableShadowMapping == 0 )
    return 1.0;

  if ( u_ShadowLightType == DISTANT_LIGHT )
    return ComputeDistantShadow( iFragPos, iNormal, iLightDir );

  return ComputeLocalShadow( iFragPos );
}

vec2 EnvMapUV( in vec3 iDir )
{
  float theta = acos(clamp(iDir.y, -1.0, 1.0));
  float phi   = atan(iDir.z, iDir.x);
  return vec2((PI + phi) * INV_TWO_PI, theta * INV_PI) + vec2(u_EnvMapRotation, 0.0);
}

vec3 SampleEnvMapNoSeamLod( in vec3 iDir, in float iLod )
{
  vec2 uv = EnvMapUV(iDir);
  uv.x = fract(uv.x);
  float lodScale = exp2(iLod);
  vec2 texel = 1.0 / max(u_EnvMapRes * lodScale, vec2(1.0));
  uv.x = clamp(uv.x, texel.x, 1.0 - texel.x);
  uv.y = clamp(uv.y, texel.y, 1.0 - texel.y);
  return textureLod(u_EnvMap, uv, iLod).rgb;
}

void main()
{
  if ( v_MaterialID < 0 )
    discard;

  HitPoint hitPoint;
  InitializeHitPoint(hitPoint);
  hitPoint._Pos        = fragWorldPos;
  hitPoint._Dist       = length(hitPoint._Pos - u_Camera._Pos);
  hitPoint._Normal     = normalize(fragNormal);
  if ( !gl_FrontFacing )
    hitPoint._Normal *= -1.0;
  hitPoint._UV         = fragUV;
  hitPoint._MaterialID = v_MaterialID;

  Material mat;
  LoadMaterial(hitPoint, mat);

  float baseAlpha = clamp(mat._Opacity, 0.0, 1.0);
  float alpha = baseAlpha;
  if ( mat._AlphaMode != ALPHA_MODE_BLEND )
    alpha = baseAlpha * ( 1.0 - clamp(mat._SpecTrans, 0.0, 1.0) );

  vec3 N = normalize(hitPoint._Normal);
  vec3 V = normalize(u_Camera._Pos - hitPoint._Pos);
  float roughness = clamp(mat._Roughness, 0.001, 1.0);
  float metallic = clamp(mat._Metallic, 0.0, 1.0);
  float reflectance = clamp(mat._Reflectance, 0.0, 1.0);
  float specTrans = clamp(mat._SpecTrans, 0.0, 1.0);

  vec3 diffuseColor = mat._Albedo * ( 1.0 - metallic ) * ( 1.0 - specTrans );
  vec3 F0 = mix(vec3(0.16 * reflectance * reflectance), mat._Albedo, metallic);

  vec3 directDiffuse = vec3(0.0);
  vec3 directSpecular = vec3(0.0);
  float specPower = mix(128.0, 6.0, roughness);

  for ( int i = 0; i < u_NbLights; ++i )
  {
    vec3 L;
    if ( u_Lights[i]._Type == DISTANT_LIGHT )
      L = normalize(u_Lights[i]._Pos);
    else
      L = normalize(u_Lights[i]._Pos - hitPoint._Pos);

    float NdotL = max(dot(N, L), 0.0);
    if ( NdotL <= 0.0 )
      continue;

    float visibility = 1.0;
    if ( ( u_EnableShadowMapping > 0 )
      && ( i == u_ShadowLightIndex )
      && ( int(u_Lights[i]._Type) == u_ShadowLightType ) )
      visibility = ComputeShadow(hitPoint._Pos, N, L);

    vec3 radiance = u_Lights[i]._Emission * visibility;
    directDiffuse += diffuseColor * radiance * NdotL;

    vec3 H = normalize(L + V);
    float NdotH = max(dot(N, H), 0.0);
    float specFactor = pow(NdotH, specPower);
    directSpecular += F0 * radiance * specFactor * NdotL;
  }

  vec3 envSpecular = vec3(0.0);
  if ( u_EnableEnvMap > 0 )
  {
    float lod = roughness * roughness * max(u_EnvMapMipCount - 1.0, 0.0);
    vec3 R = reflect(-V, N);
    vec3 prefiltered = SampleEnvMapNoSeamLod(R, lod);
    float NdotV = max(dot(N, V), 0.0);
    vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    envSpecular = prefiltered * (F0 * brdf.x + brdf.y);
  }

  vec3 premultipliedColor = ( directDiffuse + mat._Emission ) * alpha;
  premultipliedColor += directSpecular + envSpecular;

  // Keep purely transmissive surfaces (alpha near 0) visible through additive specular/reflection.
  if ( ( alpha <= 0.001 ) && ( max(max(premultipliedColor.r, premultipliedColor.g), premultipliedColor.b) <= 0.001 ) )
    discard;

  fragColor = vec4( max(premultipliedColor, vec3(0.0)), alpha );
}
