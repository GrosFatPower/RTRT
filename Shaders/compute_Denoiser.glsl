/*
 * Designed with ChatGPT
 */

#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D u_InputImage;
layout(rgba32f, binding = 1) uniform image2D u_OutputImage;

uniform float u_Threshold; // Noise threshold (similar to wavelet thresholding)
uniform int u_WaveletScale = 2; // Number of pixels to consider (1, 2, 4, ...)

// Haar wavelet transform coefficients
const float SQRT2_INV = 0.70710678118; // 1 / sqrt(2)

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
  vec4 horizontalDetail = (c00 + c10 - c01 - c11) * SQRT2_INV * 0.5;
  vec4 verticalDetail   = (c00 - c10 + c01 - c11) * SQRT2_INV * 0.5;
  vec4 diagonalDetail   = (c00 - c10 - c01 + c11) * SQRT2_INV * 0.5;

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
    vec4 LH = (c00 + c10 - c01 - c11) * SQRT2_INV * 0.5;
    vec4 HL = (c00 - c10 + c01 - c11) * SQRT2_INV * 0.5;
    vec4 HH = (c00 - c10 - c01 + c11) * SQRT2_INV * 0.5;

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

  imageStore(u_OutputImage, pixelCoord, finalColor);
}
