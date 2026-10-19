#version 410 core

#include Constants.glsl

in vec2 fragUV;
layout(location = 0) out vec4 fragColor;

uniform sampler2D u_InputImage;
uniform sampler2D u_InputNormals;
uniform sampler2D u_InputPos;
uniform sampler2D u_InputAlbedo;
uniform ivec2     u_ImageSize;

uniform int u_DenoisingMethod = 0; // 0: Bilateral, 1: Wavelet, 2: Edge-aware
uniform int u_StepSize = 1;
uniform int u_PassIndex = 0;
uniform int u_TotalPasses = 1;

uniform float u_Threshold; // Noise threshold (similar to wavelet thresholding)
uniform int u_WaveletScale = 2; // Number of pixels to consider (1, 2, 4, ...)

uniform float u_SigmaSpatial = 2.0f;
uniform float u_SigmaRange   = 0.1f;

uniform float u_ColorPhi    = 0.9f;
uniform float u_NormalPhi   = 0.3f;
uniform float u_PositionPhi = 0.6f;

// ----------------------------------------------------------------------------
// LoadClamped
// ----------------------------------------------------------------------------
vec4 LoadClamped( in sampler2D iTex, in ivec2 iPixelCoord )
{
  ivec2 clampedCoord = clamp(iPixelCoord, ivec2(0), u_ImageSize - ivec2(1));
  return texelFetch(iTex, clampedCoord, 0);
}

// ----------------------------------------------------------------------------
// LoadDemodulatedIrradiance
// ----------------------------------------------------------------------------
vec3 LoadDemodulatedIrradiance( in ivec2 iPixelCoord )
{
  ivec2 coord = clamp(iPixelCoord, ivec2(0), u_ImageSize - ivec2(1));
  vec3 color = LoadClamped(u_InputImage, coord).rgb;
  if ( 0 == u_PassIndex )
  {
    vec3 max1 = vec3(0.0);
    vec3 max2 = vec3(0.0);
    vec3 max3 = vec3(0.0);
    vec3 sum  = vec3(0.0);
    for ( int dy = -1; dy <= 1; ++dy )
    {
      for ( int dx = -1; dx <= 1; ++dx )
      {
        if ( dx == 0 && dy == 0 )
          continue;
        vec3 c = LoadClamped(u_InputImage, coord + ivec2(dx, dy)).rgb;
        sum += c;
        vec3 t1 = max(c, max1);
        vec3 r1 = min(c, max1);
        vec3 t2 = max(r1, max2);
        vec3 r2 = min(r1, max2);
        max3 = max(r2, max3);
        max2 = t2;
        max1 = t1;
      }
    }
    vec3 trimmedMean = max(( sum - max1 - max2 ) / 6.0, vec3(0.0));
    color = min(color, max(trimmedMean * 2.0, max3 * 1.25) + vec3(0.02));

    vec3 albedo = max(LoadClamped(u_InputAlbedo, coord).rgb, vec3(0.04));
    return color / albedo;
  }
  return color;
}

// ----------------------------------------------------------------------------
// SoftThreshold
// ----------------------------------------------------------------------------
vec4 SoftThreshold( in vec4 iValue, in float iThreshold )
{
  return sign(iValue) * max(abs(iValue) - iThreshold, 0.0);
}

// ----------------------------------------------------------------------------
// SingleScaleDenoiser
// ----------------------------------------------------------------------------
vec4 SingleScaleDenoiser( in ivec2 iPixelCoord )
{
  vec4 c00 = LoadClamped(u_InputImage, iPixelCoord);
  vec4 c10 = LoadClamped(u_InputImage, iPixelCoord + ivec2(1, 0));
  vec4 c01 = LoadClamped(u_InputImage, iPixelCoord + ivec2(0, 1));
  vec4 c11 = LoadClamped(u_InputImage, iPixelCoord + ivec2(1, 1));

  vec4 average          = (c00 + c10 + c01 + c11) * 0.25;
  vec4 horizontalDetail = (c00 + c10 - c01 - c11) * INV_SQRT2 * 0.5;
  vec4 verticalDetail   = (c00 - c10 + c01 - c11) * INV_SQRT2 * 0.5;
  vec4 diagonalDetail   = (c00 - c10 - c01 + c11) * INV_SQRT2 * 0.5;

  horizontalDetail = SoftThreshold( horizontalDetail, u_Threshold );
  verticalDetail   = SoftThreshold( verticalDetail, u_Threshold );
  diagonalDetail   = SoftThreshold( diagonalDetail, u_Threshold );

  vec4 restored_c00 = average + horizontalDetail + verticalDetail + diagonalDetail;
  vec4 restored_c10 = average + horizontalDetail - verticalDetail - diagonalDetail;
  vec4 restored_c01 = average - horizontalDetail + verticalDetail - diagonalDetail;
  vec4 restored_c11 = average - horizontalDetail - verticalDetail + diagonalDetail;

  return (restored_c00 + restored_c10 + restored_c01 + restored_c11) * 0.25;
}

// ----------------------------------------------------------------------------
// MultiScaleDenoiser
// ----------------------------------------------------------------------------
vec4 MultiScaleDenoiser( in ivec2 iPixelCoord )
{
  vec4 finalColor = vec4(0.);

  for ( int level = 0; level < u_WaveletScale; level++ )
  {
    int step = 1 << level;

    vec4 c00 = LoadClamped(u_InputImage, iPixelCoord);
    vec4 c10 = LoadClamped(u_InputImage, iPixelCoord + ivec2(step, 0));
    vec4 c01 = LoadClamped(u_InputImage, iPixelCoord + ivec2(0, step));
    vec4 c11 = LoadClamped(u_InputImage, iPixelCoord + ivec2(step, step));

    vec4 LL = (c00 + c10 + c01 + c11) * 0.25;
    vec4 LH = (c00 + c10 - c01 - c11) * INV_SQRT2 * 0.5;
    vec4 HL = (c00 - c10 + c01 - c11) * INV_SQRT2 * 0.5;
    vec4 HH = (c00 - c10 - c01 + c11) * INV_SQRT2 * 0.5;

    LH = SoftThreshold( LH, u_Threshold );
    HL = SoftThreshold( HL, u_Threshold );
    HH = SoftThreshold( HH, u_Threshold );

    vec4 c00_reconstructed = LL + LH + HL + HH;
    vec4 c10_reconstructed = LL + LH - HL - HH;
    vec4 c01_reconstructed = LL - LH + HL - HH;
    vec4 c11_reconstructed = LL - LH - HL + HH;

    finalColor += (c00_reconstructed + c10_reconstructed + c01_reconstructed + c11_reconstructed) * 0.25;
  }

  return finalColor / float(max(u_WaveletScale, 1));
}

// ----------------------------------------------------------------------------
// BilateralFilter
// ----------------------------------------------------------------------------
vec4 BilateralFilter( in ivec2 iPixelCoord )
{
  vec4 centerColor = LoadClamped( u_InputImage, iPixelCoord );

  vec4 sum = vec4( 0.0 );
  float weightSum = 0.0;
  for ( int y = -2; y <= 2; ++y )
  {
    for ( int x = -2; x <= 2; ++x )
    {
      vec4 neighborColor = LoadClamped( u_InputImage, iPixelCoord + ivec2( x, y ) );

      float spatialWeight = exp( -dot( vec2( x, y ), vec2( x, y ) ) / ( 2.0 * u_SigmaSpatial * u_SigmaSpatial ) );
      float rangeWeight = exp( -dot( neighborColor.rgb - centerColor.rgb, neighborColor.rgb - centerColor.rgb ) / ( 2.0 * u_SigmaRange * u_SigmaRange ) );
      float weight = spatialWeight * rangeWeight;

      sum += neighborColor * weight;
      weightSum += weight;
    }
  }

  return sum / max(weightSum, EPSILON);
}

// ----------------------------------------------------------------------------
// EdgeAwareDenoiser
// ----------------------------------------------------------------------------
vec4 EdgeAwareDenoiser( in ivec2 iPixelCoord )
{
  const float Kernel[25] = float[25](
    1.0/256.0, 1.0/64.0, 3.0/128.0, 1.0/64.0, 1.0/256.0,
    1.0/64.0,  1.0/16.0, 3.0/32.0,  1.0/16.0, 1.0/64.0,
    3.0/128.0, 3.0/32.0, 9.0/64.0,  3.0/32.0, 3.0/128.0,
    1.0/64.0,  1.0/16.0, 3.0/32.0,  1.0/16.0, 1.0/64.0,
    1.0/256.0, 1.0/64.0, 3.0/128.0, 1.0/64.0, 1.0/256.0 );

  const ivec2 Offset[25] = ivec2[25](
    ivec2(-2,-2), ivec2(-1,-2), ivec2(0,-2), ivec2(1,-2), ivec2(2,-2),
    ivec2(-2,-1), ivec2(-1,-1), ivec2(0,-1), ivec2(1,-1), ivec2(2,-1),
    ivec2(-2, 0), ivec2(-1, 0), ivec2(0, 0), ivec2(1, 0), ivec2(2, 0),
    ivec2(-2, 1), ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1), ivec2(2, 1),
    ivec2(-2, 2), ivec2(-1, 2), ivec2(0, 2), ivec2(1, 2), ivec2(2, 2) );

  vec3 cval = LoadDemodulatedIrradiance(iPixelCoord);
  vec3 nval = LoadClamped(u_InputNormals, iPixelCoord).rgb;
  vec3 pval = LoadClamped(u_InputPos, iPixelCoord).rgb;

  float colorPhi = max(u_ColorPhi * 0.15 * pow(0.5, float(u_PassIndex)), 1e-4);
  float normalPhi = max(u_NormalPhi * 0.05, 1e-4);
  float posPhi = max(u_PositionPhi * 0.01, 1e-5);

  vec3 sum = vec3(0.0);
  float cum_w = 0.0;

  for ( int i = 0; i < 25; ++i )
  {
    ivec2 uv = iPixelCoord + Offset[i] * u_StepSize;

    vec3 ctmp = LoadDemodulatedIrradiance(uv);
    vec3 t = cval - ctmp;
    float dist2 = dot(t, t);
    float c_w = min(exp(-(dist2) / colorPhi), 1.0);

    vec3 ntmp = LoadClamped(u_InputNormals, uv).rgb;
    t = nval - ntmp;
    dist2 = max(dot(t, t), 0.0);
    float n_w = min(exp(-(dist2) / normalPhi), 1.0);

    vec3 ptmp = LoadClamped(u_InputPos, uv).rgb;
    vec3 dp = pval - ptmp;
    float planeDist1 = abs(dot(nval, dp));
    float planeDist2 = abs(dot(ntmp, dp));
    float planeDist = max(planeDist1, planeDist2);
    float euclidDist2 = dot(dp, dp);
    float pDist2 = planeDist * planeDist + 0.05 * euclidDist2;
    float p_w = ( abs(pval.x) > 1e10 || abs(ptmp.x) > 1e10 )
              ? ( ( abs(pval.x) > 1e10 && abs(ptmp.x) > 1e10 ) ? 1.0 : 0.0 )
              : min(exp(-(pDist2) / posPhi), 1.0);

    float weight = c_w * n_w * p_w * Kernel[i];
    sum += ctmp * weight;
    cum_w += weight;
  }

  vec3 filtered = sum / max(cum_w, EPSILON);
  if ( u_PassIndex == ( u_TotalPasses - 1 ) )
  {
    vec3 albedo = max(LoadClamped(u_InputAlbedo, iPixelCoord).rgb, vec3(0.04));
    filtered *= albedo;
  }

  return vec4(filtered, 1.f);
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
void main()
{
  ivec2 pixelCoord = ivec2(gl_FragCoord.xy);

  if ( 0 == u_DenoisingMethod )
    fragColor = BilateralFilter(pixelCoord);
  else if ( 1 == u_DenoisingMethod )
  {
    if ( u_WaveletScale > 1 )
      fragColor = MultiScaleDenoiser(pixelCoord);
    else
      fragColor = SingleScaleDenoiser(pixelCoord);
  }
  else if ( 2 == u_DenoisingMethod )
    fragColor = EdgeAwareDenoiser(pixelCoord);
  else
    fragColor = vec4(0.0);
}
