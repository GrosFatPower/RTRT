/*
 *
 */

#ifndef _BRDF_GLSL_
#define _BRDF_GLSL_

#include Constants.glsl

// ----------------------------------------------------------------------------
// DistributionGGX
// GGX/Trowbridge-Reitz : Normal Distribution Function
// Larger the more micro-facets are aligned to H
// NDF = alpha^2 / ( PI * ( (n.h)^2  * ( aplha^2 -1 ) + 1 )^2 )
// ----------------------------------------------------------------------------
float DistributionGGX( in float iAlpha, in float iNdotH )
{
  float alphaSquared = iAlpha * iAlpha;

  float denom = (iNdotH * iNdotH * (alphaSquared - 1.f)) + 1.f;
  if ( 0. == denom )
    denom = EPSILON;

  return alphaSquared * INV_PI / ( denom * denom );
}

// ----------------------------------------------------------------------------
// G1
// Schlick-Beckmann Geometry Shadowing Function
// G_SchilickGGX = (n.v) / ( (n.v) * ( 1 - k ) + k )
// with k = alpha / 2
// ----------------------------------------------------------------------------
float G1( in float iAlpha, in float iNdotX )
{
  float k = iAlpha / 2.f;
  float denom = ( iNdotX * ( 1.f - k ) ) + k;

  return iNdotX / max(denom, EPSILON);
}

// ----------------------------------------------------------------------------
// GeometrySmith
// Smith Model
// Smaller the more micro-facets are shadowed by other micro-facets
// ----------------------------------------------------------------------------
float GeometrySmith( in float iAlpha, in float iNdotV, in float iNdotL )
{
  return G1(iAlpha, iNdotV) * G1(iAlpha, iNdotL);
}

// ----------------------------------------------------------------------------
// FresnelSchlick
// Fresnel-Schlick Function
// Proportion of specular reflectance
// FSchlick = F0 + (1-F0)(1-(h.v))^5
// ----------------------------------------------------------------------------
vec3 FresnelSchlick( in vec3 iF0, in float iVdotH )
{
  return iF0 + (vec3(1.f) - iF0) * pow(1.f - iVdotH, 5.f);
}

// ----------------------------------------------------------------------------
// DiffuseLambertOrenNayar
// Lambertian Oren-Nayar model for diffusion
// iSigma : roughness^2
// DiffuseLambertOrenNayar = A + B . max(0, cos(phi_R - phi_V)) * sin(aplha) * tan(beta)
// A = 1 - 0.5 * sigma2  / ( sigma2 + 0.33 )
// B = 0.45 * sigma2 / ( sigma2 + 0.09 )
// alpha = max(theha_I, theta_V)
// beta = min(theha_I, theta_V)
// ----------------------------------------------------------------------------
float DiffuseLambertOrenNayar( in vec3 iN, in vec3 iV, in vec3 iL, in float iNdotL, in float iNdotV, in float iSigma )
{
 float sigma2 = iSigma * iSigma;
 
 float A = 1.f - ( .5f * sigma2 / ( sigma2 + 0.33f ) );
 float B = 0.45 * sigma2 / ( sigma2 + 0.09 );
 
 float alpha = max(iNdotL, iNdotV);
 float beta = min(iNdotL, iNdotV);
 float cosPhiDiff = max(0.0, dot(iL - iN * iNdotL, iV - iN * iNdotV));
 
 return A + B * cosPhiDiff * sin(alpha) * tan(beta);
}

// ----------------------------------------------------------------------------
// Cook-Torrance Microfacet BRDF
// iN   : surface normal
// iV   : view direction
// iL   : light direction
// iMat : surface material
// ----------------------------------------------------------------------------
vec3 BRDF( in vec3 iN, in vec3 iV, in vec3 iL, in Material iMat )
{
  vec3 H = normalize(iV + iL);

  float NdotV = max(dot(iN, iV), 0.f);
  float NdotL = max(dot(iN, iL), 0.f);
  float VdotH = max(dot(iV, H), 0.f);
  float NdotH = max(dot(iN, H), 0.f);

  float alpha = max(iMat._Roughness, RESOLUTION);
  alpha *= alpha;
  vec3  F = FresnelSchlick(iMat._F0, VdotH);    // Fresnel reflectance (Schlick approximation)
  float D = DistributionGGX(alpha, NdotH);      // Normal distribution function
  float G = GeometrySmith(alpha, NdotV, NdotL); // Geometry term

  // Diffuse
  vec3 Ks = F;
  vec3 Kd = vec3(1.f) - Ks;
  Kd *= ( 1.f - iMat._Metallic ); // pure metals have no diffuse light
  vec3 lambert = iMat._Albedo * INV_PI;
  //vec3 diffuse = Kd * iMat._Albedo * INV_PI * DiffuseLambertOrenNayar(iN, iV, iL, NdotL, NdotV, alpha);

  // Specular (Cook-Torrance)
  vec3 cookTorranceNum =  D * G * F;
  float cookTorranceDenom = 4.f * NdotV * NdotL; // 4(V.N)(L.N)
  vec3 specular = cookTorranceNum / max(cookTorranceDenom, EPSILON);

  return Kd * lambert + specular; // kd * fDiffuse + ks * fSpecular
  //return diffuse + specular;
}

// ----------------------------------------------------------------------------
// DiffuseLambertianBRDF
// ----------------------------------------------------------------------------
vec3 DiffuseLambertianBRDF( in vec3 iN, in vec3 iV, in vec3 iL, in Material iMat )
{
  return iMat._Albedo * INV_PI;
}

#endif
