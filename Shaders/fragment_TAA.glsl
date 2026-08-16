#version 410 core

in vec2 fragUV;
layout(location = 0) out vec4 fragColor;
layout(location = 1) out float fragDepth;
layout(location = 2) out vec4 fragNormal;
layout(location = 3) out vec4 fragDebug;

uniform sampler2D u_CurrentColor;
uniform sampler2D u_CurrentDepth;
uniform sampler2D u_CurrentNormal;
uniform sampler2D u_Velocity;
uniform sampler2D u_HistoryColor;
uniform sampler2D u_HistoryDepth;
uniform sampler2D u_HistoryNormal;
uniform int u_HistoryValid;
uniform float u_HistoryWeight;
uniform float u_DepthThreshold;
uniform float u_NormalThreshold;
uniform vec2 u_InvResolution;

vec3 RGBToYCoCg( vec3 iColor )
{
  float y = dot(iColor, vec3(0.25, 0.5, 0.25));
  float co = iColor.r - iColor.b;
  float cg = iColor.g - 0.5 * (iColor.r + iColor.b);
  return vec3(y, co, cg);
}

vec3 YCoCgToRGB( vec3 iColor )
{
  float r = iColor.x - 0.5 * iColor.z + 0.5 * iColor.y;
  float g = iColor.x + 0.5 * iColor.z;
  float b = iColor.x - 0.5 * iColor.z - 0.5 * iColor.y;
  return vec3(r, g, b);
}

void main()
{
  vec4 current = texture(u_CurrentColor, fragUV);
  float depth = texture(u_CurrentDepth, fragUV).r;
  vec4 normal = texture(u_CurrentNormal, fragUV);
  vec2 previousUV = fragUV - texture(u_Velocity, fragUV).rg;
  vec4 debugColor = vec4(0.0);
  bool valid = (u_HistoryValid != 0) && (depth < 1.0) && all(greaterThanEqual(previousUV, vec2(0.0))) && all(lessThanEqual(previousUV, vec2(1.0)));
  if ( !valid )
    debugColor.r = 1.0;

  vec4 history = current;
  if ( valid )
  {
    float historyDepth = texture(u_HistoryDepth, previousUV).r;
    vec3 historyNormal = texture(u_HistoryNormal, previousUV).xyz * 2.0 - 1.0;
    vec3 currentNormal = normal.xyz * 2.0 - 1.0;
    if ( abs(historyDepth - depth) > max(u_DepthThreshold, depth * u_DepthThreshold * 5.0) )
    {
      valid = false;
      debugColor = vec4(1.0, 1.0, 0.0, 1.0);
    }
    else if ( dot(historyNormal, currentNormal) < u_NormalThreshold )
    {
      valid = false;
      debugColor = vec4(0.0, 0.0, 1.0, 1.0);
    }
    else
    {
      history = texture(u_HistoryColor, previousUV);
      vec3 minColor = RGBToYCoCg(current.rgb);
      vec3 maxColor = minColor;
      for ( int y = -1; y <= 1; ++y )
      for ( int x = -1; x <= 1; ++x )
      {
        vec3 sampleColor = RGBToYCoCg(texture(u_CurrentColor, fragUV + vec2(x, y) * u_InvResolution).rgb);
        minColor = min(minColor, sampleColor);
        maxColor = max(maxColor, sampleColor);
      }
      vec3 historyYCoCg = RGBToYCoCg(history.rgb);
      vec3 clampedHistory = clamp(historyYCoCg, minColor, maxColor);
      if ( any(notEqual(historyYCoCg, clampedHistory)) )
        debugColor = vec4(0.0, 1.0, 0.0, 1.0);
      history.rgb = YCoCgToRGB(clampedHistory);
    }
  }

  fragColor = valid ? mix(current, history, u_HistoryWeight) : current;
  fragDepth = depth;
  fragNormal = normal;
  fragDebug = debugColor;
}
