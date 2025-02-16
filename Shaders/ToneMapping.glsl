/*
 *
 */

#ifndef _TONEMAPPING_GLSL_
#define _TONEMAPPING_GLSL_

// ============================================================================
// Uniforms
// ============================================================================

uniform float u_Exposure;
uniform float u_Gamma;

// ----------------------------------------------------------------------------
// Luminance
// ----------------------------------------------------------------------------
float Luminance( vec3 iRGBColor )
{
  const vec3 Luma = vec3( 0.299, 0.587, 0.114 ); // UIT-R BT 601
  //const vec3 Luma = vec3(0.2126f, 0.7152f, 0.0722f); // UIT-R BT 709
  return dot(iRGBColor, Luma);
}

// ----------------------------------------------------------------------------
// ReinhardToneMapping
// ----------------------------------------------------------------------------
vec3 ReinhardToneMapping(in vec3 iColor)
{
  iColor *= u_Exposure;
  return clamp(iColor / (vec3(1.f) + iColor), 0.f, 1.f);
}

// ----------------------------------------------------------------------------
// ReinhardToneMapping_Luminance
// ----------------------------------------------------------------------------
vec3 ReinhardToneMapping_Luminance(in vec3 iColor)
{
  iColor *= u_Exposure;

  float lum = Luminance(iColor);

  return clamp(iColor / (vec3(1.f) + lum), 0.f, 1.f);
}

// ----------------------------------------------------------------------------
// GammaCorrection
// ----------------------------------------------------------------------------
vec3 GammaCorrection(in vec3 iColor)
{
  return pow(iColor, vec3(1. / u_Gamma));
}

#endif /* _TONEMAPPING_GLSL_ */
