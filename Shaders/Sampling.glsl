/*
 *
 */

#ifndef _SAMPLING_GLSL_
#define _SAMPLING_GLSL_

#include Constants.glsl
#include Structures.glsl
#include RNG.glsl
#include ToneMapping.glsl

// ----------------------------------------------------------------------------
// ComputeONB
// Orthonormal Basis
// ----------------------------------------------------------------------------
void ComputeOnB( in vec3 iN, out vec3 oT, out vec3 oBT )
{
  vec3 up = ( abs( iN.z ) < 0.999 ) ? vec3( 0, 0, 1. ) : vec3( 1., 0, 0 );

  oT = normalize( cross( up, iN ) );
  oBT = cross( iN, oT );
}

// ----------------------------------------------------------------------------
// ComputeTangentSpaceMatrix
// ----------------------------------------------------------------------------
mat3 ComputeTangentSpaceMatrix( in vec3 iN )
{
  vec3 T, BT;
  ComputeOnB(iN, T, BT);

  return mat3(T, BT, iN);
}

// ----------------------------------------------------------------------------
// ToWorld
// ----------------------------------------------------------------------------
vec3 ToWorld(vec3 iT, vec3 iBT, vec3 iN, vec3 iV )
{
  return iV.x * iT + iV.y * iBT + iV.z * iN;
}

// ----------------------------------------------------------------------------
// ToLocal
// ----------------------------------------------------------------------------
vec3 ToLocal(vec3 iT, vec3 iBT, vec3 iN, vec3 iV )
{
 return vec3(dot(iV, iT), dot(iV, iBT), dot(iV, iN));
}

// ----------------------------------------------------------------------------
// SampleHemisphere
// ----------------------------------------------------------------------------
vec3 SampleHemisphere( in vec3 iNormal )
{
  vec3 v;

  while ( true )
  {
    v = RandomVec3();

    float dotProd = dot(v, v);
    if ( ( dotProd < 1.f ) && ( dotProd > EPSILON ) )
    {
      v /= dotProd;
      if ( dot(iNormal,v) < 0.f )
        v *= -1.f;
      break;
    }
  }

  return v;
}

// ----------------------------------------------------------------------------
// UniformSampleHemisphere
// ----------------------------------------------------------------------------
vec3 UniformSampleHemisphere()
{
  float r1 = rand();
  float r2 = rand();

  float r = sqrt(max(0.0, 1.0 - r1 * r1));
  float phi = TWO_PI * r2;

  return vec3(r * cos(phi), r * sin(phi), r1);
}

// ----------------------------------------------------------------------------
// CosineSampleHemisphere
// ----------------------------------------------------------------------------
vec3 CosineSampleHemisphere( in float iR1, in float iR2 )
{
  vec3 dir;

  float r = sqrt(iR1);
  float phi = TWO_PI * iR2;

  dir.x = r * cos(phi);
  dir.y = r * sin(phi);
  dir.z = sqrt(max(0.f, 1.f - ( dir.x * dir.x ) - ( dir.y * dir.y )));

  return dir;
}

// ----------------------------------------------------------------------------
// UniformSampleonOrientedHemisphere
// ----------------------------------------------------------------------------
vec3 UniformSampleonOrientedHemisphere( in vec3 iNormal )
{
  vec3 localDir = UniformSampleHemisphere();

  mat3 tangentToWorld = ComputeTangentSpaceMatrix(iNormal);

  return tangentToWorld * localDir;
}

// ----------------------------------------------------------------------------
// UniformSampleonOrientedHemisphere
// ----------------------------------------------------------------------------
vec3 UniformSampleonOrientedHemisphere( in vec3 iNormal, in vec3 iT, in vec3 iBT )
{
  vec3 localDir = UniformSampleHemisphere();

  return mat3(iT, iBT, iNormal) * localDir;
}

// ----------------------------------------------------------------------------
// UniformSampleSphere
// ----------------------------------------------------------------------------
vec3 UniformSampleSphere()
{
  float r1 = rand();
  float r2 = rand();

  float z = 1.0 - 2.0 * r1;
  float r = sqrt(max(0.0, 1.0 - z * z));
  float phi = TWO_PI * r2;

  return vec3(r * cos(phi), r * sin(phi), z);
}

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
  float theta = acos(iRayDir.y);
  float phi   = atan(iRayDir.z, iRayDir.x);
  vec2 uv = vec2((PI + phi) * INV_TWO_PI, theta * INV_PI) + vec2(iSkyboxRotation, 0.0);
  
  return texture(iSkyboxTex, uv).rgb;
}

// ----------------------------------------------------------------------------
// SampleEnvMap
// UV mapping : https://en.wikipedia.org/wiki/UV_mapping
// iRayDir should be normalized
// (U,V) = normalized spherical coordinates
// ----------------------------------------------------------------------------
vec4 SampleEnvMap( in vec3 iRayDir, in sampler2D iEnvMap, in float iRotation, in vec2 iEnvMapRes, float iTotalWeight )
{
  float theta = acos(iRayDir.y);
  float phi   = atan(iRayDir.z, iRayDir.x);
  vec2 uv = vec2((PI + phi) * INV_TWO_PI, theta * INV_PI) + vec2(iRotation, 0.0);
  
  vec3 color = texture(iEnvMap, uv).rgb;
  float pdf = 0.;
  if ( sin( theta ) != 0. )
  {
    pdf = Luminance(color) / iTotalWeight;
    pdf *= ( iEnvMapRes.x * iEnvMapRes.y ) / ( TWO_PI * PI * sin(theta) );
  }
  
  return vec4(color, pdf);
}

// ----------------------------------------------------------------------------
// BinarySearch
// ----------------------------------------------------------------------------
vec2 BinarySearch( in float iValue, in sampler2D iEnvMap, in vec2 iEnvMapRes, in sampler2D iEnvMapCDF )
{
  ivec2 envMapRes = ivec2(iEnvMapRes);

  int lower = 0;
  int upper = envMapRes.y - 1;
  while ( lower < upper )
  {
    int mid = ( lower + upper ) >> 1;
    if ( iValue < texelFetch( iEnvMapCDF, ivec2( envMapRes.x - 1, mid ), 0 ).r )
      upper = mid;
    else
      lower = mid + 1;
  }
  int y = clamp( lower, 0, envMapRes.y - 1 );

  lower = 0;
  upper = envMapRes.x - 1;
  while ( lower < upper )
  {
    int mid = ( lower + upper ) >> 1;
    if ( iValue < texelFetch( iEnvMapCDF, ivec2( mid, y ), 0 ).r )
      upper = mid;
    else
      lower = mid + 1;
  }

  int x = clamp( lower, 0, envMapRes.x - 1 );

  return vec2( x, y ) / iEnvMapRes;
}

// ----------------------------------------------------------------------------
// SampleEnvMap
// ----------------------------------------------------------------------------
vec4 SampleEnvMap( in sampler2D iEnvMap, in float iRotation, in vec2 iEnvMapRes, in sampler2D iEnvMapCDF, in float iTotalWeight, out vec3 oDir )
{
  vec2 uv = BinarySearch(rand() * iTotalWeight, iEnvMap, iEnvMapRes, iEnvMapCDF);
  
  float phi = ( uv.x - iRotation ) * TWO_PI;
  float theta = uv.y * PI;
  oDir.x = -sin(theta) * cos(phi);
  oDir.y = cos(theta);
  oDir.z = -sin(theta) * sin(phi);

  vec3 color = texture(iEnvMap, uv).rgb;

  float pdf = 0.;
  if ( sin( theta ) != 0. )
  {
    pdf = Luminance(color) / iTotalWeight;
    pdf *= ( iEnvMapRes.x * iEnvMapRes.y ) / ( TWO_PI * PI * sin(theta) );
  }

  return vec4(color, pdf);
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
    pdf = 1.f;
  }

  return pdf;
}


#endif
