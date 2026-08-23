#ifndef _ScreenSpaceTrace_
#define _ScreenSpaceTrace_

const int SCREEN_TRACE_HIT        = 0;
const int SCREEN_TRACE_UNSTABLE   = 1;
const int SCREEN_TRACE_OFF_SCREEN = 2;
const int SCREEN_TRACE_NO_HIT     = 3;
const int SCREEN_TRACE_NO_DISPLACEMENT = 4;

struct ScreenTraceResult
{
  int   _Status;
  vec2  _UV;
  vec3  _ScenePos;
  float _Distance;
  float _Confidence;
  int   _Steps;
};

// ----------------------------------------------------------------------------
// SampleScreenTraceDepth
// Samples opaque scene depth and returns its signed offset from the ray.
// iUV selects the G-buffer pixel; iRayViewPos is the ray position in view space.
// ----------------------------------------------------------------------------
bool SampleScreenTraceDepth( in vec2 iUV, in vec3 iRayViewPos,
                             out float oDelta, out vec3 oScenePos )
{
  if ( any(lessThan(iUV, vec2(0.0))) || any(greaterThan(iUV, vec2(1.0))) )
    return false;

  float depth = texture(u_GDepth, iUV).r;
  if ( depth >= 1.0 )
    return false;

  oScenePos = texture(u_GPosition, iUV).xyz;
  vec3 sceneViewPos = ( u_View * vec4(oScenePos, 1.0) ).xyz;
  oDelta = sceneViewPos.z - iRayViewPos.z;
  return true;
}

// ----------------------------------------------------------------------------
// EvaluateScreenTraceRay
// Reconstructs the perspective-correct ray position at a screen-space parameter.
// iT spans the projected endpoints; oUV and oRayViewPos receive the sample point.
// ----------------------------------------------------------------------------
void EvaluateScreenTraceRay( in float iT, in vec2 iPixel0, in vec2 iPixel1,
                             in vec3 iQ0, in vec3 iQ1, in float iK0, in float iK1,
                             out vec2 oUV, out vec3 oRayViewPos )
{
  vec2 pixel = mix(iPixel0, iPixel1, iT);
  float k = mix(iK0, iK1, iT);
  oUV = pixel / max(u_Resolution, vec2(1.0));
  oRayViewPos = mix(iQ0, iQ1, iT) / max(k, 0.000001);
}

// ----------------------------------------------------------------------------
// ScreenTraceDepthStability
// Estimates hit reliability from the local opaque-depth discontinuity.
// iUV is the hit pixel; iThickness defines the accepted view-depth tolerance.
// ----------------------------------------------------------------------------
float ScreenTraceDepthStability( in vec2 iUV, in float iThickness, in vec3 iCenterScenePos )
{
  vec2 texel = 1.0 / max(u_Resolution, vec2(1.0));
  float centerZ = ( u_View * vec4(iCenterScenePos, 1.0) ).z;
  float spread = 0.0;
  const vec2 offsets[4] = vec2[4](vec2(1.0, 0.0), vec2(-1.0, 0.0),
                                  vec2(0.0, 1.0), vec2(0.0, -1.0));
  for ( int i = 0; i < 4; ++i )
  {
    vec2 uv = iUV + offsets[i] * texel;
    if ( any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))
      || ( texture(u_GDepth, uv).r >= 1.0 ) )
      return 0.0;

    vec3 neighborPos = texture(u_GPosition, uv).xyz;
    float neighborZ = ( u_View * vec4(neighborPos, 1.0) ).z;
    spread = max(spread, abs(neighborZ - centerZ));
  }

  float thickness = max(iThickness, 0.0001);
  return 1.0 - smoothstep(thickness, thickness * 4.0, spread);
}

// ----------------------------------------------------------------------------
// TraceOpaqueScreenSpace
// Traces a world-space ray against the opaque G-buffer in pixel-sized steps.
// iMaxSteps bounds cost; stride and bias are pixels; thickness is view-space depth.
// ----------------------------------------------------------------------------
ScreenTraceResult TraceOpaqueScreenSpace( in vec3 iOrigin, in vec3 iDirection,
                                          in float iMaxDistance, in int iMaxSteps,
                                          in float iPixelStride, in float iStartBias,
                                          in float iThickness )
{
  ScreenTraceResult result;
  result._Status = SCREEN_TRACE_NO_HIT;
  result._UV = vec2(0.0);
  result._ScenePos = vec3(0.0);
  result._Distance = 0.0;
  result._Confidence = 0.0;
  result._Steps = 0;

  vec3 view0 = ( u_View * vec4(iOrigin, 1.0) ).xyz;
  vec3 view1 = ( u_View * vec4(iOrigin + iDirection * iMaxDistance, 1.0) ).xyz;
  if ( view1.z > -0.01 )
  {
    float clipT = clamp((-0.01 - view0.z) / max(view1.z - view0.z, 0.000001), 0.0, 1.0);
    view1 = mix(view0, view1, clipT);
  }

  vec4 clip0 = u_Proj * vec4(view0, 1.0);
  vec4 clip1 = u_Proj * vec4(view1, 1.0);
  if ( ( clip0.w <= 0.0001 ) || ( clip1.w <= 0.0001 ) )
  {
    result._Status = SCREEN_TRACE_OFF_SCREEN;
    return result;
  }

  vec2 pixel0 = ( clip0.xy / clip0.w * 0.5 + 0.5 ) * u_Resolution;
  vec2 pixel1 = ( clip1.xy / clip1.w * 0.5 + 0.5 ) * u_Resolution;
  vec2 pixelDelta = pixel1 - pixel0;
  float pixelLength = max(abs(pixelDelta.x), abs(pixelDelta.y));
  float startBias = max(iStartBias, 0.25);
  float stride = max(iPixelStride, 0.25);
  if ( pixelLength <= ( startBias + stride ) )
  {
    result._Status = SCREEN_TRACE_NO_DISPLACEMENT;
    return result;
  }

  float k0 = 1.0 / clip0.w;
  float k1 = 1.0 / clip1.w;
  vec3 q0 = view0 * k0;
  vec3 q1 = view1 * k1;
  int maxSteps = clamp(iMaxSteps, 4, 128);

  float prevT = startBias / pixelLength;
  vec2 prevUV;
  vec3 prevRayViewPos;
  EvaluateScreenTraceRay(prevT, pixel0, pixel1, q0, q1, k0, k1, prevUV, prevRayViewPos);
  float prevDelta = 0.0;
  vec3 prevScenePos = vec3(0.0);
  bool prevValid = SampleScreenTraceDepth(prevUV, prevRayViewPos, prevDelta, prevScenePos);

  for ( int i = 1; i <= 128; ++i )
  {
    if ( i > maxSteps )
      break;

    float pixelDistance = startBias + float(i) * stride;
    float curT = min(pixelDistance / pixelLength, 1.0);
    vec2 curUV;
    vec3 curRayViewPos;
    EvaluateScreenTraceRay(curT, pixel0, pixel1, q0, q1, k0, k1, curUV, curRayViewPos);
    result._Steps = i;

    if ( any(lessThan(curUV, vec2(0.0))) || any(greaterThan(curUV, vec2(1.0))) )
    {
      result._Status = SCREEN_TRACE_OFF_SCREEN;
      return result;
    }

    float curDelta = 0.0;
    vec3 curScenePos = vec3(0.0);
    bool curValid = SampleScreenTraceDepth(curUV, curRayViewPos, curDelta, curScenePos);
    if ( prevValid && curValid && ( prevDelta < 0.0 ) && ( curDelta >= 0.0 ) )
    {
      float lo = prevT;
      float hi = curT;
      float hiDelta = curDelta;
      vec2 hitUV = curUV;
      vec3 hitScenePos = curScenePos;
      vec3 hitRayViewPos = curRayViewPos;
      bool stableBracket = true;
      for ( int refine = 0; refine < 5; ++refine )
      {
        float mid = ( lo + hi ) * 0.5;
        vec2 midUV;
        vec3 midRayViewPos;
        EvaluateScreenTraceRay(mid, pixel0, pixel1, q0, q1, k0, k1, midUV, midRayViewPos);
        float midDelta = 0.0;
        vec3 midScenePos = vec3(0.0);
        if ( !SampleScreenTraceDepth(midUV, midRayViewPos, midDelta, midScenePos) )
        {
          stableBracket = false;
          break;
        }
        if ( midDelta >= 0.0 )
        {
          hi = mid;
          hiDelta = midDelta;
          hitUV = midUV;
          hitScenePos = midScenePos;
          hitRayViewPos = midRayViewPos;
        }
        else
          lo = mid;
      }

      if ( !stableBracket || ( hiDelta > max(iThickness, 0.0001) ) )
      {
        result._Status = SCREEN_TRACE_UNSTABLE;
        return result;
      }

      float stability = ScreenTraceDepthStability(hitUV, iThickness, hitScenePos);
      result._Status = ( stability > 0.0 ) ? SCREEN_TRACE_HIT : SCREEN_TRACE_UNSTABLE;
      result._UV = hitUV;
      result._ScenePos = hitScenePos;
      result._Distance = length(hitRayViewPos - view0);
      result._Confidence = stability;
      return result;
    }

    prevT = curT;
    prevDelta = curDelta;
    prevScenePos = curScenePos;
    prevValid = curValid;
    if ( curT >= 1.0 )
      break;
  }

  return result;
}

#endif /* _ScreenSpaceTrace_ */
