/*
 *
 */

#ifndef _DEFERRED_PBR_LIGHTING_GLSL_
#define _DEFERRED_PBR_LIGHTING_GLSL_

#include Lights.glsl
#include PBR.glsl

struct DeferredLightSample
{
  vec3  _L;
  vec3  _Radiance;
  float _NdotL;
};

float SphereLightSolidAngle( in float iDistance2, in float iRadius )
{
  float radius2 = iRadius * iRadius;
  float safeDist2 = max(iDistance2, radius2);
  float cosThetaMax = sqrt(max(1.0 - radius2 / safeDist2, 0.0));

  return 2.0 * PI * (1.0 - cosThetaMax);
}

bool BuildDeferredLightSample( in Light iLight, in vec3 iPos, in vec3 iN, in float iDirectIntensity, out DeferredLightSample oSample )
{
  oSample._L = vec3(0.0, 1.0, 0.0);
  oSample._Radiance = vec3(0.0);
  oSample._NdotL = 0.0;

  if ( iLight._Type == DISTANT_LIGHT )
  {
    oSample._L = normalize(iLight._Pos);
    oSample._NdotL = max(dot(iN, oSample._L), 0.0);
    oSample._Radiance = iLight._Emission * iDirectIntensity;
    return ( oSample._NdotL > 0.0 ) && ( max(max(oSample._Radiance.r, oSample._Radiance.g), oSample._Radiance.b) > 0.0 );
  }

  vec3 lightPoint = iLight._Pos;
  float attenuation = 1.0;

  if ( iLight._Type == QUAD_LIGHT )
  {
    vec3 rel = iPos - iLight._Pos;
    float uu = max(dot(iLight._DirU, iLight._DirU), EPSILON);
    float vv = max(dot(iLight._DirV, iLight._DirV), EPSILON);
    float u = clamp(dot(rel, iLight._DirU) / uu, 0.0, 1.0);
    float v = clamp(dot(rel, iLight._DirV) / vv, 0.0, 1.0);
    lightPoint = iLight._Pos + iLight._DirU * u + iLight._DirV * v;
  }

  vec3 toLight = lightPoint - iPos;
  float dist2 = dot(toLight, toLight);
  if ( dist2 <= EPSILON )
    return false;

  oSample._L = toLight * inversesqrt(dist2);
  oSample._NdotL = max(dot(iN, oSample._L), 0.0);
  if ( oSample._NdotL <= 0.0 )
    return false;

  if ( iLight._Type == SPHERE_LIGHT )
  {
    float radius = max(iLight._Radius, 0.001);
    // Approximate the emitter's irradiance from the solid angle it subtends.
    attenuation = SphereLightSolidAngle(dist2, radius);
  }
  else if ( iLight._Type == QUAD_LIGHT )
  {
    vec3 lightNormal = normalize(cross(iLight._DirU, iLight._DirV));
    float facing = max(dot(lightNormal, -oSample._L), 0.0);
    float area = max(iLight._Area, EPSILON);
    attenuation = min((area * facing) / max(dist2, EPSILON), 1.0);
  }

  oSample._Radiance = iLight._Emission * attenuation * iDirectIntensity;
  return max(max(oSample._Radiance.r, oSample._Radiance.g), oSample._Radiance.b) > 0.0;
}

void EvaluateDeferredPBRLight( in PBRSurface iSurface, in vec3 iN, in vec3 iV, in DeferredLightSample iLight, out vec3 oDiffuse, out vec3 oSpecular )
{
  vec3 diffuseBRDF;
  vec3 specularBRDF;
  EvaluatePBRDirect(iSurface, iN, iV, iLight._L, diffuseBRDF, specularBRDF);

  oDiffuse = diffuseBRDF * iLight._Radiance * iLight._NdotL;
  oSpecular = specularBRDF * iLight._Radiance * iLight._NdotL;
}

#endif
