#version 410 core

#include Constants.glsl

in vec2 fragUV;
layout(location = 0) out vec4 fragColor;

uniform sampler2D u_InputImage;
uniform sampler2D u_InputNormals;
uniform sampler2D u_InputPos;
uniform ivec2     u_ImageSize;

uniform int u_DenoisingMethod = 0; // 0: Bilateral, 1: Wavelet, 2: Edge-aware

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
    ivec2(-2,-1), ivec2(-1,-1), ivec2(0,-2), ivec2(1,-1), ivec2(2,-1),
    ivec2(-2, 0), ivec2(-1, 0), ivec2(0, 0), ivec2(1, 0), ivec2(2, 0),
    ivec2(-2, 1), ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1), ivec2(2, 1),
    ivec2(-2, 2), ivec2(-1, 2), ivec2(0, 2), ivec2(1, 2), ivec2(2, 2) );

  vec3 cval = LoadClamped(u_InputImage, iPixelCoord).rgb;
  vec3 nval = LoadClamped(u_InputNormals, iPixelCoord).rgb;
  vec3 pval = LoadClamped(u_InputPos, iPixelCoord).rgb;

  vec3 sum = vec3(0.0);
  float cum_w = 0.0;

  for ( int i = 0; i < 25; ++i )
  {
    ivec2 uv = iPixelCoord + Offset[i];

    vec3 ctmp = LoadClamped(u_InputImage, uv).rgb;
    vec3 t = cval - ctmp;
    float dist2 = dot(t, t);
    float c_w = min(exp(-(dist2) / u_ColorPhi), 1.0);

    vec3 ntmp = LoadClamped(u_InputNormals, uv).rgb;
    t = nval - ntmp;
    dist2 = dot(t, t);
    float n_w = min(exp(-(dist2) / u_NormalPhi), 1.0);

    vec3 ptmp = LoadClamped(u_InputPos, uv).rgb;
    t = pval - ptmp;
    dist2 = dot(t, t);
    float p_w = min(exp(-(dist2) / u_PositionPhi), 1.0);

    float weight = c_w * n_w * p_w * Kernel[i];
    sum += ctmp * weight;
    cum_w += weight;
  }

  return vec4(sum / max(cum_w, EPSILON), 1.f);
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
