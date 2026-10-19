/*
 * Designed with ChatGPT & MistralAI
 */

#version 450

#include Constants.glsl

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D u_InputImage;
layout(rgba32f, binding = 1) uniform image2D u_InputNormals;
layout(rgba32f, binding = 2) uniform image2D u_InputPos;
layout(rgba32f, binding = 3) uniform image2D u_OutputImage;
layout(rgba32f, binding = 4) uniform image2D u_InputAlbedo;

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
// LoadClampedImage
// ----------------------------------------------------------------------------
vec4 LoadClampedImage( in ivec2 iPixelCoord, in ivec2 iSize )
{
  ivec2 coord = clamp(iPixelCoord, ivec2(0), iSize - ivec2(1));
  return imageLoad(u_InputImage, coord);
}

// ----------------------------------------------------------------------------
// LoadDemodulatedIrradiance
// ----------------------------------------------------------------------------
vec3 LoadDemodulatedIrradiance( in ivec2 iPixelCoord, in ivec2 iSize )
{
  ivec2 coord = clamp(iPixelCoord, ivec2(0), iSize - ivec2(1));
  vec3 color = imageLoad(u_InputImage, coord).rgb;
  if ( 0 == u_PassIndex )
  {
    // 3x3 Order-Statistic Firefly Clamping (excludes the 2 brightest neighbors so paired fireflies don't survive)
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
        ivec2 nc = clamp(coord + ivec2(dx, dy), ivec2(0), iSize - ivec2(1));
        vec3 c = imageLoad(u_InputImage, nc).rgb;
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

    vec3 albedo = max(imageLoad(u_InputAlbedo, coord).rgb, vec3(0.04));
    return color / albedo;
  }
  return color;
}

// ----------------------------------------------------------------------------
// Soft thresholding function (similar to wavelet denoising)
// ----------------------------------------------------------------------------
vec4 SoftThreshold( in vec4 iValue, in float iThreshold )
{
  return sign(iValue) * max(abs(iValue) - iThreshold, 0.0);
}

// ----------------------------------------------------------------------------
// SingleScaleDenoiser
// ----------------------------------------------------------------------------
vec4 SingleScaleDenoiser( in ivec2 iPixelCoord, in ivec2 iImageSize )
{
  // Load the original color
  vec4 c00 = LoadClampedImage(iPixelCoord, iImageSize);
  vec4 c10 = LoadClampedImage(iPixelCoord + ivec2(1, 0), iImageSize);
  vec4 c01 = LoadClampedImage(iPixelCoord + ivec2(0, 1), iImageSize);
  vec4 c11 = LoadClampedImage(iPixelCoord + ivec2(1, 1), iImageSize);

  // Haar Wavelet Transform (Decomposition)
  vec4 average          = (c00 + c10 + c01 + c11) * 0.25;
  vec4 horizontalDetail = (c00 + c10 - c01 - c11) * INV_SQRT2 * 0.5;
  vec4 verticalDetail   = (c00 - c10 + c01 - c11) * INV_SQRT2 * 0.5;
  vec4 diagonalDetail   = (c00 - c10 - c01 + c11) * INV_SQRT2 * 0.5;

  // Soft-thresholding (shrink small wavelet coefficients)
  horizontalDetail = SoftThreshold( horizontalDetail, u_Threshold );
  verticalDetail   = SoftThreshold( verticalDetail, u_Threshold );
  diagonalDetail   = SoftThreshold( diagonalDetail, u_Threshold );

  // Inverse Haar Wavelet Transform (Reconstruction)
  vec4 restored_c00 = average + horizontalDetail + verticalDetail + diagonalDetail;
  vec4 restored_c10 = average + horizontalDetail - verticalDetail - diagonalDetail;
  vec4 restored_c01 = average - horizontalDetail + verticalDetail - diagonalDetail;
  vec4 restored_c11 = average - horizontalDetail - verticalDetail + diagonalDetail;

  // Denoised result
  vec4 finalColor = (restored_c00 + restored_c10 + restored_c01 + restored_c11) * 0.25;
  return finalColor;
}

// ----------------------------------------------------------------------------
// MultiScaleDenoiser
// ----------------------------------------------------------------------------
vec4 MultiScaleDenoiser( in ivec2 iPixelCoord, in ivec2 iImageSize )
{
  vec4 finalColor = vec4(0.);

  for ( int level = 0; level < u_WaveletScale; level++ )
  {
    int step = 1 << level; // Scaling factor for the current level

    // Fetch 2x2 neighborhood
    ivec2 coord1 = iPixelCoord;
    ivec2 coord2 = clamp(iPixelCoord + ivec2(step, 0),    ivec2(0), iImageSize - ivec2(1));
    ivec2 coord3 = clamp(iPixelCoord + ivec2(0, step),    ivec2(0), iImageSize - ivec2(1));
    ivec2 coord4 = clamp(iPixelCoord + ivec2(step, step), ivec2(0), iImageSize - ivec2(1));

    vec4 c00 = imageLoad(u_InputImage, coord1);
    vec4 c10 = imageLoad(u_InputImage, coord2);
    vec4 c01 = imageLoad(u_InputImage, coord3);
    vec4 c11 = imageLoad(u_InputImage, coord4);

    // Haar Transform (Decomposition)
    vec4 LL = (c00 + c10 + c01 + c11) * 0.25;
    vec4 LH = (c00 + c10 - c01 - c11) * INV_SQRT2 * 0.5;
    vec4 HL = (c00 - c10 + c01 - c11) * INV_SQRT2 * 0.5;
    vec4 HH = (c00 - c10 - c01 + c11) * INV_SQRT2 * 0.5;

    // Apply noise thresholding to high frequencies
    LH = SoftThreshold( LH, u_Threshold );
    HL = SoftThreshold( HL, u_Threshold );
    HH = SoftThreshold( HH, u_Threshold );

    // Haar Inverse Transform (Reconstruction)
    vec4 c00_reconstructed = LL + LH + HL + HH;
    vec4 c10_reconstructed = LL + LH - HL - HH;
    vec4 c01_reconstructed = LL - LH + HL - HH;
    vec4 c11_reconstructed = LL - LH - HL + HH;

    finalColor += (c00_reconstructed + c10_reconstructed + c01_reconstructed + c11_reconstructed) * 0.25;
  }

  finalColor = finalColor / max(u_WaveletScale, 1);

  return finalColor;
}

// ----------------------------------------------------------------------------
// BilateralFilter
// ----------------------------------------------------------------------------
vec4 BilateralFilter( in ivec2 iPixelCoord, in ivec2 iImageSize )
{
  vec4 centerColor = LoadClampedImage( iPixelCoord, iImageSize );

  vec4 sum = vec4( 0.0 );
  float weightSum = 0.0;
  for ( int y = -2; y <= 2; ++y )
  {
    for ( int x = -2; x <= 2; ++x )
    {
      ivec2 neighborCoord = iPixelCoord + ivec2( x, y );
      vec4 neighborColor = LoadClampedImage( neighborCoord, iImageSize );

      float spatialWeight = exp( -dot( vec2( x, y ), vec2( x, y ) ) / ( 2.0 * u_SigmaSpatial * u_SigmaSpatial ) );
      float rangeWeight = exp( -dot( neighborColor.rgb - centerColor.rgb, neighborColor.rgb - centerColor.rgb ) / ( 2.0 * u_SigmaRange * u_SigmaRange ) );
      float weight = spatialWeight * rangeWeight;

      sum += neighborColor * weight;
      weightSum += weight;
    }
  }

  vec4 denoisedColor = sum / max(weightSum, EPSILON);
  return denoisedColor;
}

// ----------------------------------------------------------------------------
// EdgeAwareDenoiser
// ----------------------------------------------------------------------------
vec4 EdgeAwareDenoiser( in ivec2 iPixelCoord, in ivec2 iImageSize )
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
  
  vec3 cval = LoadDemodulatedIrradiance(iPixelCoord, iImageSize);
  vec3 nval = imageLoad(u_InputNormals, iPixelCoord).rgb;
  vec3 pval = imageLoad(u_InputPos, iPixelCoord).rgb;

  float colorPhi = max(u_ColorPhi * 0.15 * pow(0.5, float(u_PassIndex)), 1e-4);
  float normalPhi = max(u_NormalPhi * 0.05, 1e-4);
  float posPhi = max(u_PositionPhi * 0.01, 1e-5);

  vec3 sum = vec3(0.0);
  float cum_w = 0.0;
  
  for ( uint i = 0; i < 25; ++i )
  {
    ivec2 uv = clamp(iPixelCoord + Offset[i] * u_StepSize, ivec2(0), iImageSize - ivec2(1));
    
    // Demodulated Irradiance / Color
    vec3 ctmp = LoadDemodulatedIrradiance(uv, iImageSize);
    vec3 t = cval - ctmp;
    float dist2 = dot(t, t);
    float c_w = min(exp(-(dist2) / colorPhi), 1.0);
    
    // Normals (strictly preserve normal discontinuities at all step sizes)
    vec3 ntmp = imageLoad(u_InputNormals, uv).rgb;
    t = nval - ntmp;
    dist2 = max(dot(t, t), 0.0);
    float n_w = min(exp(-(dist2) / normalPhi), 1.0);
    
    // Plane-distance + Euclidean position weight
    vec3 ptmp = imageLoad(u_InputPos, uv).rgb;
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
    vec3 albedo = max(imageLoad(u_InputAlbedo, iPixelCoord).rgb, vec3(0.04));
    filtered *= albedo;
  }
  
  return vec4(filtered, 1.f);
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
void main()
{
  ivec2 pixelCoord = ivec2(gl_GlobalInvocationID.xy);
  ivec2 size = imageSize( u_InputImage );

  if ( pixelCoord.x >= size.x || pixelCoord.y >= size.y )
    return; // Do not read/write outside limits

  // Select the denoising method
  if ( 0 == u_DenoisingMethod )
    imageStore(u_OutputImage, pixelCoord, BilateralFilter(pixelCoord, size));
  else if ( 1 == u_DenoisingMethod )
  {
    if ( u_WaveletScale > 1 )
      imageStore(u_OutputImage, pixelCoord, MultiScaleDenoiser(pixelCoord, size));
    else
      imageStore(u_OutputImage, pixelCoord, SingleScaleDenoiser(pixelCoord, size));
  }
  else if ( 2 == u_DenoisingMethod )
    imageStore(u_OutputImage, pixelCoord, EdgeAwareDenoiser(pixelCoord, size));
  else
    imageStore(u_OutputImage, pixelCoord, vec4(0.0));
}
