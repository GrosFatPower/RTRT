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
uniform float u_SSRPixelStride = 1.0;
uniform float u_SSRStartBias = 1.0;
uniform float u_SSRMaxDistance = 35.0;
uniform float u_SSRThickness = 0.25;
uniform float u_SSRMaxRoughness = 0.55;
uniform float u_SSRFade = 0.18;

const float PI = 3.14159265358979323846;
const float INV_PI = 0.31830988618379067154;
const float INV_TWO_PI = 0.15915494309189533577;

#include ScreenSpaceTrace.glsl

// ----------------------------------------------------------------------------
// EnvMapUV
// Converts a world-space direction to the rotated latitude-longitude environment UV.
// iDir is expected to be normalized.
// ----------------------------------------------------------------------------
vec2 EnvMapUV( in vec3 iDir )
{
  float theta = acos(clamp(iDir.y, -1.0, 1.0));
  float phi   = atan(iDir.z, iDir.x);
  return vec2((PI + phi) * INV_TWO_PI, theta * INV_PI) + vec2(u_EnvMapRotation, 0.0);
}

// ----------------------------------------------------------------------------
// SampleEnvMapNoSeam
// Samples the base environment mip while avoiding longitude seam bleeding.
// iDir selects the normalized reflection direction.
// ----------------------------------------------------------------------------
vec3 SampleEnvMapNoSeam( in vec3 iDir )
{
  vec2 uv = EnvMapUV(iDir);
  uv.x = fract(uv.x);
  vec2 texel = 1.0 / max(u_EnvMapRes, vec2(1.0));
  uv.x = clamp(uv.x, texel.x, 1.0 - texel.x);
  uv.y = clamp(uv.y, texel.y, 1.0 - texel.y);
  return texture(u_EnvMap, uv).rgb;
}

// ----------------------------------------------------------------------------
// EdgeFade
// Computes SSR confidence near the screen boundary.
// iUV is normalized screen space; the result ranges from zero to one.
// ----------------------------------------------------------------------------
float EdgeFade( in vec2 iUV )
{
  float fade = max(u_SSRFade, 0.0001);
  vec2 distToEdge = min(iUV, 1.0 - iUV);
  return clamp(min(distToEdge.x, distToEdge.y) / fade, 0.0, 1.0);
}

// ----------------------------------------------------------------------------
// main
// Traces and outputs the screen-space reflection color and confidence.
// G-buffer material roughness gates tracing before the shared traversal runs.
// ----------------------------------------------------------------------------
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

  float maxDistance = max(u_SSRMaxDistance, 0.001);
  ScreenTraceResult trace = TraceOpaqueScreenSpace(pos, R, maxDistance,
    u_SSRMaxSteps, u_SSRPixelStride, u_SSRStartBias, u_SSRThickness);
  if ( SCREEN_TRACE_HIT != trace._Status )
    return;

  float roughnessFade = 1.0 - smoothstep(u_SSRMaxRoughness * 0.65, u_SSRMaxRoughness, roughness);
  float edgeFade = EdgeFade(trace._UV);
  float distanceFade = 1.0 - clamp(trace._Distance / maxDistance, 0.0, 1.0);
  float confidence = roughnessFade * edgeFade * distanceFade * trace._Confidence;

  vec3 reflection = texture(u_SSRSource, trace._UV).rgb;
  fragColor = vec4(reflection, confidence);
}
