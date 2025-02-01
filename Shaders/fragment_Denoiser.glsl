/*
 * Designed with ChatGPT
 */

#ifndef _FRAG_DENOISER_GLSL_
#define _FRAG_DENOISER_GLSL_

// Bilateral filter parameters
uniform float u_SigmaSpatial = 2.0;  // Controls the effect of distance between pixels
uniform float u_SigmaColor   = 0.2;  // Controls the effect of color difference
uniform float u_Threshold    = 0.05; // Noise threshold (similar to wavelet thresholding)


// ----------------------------------------------------------------------------
// Soft thresholding function (similar to wavelet denoising)
// ----------------------------------------------------------------------------
vec3 SoftThreshold( in vec3 iValue, in float iThreshold )
{
  return sign(iValue) * max(abs(iValue) - iThreshold, 0.0);
}

// ----------------------------------------------------------------------------
// BilateralFilter
// ----------------------------------------------------------------------------
vec4 BilateralFilter( in sampler2D iTex, in vec2 iFragCoord, in vec2 iInvRes )
{
  // Get the color of the current pixel
  vec3 centerColor = texture(iTex, iFragCoord * iInvRes).rgb;
  
  // Accumulate weighted color sum
  vec3 sum = vec3(0.0);
  float totalWeight = 0.0;
  
  // 5x5 kernel loop (change the range for a larger kernel)
  for ( int x = -2; x <= 2; ++x )
  {
    for ( int y = -2; y <= 2; ++y )
    {
      // Compute texture offset based on kernel position
      vec2 offset = vec2(x, y);
                
      // Fetch neighboring pixel color
      vec3 sampleColor = texture(iTex, (iFragCoord + offset) * iInvRes).rgb;
      
      // Compute spatial weight (penalizes distant pixels)
      float spatialWeight = exp(-float(x*x + y*y) / (2.0 * u_SigmaSpatial * u_SigmaSpatial));
      
      // Compute color weight (penalizes color differences)
      float colorWeight = exp(-distance(sampleColor, centerColor) / (2.0 * u_SigmaColor * u_SigmaColor));
      
      // Compute total weight (combined spatial and color factors)
      float weight = spatialWeight * colorWeight;
      
      // Accumulate weighted color
      sum += sampleColor * weight;
      totalWeight += weight;
    }
  }
  
  // Normalize and apply soft thresholding
  vec3 filteredColor = sum / totalWeight;
  filteredColor = SoftThreshold(filteredColor, u_Threshold);
  
  // Output final denoised color
  return  vec4(filteredColor, 1.0);
}

#endif
