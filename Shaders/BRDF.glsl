/*
 *
 */


// ----------------------------------------------------------------------------
// D
// GGX/Trowbridge-Reitz : Normal Distribution Function
// Larger the more micro-facets are aligned to H
// NDF = alpha^2 / ( PI * ( (n.h)^2  * ( aplha^2 -1 ) + 1 )^2 )
// ----------------------------------------------------------------------------
float D_GGX( in float iAlpha, in vec3 iN, in vec3 iH )
{
  float num = pow(iAlpha, 2.f);

  float NdotH = max(dot(iN, iH), 0.f);
  float denom = PI * pow(pow(NdotH, 2.f) * (pow(iAlpha, 2.f) - 1.f) + 1.f, 2.f);

  return num / max(denom, EPSILON);
}

// ----------------------------------------------------------------------------
// G1
// Schlick-Beckmann Geometry Shadowing Function
// G_SchilickGGX = (n.v) / ( (n.v) * ( 1 - k ) + k )
// with k = alpha / 2
// ----------------------------------------------------------------------------
float G1( in float iAlpha, in vec3 iN, in vec3 iX )
{
  float NdotX = max(dot(iN, iX), 0.f);

  float num = NdotX;

  float k = iAlpha / 2.f;
  float denom = NdotX * ( 1.f - k ) + k;

  return num / max(denom, EPSILON);
}

// ----------------------------------------------------------------------------
// G
// Smith Model
// Smaller the more micro-facets are shadowed by other micro-facets
// ----------------------------------------------------------------------------
float G_Smith( in float iAlpha, in vec3 iN, in vec3 iV, in vec3 iL )
{
  return G1(iAlpha, iN, iV) * G1(iAlpha, iN, iL);
}

// ----------------------------------------------------------------------------
// FSchlick
// Fresnel-Schlick Function
// Proportion of specular reflectance
// FSchlick = F0 + (1-F0)(1-(h.v))^5
// ----------------------------------------------------------------------------
vec3 FSchlick( in vec3 iF0, in vec3 iV, in vec3 iH )
{
  return iF0 + (vec3(1.f) - iF0) * pow(1.f - max(dot(iV, iH), 0.f), 5.f);
}


// ----------------------------------------------------------------------------
// Cook-Torrance Microfacet BRDF
// iN   : surface normal
// iV   : view direction
// iL   : light direction
// iF0  : base reflectance
// iMat : surface material
// ----------------------------------------------------------------------------
vec3 BRDF( in vec3 iN, in vec3 iV, in vec3 iL, in vec3 iF0, in Material iMat )
{
  vec3 H = normalize(iV + iL);

  float alpha = pow(iMat._Roughness, 2.f);
  vec3  F = FSchlick(iF0, iV, H);       // Fresnel reflectance (Schlick approximation)
  float D = D_GGX(alpha, iN, H);        // Normal distribution function
  float G = G_Smith(alpha, iN, iV, iL); // Geometry term

  // Diffuse
  vec3 Ks = F;
  vec3 Kd = vec3(1.f) - Ks;
  Kd *= ( 1.f - iMat._Metallic ); // pure metals have no diffuse light
  vec3 lambert = iMat._Albedo * INV_PI;

  // Specular (Cook-Torrance)
  vec3 cookTorranceNum =  D * G * F;
  float cookTorranceDenom = 4.f * max(dot(iV, iN), 0.f) * max(dot(iL, iN), 0.f); // 4(V.N)(L.N)
  vec3 specular = cookTorranceNum / max(cookTorranceDenom, EPSILON);

  return Kd * lambert + specular; // kd * fDiffuse + ks * fSpecular
}
