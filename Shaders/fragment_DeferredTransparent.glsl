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

uniform sampler2D   u_EnvMap;
uniform sampler2D   u_BRDFLUT;
uniform vec2        u_EnvMapRes = vec2(1.0);
uniform float       u_EnvMapRotation = 0.f;
uniform float       u_EnvMapMipCount = 1.0;
uniform int         u_EnableEnvMap = 0;
uniform int         u_EnablePBRDirectLighting = 1;
uniform float       u_DirectLightIntensity = 1.0;
uniform float       u_SpecularIBLMaxRoughness = 0.5;

uniform sampler2D   u_SceneColor;
uniform sampler2D   u_GDepth;
uniform sampler2D   u_GPosition;
uniform mat4        u_View;
uniform mat4        u_Proj;
uniform vec2        u_Resolution = vec2(1.0);
uniform int         u_EnableRefraction = 1;
uniform int         u_RefractionMaxSteps = 48;
uniform float       u_RefractionStepSize = 0.18;
uniform float       u_RefractionMaxDistance = 35.0;
uniform float       u_RefractionThickness = 0.25;
uniform float       u_RefractionEdgeFade = 0.18;
uniform float       u_SceneColorMipCount = 1.0;

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

bool ProjectWorldPos( in vec3 iPos, out vec2 oUV, out vec3 oViewPos )
{
  vec4 viewPos = u_View * vec4(iPos, 1.0);
  vec4 clipPos = u_Proj * viewPos;
  if ( clipPos.w <= 0.0001 )
    return false;

  vec3 ndc = clipPos.xyz / clipPos.w;
  oUV = ndc.xy * 0.5 + 0.5;
  oViewPos = viewPos.xyz;
  return ( oUV.x >= 0.0 ) && ( oUV.y >= 0.0 ) && ( oUV.x <= 1.0 ) && ( oUV.y <= 1.0 );
}

float RefractionEdgeFade( in vec2 iUV )
{
  vec2 distToEdge = min(iUV, 1.0 - iUV);
  return clamp(min(distToEdge.x, distToEdge.y) / max(u_RefractionEdgeFade, 0.0001), 0.0, 1.0);
}

bool IsValidRefractionHit( in vec2 iUV, in vec3 iPrevRayViewPos, in vec3 iRayViewPos, out vec3 oScenePos )
{
  float depth = texture(u_GDepth, iUV).r;
  if ( depth >= 1.0 )
    return false;

  oScenePos = texture(u_GPosition, iUV).xyz;
  vec3 sceneViewPos = ( u_View * vec4(oScenePos, 1.0) ).xyz;
  float prevDelta = sceneViewPos.z - iPrevRayViewPos.z;
  float curDelta = sceneViewPos.z - iRayViewPos.z;
  return ( prevDelta < 0.0 ) && ( curDelta >= 0.0 )
    && ( curDelta <= max(u_RefractionThickness, 0.0001) );
}

vec3 SampleRefractedBackground( in vec3 iPos, in vec3 iDir, in float iRoughness,
                                in vec2 iBaseUV, out float oConfidence )
{
  float lod = iRoughness * iRoughness * max(u_SceneColorMipCount - 1.0, 0.0);
  vec3 baseColor = textureLod(u_SceneColor, iBaseUV, lod).rgb;
  float stepSize = max(u_RefractionStepSize, 0.001);
  float maxDistance = max(u_RefractionMaxDistance, stepSize);
  int maxSteps = clamp(u_RefractionMaxSteps, 4, 128);
  float startDistance = max(u_RefractionThickness * 2.0, stepSize) + stepSize * 0.5;

  vec3 prevPos = iPos + iDir * startDistance;
  vec2 prevUV;
  vec3 prevViewPos;
  if ( !ProjectWorldPos(prevPos, prevUV, prevViewPos) )
  {
    oConfidence = 0.0;
    return baseColor;
  }

  vec2 hitUV = iBaseUV;
  vec3 hitScenePos = vec3(0.0);
  bool hit = false;
  for ( int i = 1; i <= 128; ++i )
  {
    if ( i > maxSteps )
      break;

    float dist = startDistance + float(i) * stepSize;
    if ( dist > maxDistance )
      break;

    vec3 rayPos = iPos + iDir * dist;
    vec2 rayUV;
    vec3 rayViewPos;
    if ( !ProjectWorldPos(rayPos, rayUV, rayViewPos) )
      break;

    if ( IsValidRefractionHit(rayUV, prevViewPos, rayViewPos, hitScenePos) )
    {
      vec3 lo = prevPos;
      vec3 hi = rayPos;
      for ( int refine = 0; refine < 4; ++refine )
      {
        vec3 mid = ( lo + hi ) * 0.5;
        vec2 midUV;
        vec3 midViewPos;
        if ( ProjectWorldPos(mid, midUV, midViewPos)
          && IsValidRefractionHit(midUV, prevViewPos, midViewPos, hitScenePos) )
        {
          hi = mid;
          hitUV = midUV;
        }
        else
          lo = mid;
      }
      hit = true;
      break;
    }

    prevPos = rayPos;
    prevViewPos = rayViewPos;
  }

  if ( hit )
  {
    float edgeFade = RefractionEdgeFade(hitUV);
    float distanceFade = 1.0 - clamp(length(hitScenePos - iPos) / maxDistance, 0.0, 1.0);
    oConfidence = edgeFade * distanceFade;
    return mix(baseColor, textureLod(u_SceneColor, hitUV, lod).rgb, oConfidence);
  }

  if ( u_EnableEnvMap > 0 )
  {
    oConfidence = RefractionEdgeFade(prevUV);
    float envLod = min(lod, max(u_EnvMapMipCount - 1.0, 0.0));
    vec3 environment = SampleEnvMapNoSeamLod(iDir, envLod);
    return mix(baseColor, environment, oConfidence);
  }

  oConfidence = 0.0;
  return baseColor;
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
  float dielectricF0 = pow(( max(mat._IOR, 1.0) - 1.0 ) / ( max(mat._IOR, 1.0) + 1.0 ), 2.0);
  if ( ( u_EnableRefraction != 0 ) && ( specTrans > 0.001 ) )
    F0 = mix(vec3(dielectricF0), mat._Albedo, metallic);
  PBRSurface pbrSurface = MakePBRSurface(mat._Albedo, roughness, metallic, reflectance);
  pbrSurface._F0 = F0;

  vec3 directDiffuse = vec3(0.0);
  vec3 directSpecular = vec3(0.0);
  vec3 legacyDiffuse = vec3(0.0);
  vec3 legacySpecular = vec3(0.0);

  for ( int i = 0; i < u_NbLights; ++i )
  {
    vec3 L;
    if ( u_Lights[i]._Type == DISTANT_LIGHT )
      L = normalize(u_Lights[i]._Pos);
    else
      L = normalize(u_Lights[i]._Pos - hitPoint._Pos);

    float NdotL = max(dot(N, L), 0.0);

    float visibility = 1.0;
    bool hasShadow = false;
    vec3 shadowL = L;
    bool hasLightSample = GetDeferredLightCenterDirection(u_Lights[i], hitPoint._Pos, shadowL);
    visibility = ComputeShadowForLight(i, int(u_Lights[i]._Type), hitPoint._Pos, N, shadowL, hasShadow);

    if ( hasLightSample )
    {
      vec3 lightDiffuse;
      vec3 lightSpecular;
      EvaluateDeferredPBRLight(pbrSurface, hitPoint._Pos, N, V, u_Lights[i], u_DirectLightIntensity, lightDiffuse, lightSpecular);
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

  vec3 premultipliedColor = vec3(0.0);
  if ( ( u_EnableRefraction != 0 ) && ( specTrans > 0.001 ) )
  {
    float coverage = baseAlpha;
    float NdotV = max(dot(N, V), 0.0);
    float fresnel = dielectricF0 + ( 1.0 - dielectricF0 ) * pow(1.0 - NdotV, 5.0);
    vec3 incident = -V;
    vec3 refractedDir = refract(incident, N, 1.0 / max(mat._IOR, 1.0));
    if ( dot(refractedDir, refractedDir) <= 0.0001 )
      refractedDir = reflect(incident, N);

    vec2 sceneUV = gl_FragCoord.xy / max(u_Resolution, vec2(1.0));
    float refractionConfidence = 0.0;
    vec3 refractedBackground = SampleRefractedBackground(hitPoint._Pos, normalize(refractedDir), roughness,
                                                         sceneUV, refractionConfidence);
    vec3 transmission = refractedBackground * mat._Albedo * specTrans * ( 1.0 - fresnel );
    vec3 diffuse = ( u_EnablePBRDirectLighting != 0 ) ? directDiffuse : legacyDiffuse;
    vec3 specular = ( u_EnablePBRDirectLighting != 0 ) ? directSpecular : legacySpecular;
    premultipliedColor = coverage * ( mat._Emission + diffuse + specular + envSpecular + transmission );
    alpha = coverage;
  }
  else
  {
    premultipliedColor = mat._Emission * alpha;
    if ( u_EnablePBRDirectLighting != 0 )
      premultipliedColor += directDiffuse * alpha + directSpecular;
    else
      premultipliedColor += legacyDiffuse * alpha + legacySpecular;
    premultipliedColor += envSpecular;
  }

  // Keep purely transmissive surfaces (alpha near 0) visible through additive specular/reflection.
  if ( ( alpha <= 0.001 ) && ( max(max(premultipliedColor.r, premultipliedColor.g), premultipliedColor.b) <= 0.001 ) )
    discard;

  fragColor = vec4( max(premultipliedColor, vec3(0.0)), alpha );
}
