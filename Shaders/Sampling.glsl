/*
 *
 */

#ifndef _SAMPLING_GLSL_
#define _SAMPLING_GLSL_

#include Constants.glsl
#include Structures.glsl

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
// GetLightDirSample
// ----------------------------------------------------------------------------
vec3 GetLightDirSample( in vec3 iSamplePos, in Light iLight )
{
  vec3 lightSample;

  if ( QUAD_LIGHT == iLight._Type )
    lightSample = GetLightDirSample(iSamplePos, iLight._Pos, iLight._DirU, iLight._DirV);
  else if ( SPHERE_LIGHT == iLight._Type )
    lightSample = GetLightDirSample(iSamplePos, iLight._Pos, iLight._Radius);
  else
    lightSample = iLight._Pos;

  return lightSample;
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

// ----------------------------------------------------------------------------
// SampleEnvMap
// UV mapping : https://en.wikipedia.org/wiki/UV_mapping
// iRayDir should be normalized
// (U,V) = normalized spherical coordinates
// ----------------------------------------------------------------------------
vec4 SampleEnvMap( in vec3 iRayDir, in sampler2D iEnvMap, in float iRotation, in vec2 iRes, float iTotalWeight )
{
  float theta = asin(iRayDir.y);
  float phi   = atan(iRayDir.z, iRayDir.x);
  vec2 uv = vec2(.5f + phi * INV_TWO_PI, .5f - theta * INV_PI) + vec2(iRotation, 0.0);

  vec3 color = texture(iEnvMap, uv).rgb;
  return vec4(color, 0.f); // ToDo
}

// ----------------------------------------------------------------------------
// PowerHeuristic
// The power heuristic for Multiple Importance Sampling
// Wf = ( f ^ 2 ) / ( f ^2 + g ^ 2 )
// ----------------------------------------------------------------------------
float PowerHeuristic(float iF, float iG)
{
  iF *= iF;
  return iF / (iF + iG * iG + EPSILON);
}

// ----------------------------------------------------------------------------
// LightPDF
// ----------------------------------------------------------------------------
float LightPDF( in Light iLight, in Ray iRay )
{
  float pdf = 0.f;

  if ( QUAD_LIGHT == iLight._Type )
  {
    float hitDist = 0.f;
    if ( QuadIntersection(iLight._Pos, iLight._DirU, iLight._DirV, iRay, hitDist) && ( hitDist > 0.f ) )
    {
      vec3 normal = cross(iLight._DirU, iLight._DirV);
      float quadArea = length(normal);
      normal /= quadArea;

      float cosine = abs(dot(normal, -iRay._Dir));
      pdf = ( hitDist * hitDist ) / (cosine * quadArea);
    }
  }
  else if ( SPHERE_LIGHT == iLight._Type )
  {
    vec3 centerToOrig = iRay._Orig - iLight._Pos;
    float distToCenterSq = dot(centerToOrig, centerToOrig);
    float radSq = iLight._Radius * iLight._Radius;
    if ( distToCenterSq < radSq )
    {
      // We are inside the sphere.
      // The solid angle is thus 4*PI and any direction will hit the sphere.
      pdf = 1.f / ( 4.f * PI );
    }
    else
    {
      // We are outside the sphere.
      // The sphere area is therefore visible as a circle with a solid angle of at most 2pi.
      float hitDist = 0.f;
      if ( SphereIntersection(vec4(iLight._Pos, iLight._Radius), iRay, hitDist) && ( hitDist > 0.f ) )
      {
        float discriminant = 1.f - ( radSq / distToCenterSq );
        float cosThetaMax = sqrt(max(0.f, discriminant));
        float solidAngle = TWO_PI * (1.f - cosThetaMax);
        pdf = 1.f / solidAngle;
      }
    }
  }
  else
  {
    // ToDo
  }

  return pdf;
}


#endif
