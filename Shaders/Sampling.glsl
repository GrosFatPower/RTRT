/*
 *
 */

#ifndef _SAMPLING_GLSL_
#define _SAMPLING_GLSL_

#include Constants.glsl
#include Structures.glsl
#include Intersections.glsl
#include RNG.glsl
#include ToneMapping.glsl

struct LightDirectionSample
{
  vec3  _Direction;
  float _Distance;
  float _Pdf;
};

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
// ComputeTangentFrame
// Tangent frame matching the local UV parameterization, for normal maps.
// ----------------------------------------------------------------------------
void ComputeTangentFrame( in vec3 iNormal, in vec3 iPosition, in vec2 iUV, out vec3 oTangent, out vec3 oBitangent )
{
  vec3 dpdx = dFdx(iPosition);
  vec3 dpdy = dFdy(iPosition);
  vec2 duvdx = dFdx(iUV);
  vec2 duvdy = dFdy(iUV);
  float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;

  if ( abs(determinant) < 0.000001 )
  {
    ComputeOnB(iNormal, oTangent, oBitangent);
    return;
  }

  oTangent = normalize((dpdx * duvdy.y - dpdy * duvdx.y) / determinant);
  oTangent = normalize(oTangent - iNormal * dot(iNormal, oTangent));
  oBitangent = normalize(cross(iNormal, oTangent)) * sign(determinant);
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
// https://www.scratchapixel.com/lessons/3d-basic-rendering/global-illumination-path-tracing/global-illumination-path-tracing-practical-implementation.html
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
// UniformSampleHemisphere
// https://www.scratchapixel.com/lessons/3d-basic-rendering/global-illumination-path-tracing/global-illumination-path-tracing-practical-implementation.html
// ----------------------------------------------------------------------------
vec3 UniformSampleHemisphere( in float iR1, in float iR2 )
{
  float sinTheta = sqrt(max(0.0, 1.0 - iR1 * iR1));
  float phi = TWO_PI * iR2;

  return vec3(sinTheta * cos(phi), sinTheta * sin(phi), iR1);
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
// SampleSphereLightDirection
// ----------------------------------------------------------------------------
bool SampleSphereLightDirection( in vec3 iSamplePos, in vec3 iLightPos, in float iLightRadius, out LightDirectionSample oSample )
{
  vec3 toCenter = iLightPos - iSamplePos;
  float dist2 = dot(toCenter, toCenter);
  float radius = max(iLightRadius, RESOLUTION);
  float radius2 = radius * radius;

  if ( dist2 <= radius2 )
  {
    oSample._Direction = UniformSampleSphere();
    oSample._Pdf = INV_4_PI;
  }
  else
  {
    vec3 centerDir = toCenter * inversesqrt(dist2);
    float cosThetaMax = sqrt(max(1.f - radius2 / dist2, 0.f));
    float cosTheta = 1.f - rand() * (1.f - cosThetaMax);
    float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));
    float phi = TWO_PI * rand();
    vec3 T, BT;
    ComputeOnB(centerDir, T, BT);
    oSample._Direction = normalize(T * (sinTheta * cos(phi)) + BT * (sinTheta * sin(phi)) + centerDir * cosTheta);
    oSample._Pdf = 1.f / max(TWO_PI * (1.f - cosThetaMax), EPSILON);
  }

  float hitDist = 0.f;
  if ( !SphereIntersection(vec4(iLightPos, radius), Ray(iSamplePos, oSample._Direction), hitDist) || ( hitDist <= 0.f ) )
    return false;

  oSample._Distance = hitDist;
  return true;
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
// SampleLightDirection
// ----------------------------------------------------------------------------
bool SampleLightDirection( in vec3 iSamplePos, in Light iLight, out LightDirectionSample oSample )
{
  if ( QUAD_LIGHT == iLight._Type )
  {
    vec3 lightPos = iLight._Pos + rand() * iLight._DirU + rand() * iLight._DirV;
    vec3 toLight = lightPos - iSamplePos;
    float dist2 = dot(toLight, toLight);
    vec3 lightNormal = cross(iLight._DirU, iLight._DirV);
    float area = length(lightNormal);
    if ( ( dist2 <= EPSILON ) || ( area <= EPSILON ) )
      return false;

    oSample._Distance = sqrt(dist2);
    oSample._Direction = toLight / oSample._Distance;
    lightNormal /= area;
    float cosine = abs(dot(lightNormal, -oSample._Direction));
    if ( cosine <= EPSILON )
      return false;
    oSample._Pdf = dist2 / (cosine * area);
    return true;
  }

  if ( SPHERE_LIGHT == iLight._Type )
    return SampleSphereLightDirection(iSamplePos, iLight._Pos, iLight._Radius, oSample);

  oSample._Direction = normalize(iLight._Pos);
  oSample._Distance = INFINITY;
  oSample._Pdf = 1.f;
  return true;
}

// ----------------------------------------------------------------------------
// GetLightDirSample
// Legacy helpers used by the non-path-traced ray renderer.
// ----------------------------------------------------------------------------
vec3 GetLightDirSample( in vec3 iSamplePos, in vec3 iLightPos, in float iLightRadius )
{
  LightDirectionSample lightSample;
  if ( SampleSphereLightDirection(iSamplePos, iLightPos, iLightRadius, lightSample) )
    return lightSample._Direction * lightSample._Distance;
  return vec3(0.f);
}

vec3 GetLightDirSample( in vec3 iSamplePos, in Light iLight )
{
  LightDirectionSample lightSample;
  if ( SampleLightDirection(iSamplePos, iLight, lightSample) )
    return lightSample._Direction * lightSample._Distance;
  return vec3(0.f);
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
    float radius = max(iLight._Radius, RESOLUTION);
    float radSq = radius * radius;
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
      if ( SphereIntersection(vec4(iLight._Pos, radius), iRay, hitDist) && ( hitDist > 0.f ) )
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
    vec3 lightDir = normalize(iLight._Pos);
    pdf = ( dot(lightDir, iRay._Dir) > 0.999999f ) ? 1.f : 0.f;
  }

  return pdf;
}


#endif
