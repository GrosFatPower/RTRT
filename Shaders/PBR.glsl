/*
 *
 */

#ifndef _PBR_GLSL_
#define _PBR_GLSL_

#include Constants.glsl

struct PBRSurface
{
  vec3  _Albedo;
  float _Roughness;
  float _Metallic;
  float _Reflectance;
  vec3  _F0;
};

PBRSurface MakePBRSurface( in vec3 iAlbedo, in float iRoughness, in float iMetallic, in float iReflectance )
{
  PBRSurface surface;
  surface._Albedo = max(iAlbedo, vec3(0.0));
  surface._Roughness = clamp(iRoughness, 0.001, 1.0);
  surface._Metallic = clamp(iMetallic, 0.0, 1.0);
  surface._Reflectance = clamp(iReflectance, 0.0, 1.0);
  surface._F0 = mix(vec3(0.16 * surface._Reflectance * surface._Reflectance), surface._Albedo, surface._Metallic);
  return surface;
}

float PBRDistributionGGX( in float iAlpha, in float iNdotH )
{
  float alpha2 = iAlpha * iAlpha;
  float denom = ( iNdotH * iNdotH * ( alpha2 - 1.0 ) ) + 1.0;
  return alpha2 * INV_PI / max(denom * denom, EPSILON);
}

float PBRGeometrySchlickGGX( in float iNdotX, in float iRoughness )
{
  float r = iRoughness + 1.0;
  float k = ( r * r ) * 0.125;
  return iNdotX / max(iNdotX * (1.0 - k) + k, EPSILON);
}

float PBRGeometrySmith( in float iNdotV, in float iNdotL, in float iRoughness )
{
  return PBRGeometrySchlickGGX(iNdotV, iRoughness) * PBRGeometrySchlickGGX(iNdotL, iRoughness);
}

vec3 PBRFresnelSchlick( in vec3 iF0, in float iVdotH )
{
  float f = pow(clamp(1.0 - iVdotH, 0.0, 1.0), 5.0);
  return iF0 + (vec3(1.0) - iF0) * f;
}

void EvaluatePBRDirect( in PBRSurface iSurface, in vec3 iN, in vec3 iV, in vec3 iL, out vec3 oDiffuse, out vec3 oSpecular )
{
  vec3 H = normalize(iV + iL);

  float NdotV = max(dot(iN, iV), 0.0);
  float NdotL = max(dot(iN, iL), 0.0);
  float NdotH = max(dot(iN, H), 0.0);
  float VdotH = max(dot(iV, H), 0.0);

  float alpha = iSurface._Roughness * iSurface._Roughness;
  float D = PBRDistributionGGX(alpha, NdotH);
  float G = PBRGeometrySmith(NdotV, NdotL, iSurface._Roughness);
  vec3 F = PBRFresnelSchlick(iSurface._F0, VdotH);

  vec3 kd = (vec3(1.0) - F) * (1.0 - iSurface._Metallic);
  oDiffuse = kd * iSurface._Albedo * INV_PI;
  oSpecular = (D * G * F) / max(4.0 * NdotV * NdotL, EPSILON);
}

#endif
