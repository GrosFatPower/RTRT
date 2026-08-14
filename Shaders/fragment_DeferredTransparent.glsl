#version 410 core

#include Constants.glsl
#include Lights.glsl
#include Shadows.glsl
#include Structures.glsl
#include Material.glsl
#include Lights.glsl
#include Sampling.glsl
#include DeferredPBRLighting.glsl

in vec3 fragWorldPos;
in vec3 fragNormal;
in vec2 fragUV;
flat in int v_MaterialID;

out vec4 fragColor;

uniform sampler2D   u_GDepth;
uniform sampler2D   u_EnvMap;
uniform sampler2D   u_BRDFLUT;
uniform vec2        u_EnvMapRes = vec2(1.0);
uniform float       u_EnvMapRotation = 0.f;
uniform float       u_EnvMapMipCount = 1.0;
uniform int         u_EnableEnvMap = 0;
uniform int         u_EnablePBRDirectLighting = 1;
uniform float       u_DirectLightIntensity = 1.0;
uniform float       u_SpecularIBLMaxRoughness = 0.5;

uniform Camera      u_Camera;

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
  ComputeTangentFrame(hitPoint._Normal, hitPoint._Pos, hitPoint._UV, hitPoint._Tangent, hitPoint._Bitangent);

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
  PBRSurface pbrSurface = MakePBRSurface(mat._Albedo, roughness, metallic, reflectance);

  vec3 directDiffuse = vec3(0.0);
  vec3 directSpecular = vec3(0.0);
  vec3 legacyDiffuse = vec3(0.0);
  vec3 legacySpecular = vec3(0.0);

  for ( int i = 0; i < u_NbLights; ++i )
  {
    DeferredLightSample lightSample;
    bool hasLightSample = BuildDeferredLightSample(u_Lights[i], hitPoint._Pos, N, u_DirectLightIntensity, lightSample);

    vec3 L;
    if ( u_Lights[i]._Type == DISTANT_LIGHT )
      L = normalize(u_Lights[i]._Pos);
    else
      L = normalize(u_Lights[i]._Pos - hitPoint._Pos);

    float NdotL = max(dot(N, L), 0.0);

    float visibility = 1.0;
    bool hasShadow = false;
    visibility = ComputeShadowForLight(i, int(u_Lights[i]._Type), hitPoint._Pos, N, hasLightSample ? lightSample._L : L, hasShadow);

    if ( hasLightSample )
    {
      vec3 lightDiffuse;
      vec3 lightSpecular;
      EvaluateDeferredPBRLight(pbrSurface, N, V, lightSample, lightDiffuse, lightSpecular);
      directDiffuse += lightDiffuse * visibility * ( 1.0 - specTrans );
      directSpecular += lightSpecular * visibility;
    }

    if ( NdotL > 0.0 )
    {
      float specPower = mix(128.0, 6.0, roughness);
      vec3 H = normalize(L + V);
      float NdotH = max(dot(N, H), 0.0);
      float specFactor = pow(NdotH, specPower);
      vec3 radiance = u_Lights[i]._Emission * visibility * u_DirectLightIntensity;
      legacyDiffuse += diffuseColor * radiance * NdotL;
      legacySpecular += F0 * radiance * specFactor * NdotL;
    }
  }

  vec3 envSpecular = vec3(0.0);
  if ( u_EnableEnvMap > 0 )
  {
    float lod = roughness * max(u_EnvMapMipCount - 1.0, 0.0);
    vec3 R = reflect(-V, N);
    vec3 prefiltered = SampleEnvMapNoSeamLod(R, lod);
    float NdotV = max(dot(N, V), 0.0);
    vec3 roughFresnel = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NdotV, 5.0);
    vec2 brdf = texture(u_BRDFLUT, vec2(NdotV, roughness)).rg;
    float roughnessFade = 1.0 - smoothstep(u_SpecularIBLMaxRoughness * 0.75, u_SpecularIBLMaxRoughness, roughness);
    envSpecular = prefiltered * (roughFresnel * brdf.x + brdf.y) * roughnessFade;
  }

  vec3 premultipliedColor = mat._Emission * alpha;
  if ( u_EnablePBRDirectLighting != 0 )
    premultipliedColor += directDiffuse * alpha + directSpecular;
  else
    premultipliedColor += legacyDiffuse * alpha + legacySpecular;
  premultipliedColor += envSpecular;

  // Keep purely transmissive surfaces (alpha near 0) visible through additive specular/reflection.
  if ( ( alpha <= 0.001 ) && ( max(max(premultipliedColor.r, premultipliedColor.g), premultipliedColor.b) <= 0.001 ) )
    discard;

  fragColor = vec4( max(premultipliedColor, vec3(0.0)), alpha );
}
