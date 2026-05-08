#version 410 core

#include Globals.glsl
#include Lights.glsl
#include Shadows.glsl
#include Sampling.glsl
#include Structures.glsl
#include Intersections.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D   u_GAlbedo;
uniform sampler2D   u_GNormal;
uniform sampler2D   u_GPosition;
uniform sampler2D   u_GMaterial;
uniform sampler2D   u_GDepth;
uniform sampler2D   u_SSAOMap;
uniform sampler2D   u_SSRMap;

uniform vec3 u_Ambient = vec3(0.001, 0.001, 0.001);
uniform vec3 u_BackgroundColor = vec3(1.0, 1.0, 1.0);
uniform vec2 u_Resolution;
uniform Camera u_Camera;

uniform int       u_EnableBackground  = 0;
uniform int       u_EnableEnvMap      = 0;
uniform float     u_EnvMapRotation    = 0.f;
uniform vec2      u_EnvMapRes;
uniform sampler2D u_EnvMap;
uniform sampler2D u_BRDFLUT;
uniform float     u_EnvMapMipCount = 1.0;

uniform int   u_EnableSSAO          = 0;
uniform float u_SSAOIntensity       = 1.0;
uniform int   u_EnableSSR           = 0;
uniform float u_SSRIntensity        = 0.6;
uniform float u_SSRMaxRoughness     = 0.55;
uniform int   u_EnableSpecularIBL   = 0;
uniform float u_SpecularIBLIntensity = 1.0;

vec3 GetCameraRayDir()
{
  vec2 centeredUV = fragUV * 2.0 - 1.0;

  float scale = tan(u_Camera._FOV * .5);
  centeredUV.x *= scale;
  centeredUV.y *= ( u_Resolution.y / u_Resolution.x ) * scale;

  return normalize(u_Camera._Right * centeredUV.x + u_Camera._Up * centeredUV.y + u_Camera._Forward);
}

vec2 EnvMapUV( in vec3 iDir )
{
  float theta = acos(clamp(iDir.y, -1.0, 1.0));
  float phi   = atan(iDir.z, iDir.x);
  return vec2((PI + phi) * INV_TWO_PI, theta * INV_PI) + vec2(u_EnvMapRotation, 0.0);
}

vec3 SampleEnvMapNoSeam( in vec3 iDir )
{
  vec2 uv = EnvMapUV(iDir);
  uv.x = fract(uv.x);
  vec2 texel = 1.0 / max(u_EnvMapRes, vec2(1.0));
  uv.x = clamp(uv.x, texel.x, 1.0 - texel.x);
  uv.y = clamp(uv.y, texel.y, 1.0 - texel.y);
  return texture(u_EnvMap, uv).rgb;
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

void main()
{
  vec3 albedo  = texture(u_GAlbedo, fragUV).rgb;
  vec3 N       = normalize(texture(u_GNormal, fragUV).xyz * 2.0 - 1.0);
  vec3 pos     = texture(u_GPosition, fragUV).xyz;
  vec3 material = texture(u_GMaterial, fragUV).rgb;
  float depth  = texture(u_GDepth, fragUV).x;
  float aoRaw  = texture(u_SSAOMap, fragUV).r;
  float ao     = ( u_EnableSSAO != 0 ) ? clamp(1.0 - (1.0 - aoRaw) * u_SSAOIntensity, 0.0, 1.0) : 1.0;
  float roughness = clamp(material.r, 0.001, 1.0);
  float metallic = clamp(material.g, 0.0, 1.0);
  float reflectance = clamp(material.b, 0.0, 1.0);
  vec4 ssrSample = texture(u_SSRMap, fragUV);

  vec3 cameraRayDir = GetCameraRayDir();

  if ( depth >= 1.0 )
  {
    if ( ( u_DebugMode & 0x80 ) != 0 )
    {
      fragColor = vec4(0.0, 0.0, 0.0, 1.0);
      return;
    }

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
      fragColor = vec4(SampleEnvMapNoSeamLod(cameraRayDir, 0.0), 1.);
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

  vec3 V = normalize(u_Camera._Pos - pos);
  float NdotV = max(dot(N, V), 0.0);
  vec3 F0 = mix(vec3(0.16 * reflectance * reflectance), albedo, metallic);
  vec3 specularIBL = vec3(0.0);
  vec3 specularSSR = vec3(0.0);

  if ( ( u_EnableSpecularIBL != 0 ) && ( u_EnableEnvMap > 0 ) )
  {
    float lod = roughness * roughness * max(u_EnvMapMipCount - 1.0, 0.0);
    vec3 R = reflect(-V, N);
    vec3 prefiltered = SampleEnvMapNoSeamLod(R, lod);
    vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    specularIBL = prefiltered * (F0 * brdf.x + brdf.y);
    specularIBL *= u_SpecularIBLIntensity;
    specularIBL *= mix(1.0, ao, 0.2);
  }

  if ( u_EnableSSR != 0 )
  {
    float roughnessFade = 1.0 - smoothstep(u_SSRMaxRoughness * 0.65, u_SSRMaxRoughness, roughness);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    specularSSR = ssrSample.rgb * F * ssrSample.a * roughnessFade * u_SSRIntensity;
    specularSSR *= mix(1.0, ao, 0.2);
  }

  vec4 alpha = vec4(0.);
  float shadowFactorDebug = 0.0;
  int shadowFactorCount = 0;
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

    vec3 H = reflect(-L, N);

    float specPow = 32.0;
    float specularStrength = 0.5f;
    specular = pow(max(dot(V, H), 0.), specPow) * specularStrength;

    float visibility = 1.0;
    bool hasShadow = false;
    visibility = ComputeShadowForLight(i, int(u_Lights[i]._Type), pos, N, L, hasShadow);
    if ( hasShadow )
    {
      shadowFactorDebug += visibility;
      shadowFactorCount++;
    }

    float directLighting = visibility * min(diffuse + specular, 1.0);
    directLighting *= mix(1.0, ao, 0.35);
    alpha += vec4(normalize(u_Lights[i]._Emission), 1.) * ( ambientStrength + directLighting );
  }

  if ( ( u_DebugMode & 0x08 ) != 0 )
  {
    float debugVisibility = ( shadowFactorCount > 0 ) ? ( shadowFactorDebug / float(shadowFactorCount) ) : ( 1.0 );
    fragColor = vec4(vec3(debugVisibility), 1.0);
    return;
  }

  if ( ( u_DebugMode & 0x10 ) != 0 )
  {
    fragColor = vec4(vec3(aoRaw), 1.0);
    return;
  }
  else if ( ( u_DebugMode & 0x20 ) != 0 )
  {
    fragColor = vec4(specularIBL, 1.0);
    return;
  }
  else if ( ( u_DebugMode & 0x40 ) != 0 )
  {
    fragColor = vec4(roughness, metallic, reflectance, 1.0);
    return;
  }
  else if ( ( u_DebugMode & 0x80 ) != 0 )
  {
    float confidence = clamp(ssrSample.a, 0.0, 1.0);
    vec3 debugColor = max(ssrSample.rgb, vec3(confidence));
    fragColor = vec4(debugColor, 1.0);
    return;
  }

  vec3 outColor = albedo * alpha.rgb + specularIBL + specularSSR;
  fragColor = vec4(min(outColor, vec3(1.0)), 1.0);
}
