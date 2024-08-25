/*
 *
 */

// ----------------------------------------------------------------------------
// GetLightDirSample
// ----------------------------------------------------------------------------
vec3 GetLightDirSample( vec3 iSamplePos, vec3 iLightPos, float iLightRadius )
{
  vec3 lightSample = iLightPos + RandomUnitVector() * iLightRadius;

  return lightSample - iSamplePos;
}

// ----------------------------------------------------------------------------
// GetLightDirSample
// ----------------------------------------------------------------------------
vec3 GetLightDirSample( vec3 iSamplePos, vec3 iLightPos, vec3 iDirU, vec3 iDirV )
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
vec3 SampleSkybox( vec3 iRayDir, sampler2D iSkyboxTex, float iSkyboxRotation )
{
  float theta = asin(iRayDir.y);
  float phi   = atan(iRayDir.z, iRayDir.x);
  vec2 uv = vec2(.5f + phi * INV_TWO_PI, .5f - theta * INV_PI) + vec2(iSkyboxRotation, 0.0);

  vec3 skycolor = texture(iSkyboxTex, uv).rgb;
  return skycolor;
}
