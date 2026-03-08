#version 410 core

#include Globals.glsl
#include Lights.glsl
#include Sampling.glsl
#include Structures.glsl
#include Intersections.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D   u_GAlbedo;
uniform sampler2D   u_GNormal;
uniform sampler2D   u_GPosition;
uniform sampler2D   u_GDepth;
uniform samplerCube u_ShadowCubeMap;
uniform sampler2D   u_Shadow2DMap;
uniform sampler2D   u_SSAOMap;

uniform vec3 u_Ambient = vec3(0.001, 0.001, 0.001);
uniform vec3 u_BackgroundColor = vec3(1.0, 1.0, 1.0);
uniform vec2 u_Resolution;
uniform Camera u_Camera;

uniform int       u_EnableBackground  = 0;
uniform int       u_EnableEnvMap      = 0;
uniform float     u_EnvMapRotation    = 0.f;
uniform vec2      u_EnvMapRes;
uniform sampler2D u_EnvMap;

uniform int   u_EnableShadowMapping = 0;
uniform int   u_ShadowLightIndex    = -1;
uniform int   u_ShadowLightType     = -1;
uniform vec3  u_ShadowLightPos      = vec3(0.0);
uniform vec3  u_ShadowLightDir      = vec3(0.0, 1.0, 0.0);
uniform mat4  u_ShadowLightViewProj = mat4(1.0);
uniform float u_ShadowBias          = 0.02;
uniform float u_ShadowFar           = 25.0;
uniform int   u_EnableSSAO          = 0;
uniform float u_SSAOIntensity       = 1.0;

vec3 GetCameraRayDir()
{
  vec2 centeredUV = fragUV * 2.0 - 1.0;

  float scale = tan(u_Camera._FOV * .5);
  centeredUV.x *= scale;
  centeredUV.y *= ( u_Resolution.y / u_Resolution.x ) * scale;

  return normalize(u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward);
}

bool TraceVisibleLight( in Ray iRay, in float iSceneDist, out vec3 oLightColor )
{
  float closestLightDist = iSceneDist;
  bool hitLight = false;
  oLightColor = vec3(0.0);

  for ( int i = 0; i < u_NbLights; ++i )
  {
    float hitDist = 0.0;
    bool hit = false;

    if ( u_Lights[i]._Type == SPHERE_LIGHT )
      hit = SphereIntersection( vec4( u_Lights[i]._Pos, u_Lights[i]._Radius ), iRay, hitDist );
    else if ( u_Lights[i]._Type == QUAD_LIGHT )
      hit = QuadIntersection( u_Lights[i]._Pos, u_Lights[i]._DirU, u_Lights[i]._DirV, iRay, hitDist );

    if ( hit && ( hitDist > 0.0 ) && ( hitDist < closestLightDist ) )
    {
      closestLightDist = hitDist;
      oLightColor = u_Lights[i]._Emission;
      hitLight = true;
    }
  }

  return hitLight;
}

float ComputeLocalShadow( vec3 iFragPos )
{
  vec3 fragToLight = iFragPos - u_ShadowLightPos;
  float currentDepth = length(fragToLight);
  if ( currentDepth <= 0.0 || currentDepth >= u_ShadowFar )
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

void main()
{
  vec3 albedo  = texture(u_GAlbedo, fragUV).rgb;
  vec3 N       = normalize(texture(u_GNormal, fragUV).xyz * 2.0 - 1.0);
  vec3 pos     = texture(u_GPosition, fragUV).xyz;
  float depth  = texture(u_GDepth, fragUV).x;
  float aoRaw  = texture(u_SSAOMap, fragUV).r;
  float ao     = ( u_EnableSSAO != 0 ) ? clamp(1.0 - (1.0 - aoRaw) * u_SSAOIntensity, 0.0, 1.0) : 1.0;

  vec3 cameraRayDir = GetCameraRayDir();

  if ( depth >= 1.0 )
  {
    if ( u_ShowLights != 0 )
    {
      Ray lightRay = Ray( u_Camera._Pos, cameraRayDir );
      vec3 lightColor = vec3(0.0);
      if ( TraceVisibleLight( lightRay, 1e20, lightColor ) )
      {
        fragColor = vec4(lightColor, 1.0);
        return;
      }
    }

    if ( u_EnableEnvMap > 0 )
      fragColor = vec4(SampleSkybox(cameraRayDir, u_EnvMap, u_EnvMapRotation), 1.);
    else if ( u_EnableBackground > 0 )
      fragColor = vec4(u_BackgroundColor, 1.);
    else
      fragColor = vec4(0., 0., 0., 1.);
    return;
  }

  if ( ( u_DebugMode & 0x01 ) != 0 )
  {
    fragColor = vec4(vec3(depth), 1.);
    return;
  }
  else if ( ( u_DebugMode & 0x02 ) != 0 )
  {
    fragColor = vec4(abs(N), 1.);
    return;
  }

  float sceneDist = length(pos - u_Camera._Pos);
  if ( u_ShowLights != 0 )
  {
    Ray lightRay = Ray( u_Camera._Pos, cameraRayDir );
    vec3 lightColor = vec3(0.0);
    if ( TraceVisibleLight( lightRay, sceneDist, lightColor ) )
    {
      fragColor = vec4(lightColor, 1.0);
      return;
    }
  }

  vec4 alpha = vec4(0.);
  float shadowFactorDebug = 1.0;
  for ( int i = 0; i < u_NbLights; ++i )
  {
    float ambientStrength = .1 * ao;
    float diffuse = 0.;
    float specular = 0.;

    vec3 L;
    if ( u_Lights[i]._Type == DISTANT_LIGHT )
      L = normalize(u_Lights[i]._Pos);
    else
      L = normalize(u_Lights[i]._Pos - pos);

    diffuse = max(0., dot(N, L));

    vec3 V = normalize(u_Camera._Pos - pos);
    vec3 H = reflect(-L, N);

    float specPow = 32.0;
    float specularStrength = 0.5f;
    specular = pow(max(dot(V, H), 0.), specPow) * specularStrength;

    float visibility = 1.0;
    if ( ( u_EnableShadowMapping > 0 )
      && ( i == u_ShadowLightIndex )
      && ( int(u_Lights[i]._Type) == u_ShadowLightType ) )
    {
      visibility = ComputeShadow(pos, N, L);
      shadowFactorDebug = visibility;
    }

    float directLighting = visibility * min(diffuse + specular, 1.0);
    directLighting *= mix(1.0, ao, 0.35);
    alpha += vec4(normalize(u_Lights[i]._Emission), 1.) * ( ambientStrength + directLighting );
  }

  if ( ( u_DebugMode & 0x08 ) != 0 )
  {
    fragColor = vec4(vec3(shadowFactorDebug), 1.0);
    return;
  }

  if ( ( u_DebugMode & 0x10 ) != 0 )
  {
    fragColor = vec4(vec3(aoRaw), 1.0);
    return;
  }

  fragColor = min(vec4(albedo, 1.) * alpha, vec4(1.));
}
