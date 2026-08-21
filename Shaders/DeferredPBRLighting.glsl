/*
 *
 */

#ifndef _DEFERRED_PBR_LIGHTING_GLSL_
#define _DEFERRED_PBR_LIGHTING_GLSL_

#include Lights.glsl
#include PBR.glsl

const int DEFERRED_AREA_LIGHT_SAMPLE_COUNT = 8;

float SphereLightSolidAngle( in float iDistance2, in float iRadius )
{
  float radius2 = iRadius * iRadius;
  float safeDist2 = max(iDistance2, radius2);
  float cosThetaMax = sqrt(max(1.0 - radius2 / safeDist2, 0.0));

  return 2.0 * PI * (1.0 - cosThetaMax);
}

vec2 DeferredHammersley( int iIndex )
{
  // Fixed low-discrepancy samples keep the deferred result stable between frames.
  const float radicalInverse[DEFERRED_AREA_LIGHT_SAMPLE_COUNT] = float[DEFERRED_AREA_LIGHT_SAMPLE_COUNT](
    0.0, 0.5, 0.25, 0.75, 0.125, 0.625, 0.375, 0.875 );
  return vec2((float(iIndex) + 0.5) / float(DEFERRED_AREA_LIGHT_SAMPLE_COUNT), radicalInverse[iIndex]);
}

void DeferredComputeBasis( in vec3 iN, out vec3 oT, out vec3 oBT )
{
  vec3 up = ( abs(iN.z) < 0.999 ) ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
  oT = normalize(cross(up, iN));
  oBT = cross(iN, oT);
}

bool GetDeferredLightCenterDirection( in Light iLight, in vec3 iPos, out vec3 oL )
{
  if ( iLight._Type == DISTANT_LIGHT )
  {
    oL = normalize(iLight._Pos);
    return true;
  }

  vec3 toLight = iLight._Pos - iPos;
  float dist2 = dot(toLight, toLight);
  if ( dist2 <= EPSILON )
    return false;
  oL = toLight * inversesqrt(dist2);
  return true;
}

void AccumulateDeferredPBRSample( in PBRSurface iSurface, in vec3 iN, in vec3 iV, in vec3 iL, in vec3 iRadiance,
                                  inout vec3 ioDiffuse, inout vec3 ioSpecular )
{
  float NdotL = max(dot(iN, iL), 0.0);
  if ( NdotL <= 0.0 )
    return;

  vec3 diffuseBRDF;
  vec3 specularBRDF;
  EvaluatePBRDirect(iSurface, iN, iV, iL, diffuseBRDF, specularBRDF);
  ioDiffuse += diffuseBRDF * iRadiance * NdotL;
  ioSpecular += specularBRDF * iRadiance * NdotL;
}

void EvaluateDeferredPBRLight( in PBRSurface iSurface, in vec3 iPos, in vec3 iN, in vec3 iV, in Light iLight,
                               in float iDirectIntensity, out vec3 oDiffuse, out vec3 oSpecular )
{
  oDiffuse = vec3(0.0);
  oSpecular = vec3(0.0);
  vec3 emittedRadiance = iLight._Emission * iDirectIntensity;
  if ( max(max(emittedRadiance.r, emittedRadiance.g), emittedRadiance.b) <= 0.0 )
    return;

  if ( iLight._Type == DISTANT_LIGHT )
  {
    AccumulateDeferredPBRSample(iSurface, iN, iV, normalize(iLight._Pos), emittedRadiance, oDiffuse, oSpecular);
    return;
  }

  if ( iLight._Type == SPHERE_LIGHT )
  {
    vec3 toCenter = iLight._Pos - iPos;
    float dist2 = dot(toCenter, toCenter);
    float radius = max(iLight._Radius, 0.001);
    float radius2 = radius * radius;
    bool inside = dist2 <= radius2;
    vec3 centerDir = inside ? vec3(0.0, 1.0, 0.0) : toCenter * inversesqrt(dist2);
    float cosThetaMax = inside ? -1.0 : sqrt(max(1.0 - radius2 / dist2, 0.0));
    float solidAngle = inside ? (4.0 * PI) : SphereLightSolidAngle(dist2, radius);
    vec3 T, BT;
    DeferredComputeBasis(centerDir, T, BT);

    // Sample the cone subtended by the sphere uniformly in solid angle.
    // Each sample represents solidAngle / sampleCount steradians.
    for ( int sampleIndex = 0; sampleIndex < DEFERRED_AREA_LIGHT_SAMPLE_COUNT; ++sampleIndex )
    {
      vec2 xi = DeferredHammersley(sampleIndex);
      float cosTheta = mix(cosThetaMax, 1.0, xi.x);
      float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));
      float phi = TWO_PI * xi.y;
      vec3 L = normalize(T * (sinTheta * cos(phi)) + BT * (sinTheta * sin(phi)) + centerDir * cosTheta);
      vec3 sampleRadiance = emittedRadiance * (solidAngle / float(DEFERRED_AREA_LIGHT_SAMPLE_COUNT));
      AccumulateDeferredPBRSample(iSurface, iN, iV, L, sampleRadiance, oDiffuse, oSpecular);
    }
    return;
  }

  if ( iLight._Type == QUAD_LIGHT )
  {
    vec3 lightNormal = cross(iLight._DirU, iLight._DirV);
    float normalLength = length(lightNormal);
    if ( normalLength <= EPSILON )
      return;
    lightNormal /= normalLength;
    float area = max(iLight._Area, EPSILON);
    // Uniform area samples are converted to solid angle with cosLight / distance^2.
    for ( int sampleIndex = 0; sampleIndex < DEFERRED_AREA_LIGHT_SAMPLE_COUNT; ++sampleIndex )
    {
      vec2 xi = DeferredHammersley(sampleIndex);
      vec3 lightPos = iLight._Pos + xi.x * iLight._DirU + xi.y * iLight._DirV;
      vec3 toLight = lightPos - iPos;
      float dist2 = dot(toLight, toLight);
      if ( dist2 <= EPSILON )
        continue;
      vec3 L = toLight * inversesqrt(dist2);
      float facing = max(dot(lightNormal, -L), 0.0);
      vec3 sampleRadiance = emittedRadiance * (area * facing / dist2) / float(DEFERRED_AREA_LIGHT_SAMPLE_COUNT);
      AccumulateDeferredPBRSample(iSurface, iN, iV, L, sampleRadiance, oDiffuse, oSpecular);
    }
  }
}

#endif
