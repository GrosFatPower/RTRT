#version 410 core

#include Structures.glsl

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D u_GNormal;
uniform sampler2D u_GPosition;
uniform sampler2D u_GMaterial;
uniform sampler2D u_GDepth;
uniform sampler2D u_SSRSource;

uniform sampler2D u_EnvMap;
uniform int       u_EnableEnvMap = 0;
uniform float     u_EnvMapRotation = 0.0;
uniform vec2      u_EnvMapRes;

uniform Camera u_Camera;
uniform mat4 u_View;
uniform mat4 u_Proj;
uniform vec2 u_Resolution;

uniform int   u_EnableSSR = 0;
uniform int   u_SSRMaxSteps = 48;
uniform float u_SSRStepSize = 0.18;
uniform float u_SSRMaxDistance = 35.0;
uniform float u_SSRThickness = 0.25;
uniform float u_SSRMaxRoughness = 0.55;
uniform float u_SSRFade = 0.18;

const float PI = 3.14159265358979323846;
const float INV_PI = 0.31830988618379067154;
const float INV_TWO_PI = 0.15915494309189533577;

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

float EdgeFade( in vec2 iUV )
{
  float fade = max(u_SSRFade, 0.0001);
  vec2 distToEdge = min(iUV, 1.0 - iUV);
  return clamp(min(distToEdge.x, distToEdge.y) / fade, 0.0, 1.0);
}

float InterleavedGradientNoise( in vec2 iPixel )
{
  vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
  return fract(magic.z * fract(dot(iPixel, magic.xy)));
}

bool IsValidHit( in vec2 iUV, in vec3 iPrevRayViewPos, in vec3 iRayViewPos, out vec3 oScenePos )
{
  float depth = texture(u_GDepth, iUV).r;
  if ( depth >= 1.0 )
    return false;

  oScenePos = texture(u_GPosition, iUV).xyz;
  vec3 sceneViewPos = (u_View * vec4(oScenePos, 1.0)).xyz;
  float prevDelta = sceneViewPos.z - iPrevRayViewPos.z;
  float curDelta = sceneViewPos.z - iRayViewPos.z;
  bool crossedSurface = ( prevDelta < 0.0 ) && ( curDelta >= 0.0 );
  return crossedSurface && ( curDelta <= max(u_SSRThickness, 0.0001) );
}

void main()
{
  fragColor = vec4(0.0);

  float depth = texture(u_GDepth, fragUV).r;
  if ( ( u_EnableSSR == 0 ) || ( depth >= 1.0 ) )
    return;

  vec3 material = texture(u_GMaterial, fragUV).rgb;
  float roughness = clamp(material.r, 0.0, 1.0);
  if ( roughness >= u_SSRMaxRoughness )
    return;

  vec3 pos = texture(u_GPosition, fragUV).xyz;
  vec3 N = normalize(texture(u_GNormal, fragUV).xyz * 2.0 - 1.0);
  vec3 V = normalize(u_Camera._Pos - pos);
  vec3 R = normalize(reflect(-V, N));

  if ( dot(R, N) <= 0.0001 )
    return;

  float stepSize = max(u_SSRStepSize, 0.001);
  float maxDistance = max(u_SSRMaxDistance, stepSize);
  int maxSteps = clamp(u_SSRMaxSteps, 4, 128);

  float jitter = InterleavedGradientNoise(gl_FragCoord.xy);
  float startDistance = max(u_SSRThickness * 2.0, stepSize) + jitter * stepSize;
  vec3 prevPos = pos + R * startDistance;
  vec2 prevUV;
  vec3 prevViewPos;
  if ( !ProjectWorldPos(prevPos, prevUV, prevViewPos) )
    return;

  vec2 hitUV = fragUV;
  vec3 hitViewPos = vec3(0.0);
  vec3 hitScenePos = vec3(0.0);
  bool hit = false;

  for ( int i = 1; i <= 128; ++i )
  {
    if ( i > maxSteps )
      break;

    float dist = startDistance + float(i) * stepSize;
    if ( dist > maxDistance )
      break;

    vec3 rayPos = pos + R * dist;
    vec2 rayUV;
    vec3 rayViewPos;
    if ( !ProjectWorldPos(rayPos, rayUV, rayViewPos) )
      break;

    if ( IsValidHit(rayUV, prevViewPos, rayViewPos, hitScenePos) )
    {
      vec3 lo = prevPos;
      vec3 hi = rayPos;
      for ( int refine = 0; refine < 4; ++refine )
      {
        vec3 mid = (lo + hi) * 0.5;
        vec2 midUV;
        vec3 midViewPos;
        if ( ProjectWorldPos(mid, midUV, midViewPos) && IsValidHit(midUV, prevViewPos, midViewPos, hitScenePos) )
        {
          hi = mid;
          hitUV = midUV;
          hitViewPos = midViewPos;
        }
        else
        {
          lo = mid;
        }
      }
      hit = true;
      break;
    }

    prevPos = rayPos;
    prevViewPos = rayViewPos;
  }

  if ( !hit )
  {
    if ( u_EnableEnvMap != 0 )
      fragColor = vec4(SampleEnvMapNoSeam(R), 0.0);
    return;
  }

  float roughnessFade = 1.0 - smoothstep(u_SSRMaxRoughness * 0.65, u_SSRMaxRoughness, roughness);
  float edgeFade = EdgeFade(hitUV);
  float distanceFade = 1.0 - clamp(length(hitScenePos - pos) / maxDistance, 0.0, 1.0);
  float confidence = roughnessFade * edgeFade * distanceFade;

  vec3 reflection = texture(u_SSRSource, hitUV).rgb;
  fragColor = vec4(reflection, confidence);
}
