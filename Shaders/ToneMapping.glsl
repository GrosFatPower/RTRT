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
// ReinhardToneMapping
// ----------------------------------------------------------------------------
vec3 ReinhardToneMapping(in vec3 iColor)
{
  iColor *= u_Exposure;
  return clamp(iColor / (vec3(1.f) + iColor), 0.f, 1.f);
}

// ----------------------------------------------------------------------------
// GammaCorrection
// ----------------------------------------------------------------------------
vec3 GammaCorrection(in vec3 iColor)
{
  return pow(iColor, vec3(1. / u_Gamma));
}

#endif /* _TONEMAPPING_GLSL_ */
