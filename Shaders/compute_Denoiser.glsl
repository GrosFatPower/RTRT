/*
 * Designed with ChatGPT & MistralAI
 */

#version 450

#include Constants.glsl

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D u_InputImage;
layout(rgba32f, binding = 1) uniform image2D u_OutputImage;

uniform float u_Threshold; // Noise threshold (similar to wavelet thresholding)
uniform int u_WaveletScale = 2; // Number of pixels to consider (1, 2, 4, ...)

uniform float u_SigmaSpatial = 2.0;
uniform float u_SigmaSange = 0.1;

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
vec4 SingleScaleDenoiser( in ivec2 iPixelCoord )
{
  // Load the original color
  vec4 c00 = imageLoad(u_InputImage, iPixelCoord);
  vec4 c10 = imageLoad(u_InputImage, iPixelCoord + ivec2(1, 0));
  vec4 c01 = imageLoad(u_InputImage, iPixelCoord + ivec2(0, 1));
  vec4 c11 = imageLoad(u_InputImage, iPixelCoord + ivec2(1, 1));

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

  finalColor = finalColor / u_WaveletScale;

  return finalColor;
}

// ----------------------------------------------------------------------------
// BilateralFilter
// ----------------------------------------------------------------------------
vec4 BilateralFilter( in ivec2 iPixelCoord )
{
  vec4 centerColor = imageLoad( u_InputImage, iPixelCoord );

  vec4 sum = vec4( 0.0 );
  float weightSum = 0.0;
  for ( int y = -2; y <= 2; ++y )
  {
    for ( int x = -2; x <= 2; ++x )
    {
      ivec2 neighborCoord = iPixelCoord + ivec2( x, y );
      vec4 neighborColor = imageLoad( u_InputImage, neighborCoord );

      float spatialWeight = exp( -dot( vec2( x, y ), vec2( x, y ) ) / ( 2.0 * u_SigmaSpatial * u_SigmaSpatial ) );
      float rangeWeight = exp( -dot( neighborColor.rgb - centerColor.rgb, neighborColor.rgb - centerColor.rgb ) / ( 2.0 * u_SigmaSange * u_SigmaSange ) );
      float weight = spatialWeight * rangeWeight;

      sum += neighborColor * weight;
      weightSum += weight;
    }
  }

  vec4 denoisedColor = sum / weightSum;
  return denoisedColor;
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

  vec4 finalColor;
  if ( u_WaveletScale > 1 )
    finalColor = MultiScaleDenoiser(pixelCoord, size);
  else
    finalColor = SingleScaleDenoiser(pixelCoord);
  //finalColor = BilateralFilter(pixelCoord);

  imageStore(u_OutputImage, pixelCoord, finalColor);
}
