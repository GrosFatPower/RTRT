/*
 *
 */

#ifndef _SAMPLING_GLSL_
#define _SAMPLING_GLSL_

#include Constants.glsl

// ----------------------------------------------------------------------------
// GetLightDirSample
// ----------------------------------------------------------------------------
vec3 GetLightDirSample( in vec3 iSamplePos, in vec3 iLightPos, in float iLightRadius )
{
  vec3 lightSample = iLightPos + RandomUnitVector() * iLightRadius;

  return lightSample - iSamplePos;
}

// ----------------------------------------------------------------------------
// GetLightDirSample
// ----------------------------------------------------------------------------
vec3 GetLightDirSample( in vec3 iSamplePos, in vec3 iLightPos, in vec3 iDirU, in vec3 iDirV )
{
  vec3 lightSample = iLightPos + rand() * iDirU + rand() * iDirV;

  return lightSample - iSamplePos;
}

// ----------------------------------------------------------------------------
// SampleSkybox
// UV mapping : https://en.wikipedia.org/wiki/UV_mapping
// iRayDir should be normalized
// (U,V) = normalized spherical coordinates
// ----------------------------------------------------------------------------
vec3 SampleSkybox( in vec3 iRayDir, in sampler2D iSkyboxTex, in float iSkyboxRotation )
{
  float theta = asin(iRayDir.y);
  float phi   = atan(iRayDir.z, iRayDir.x);
  vec2 uv = vec2(.5f + phi * INV_TWO_PI, .5f - theta * INV_PI) + vec2(iSkyboxRotation, 0.0);

  vec3 skycolor = texture(iSkyboxTex, uv).rgb;
  return skycolor;
}

#endif
