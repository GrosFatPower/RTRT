/*
 * This is a modified version of the original code : https://github.com/knightcrawler25/GLSL-PathTracer
 */

#ifndef _DISNEY_BRDF_GLSL_
#define _DISNEY_BRDF_GLSL_

#include Structures.glsl
#include Material.glsl
#include Sampling.glsl
#include ToneMapping.glsl

// ----------------------------------------------------------------------------
// SchlickFresnel
// ???
// ----------------------------------------------------------------------------
float SchlickFresnel( in float iU )
{
  float m = clamp(1.f - iU, 0.f, 1.f);
  float m2 = m * m;
  return m2 * m2 * m;
}

// ----------------------------------------------------------------------------
// DielectricFresnel
// ----------------------------------------------------------------------------
float DielectricFresnel( in float iCosThetaI, in float iEta )
{
  float sinThetaTSq = iEta * iEta * (1.f - iCosThetaI * iCosThetaI );

  // Total internal reflection
  if ( sinThetaTSq > 1.f )
    return 1.f;

  float cosThetaT = sqrt(max(1.f - sinThetaTSq, 0.f));

  float rs = ( ( iEta * cosThetaT ) - iCosThetaI ) / ( ( iEta * cosThetaT ) + iCosThetaI );
  float rp = ( ( iEta * iCosThetaI ) - cosThetaT ) / ( ( iEta * iCosThetaI ) + cosThetaT );

  return .5f * ( rs * rs + rp * rp );
}

// ----------------------------------------------------------------------------
// DisneyFresnel
// ----------------------------------------------------------------------------
float DisneyFresnel( in Material iMat, in float iEta, in float iLDotH, in float iVDotH )
{
  float metallicFresnel = SchlickFresnel(iLDotH);
  float dielectricFresnel = DielectricFresnel(abs(iVDotH), iEta);
  return mix(dielectricFresnel, metallicFresnel, iMat._Metallic);
}

// ----------------------------------------------------------------------------
// EvalDiffuse
// ----------------------------------------------------------------------------
vec3 EvalDiffuse( in Material iMat, in vec3 iCsheen, in vec3 iV, in vec3 iL, in vec3 iH, out float oPdf )
{
  oPdf = 0.f;
  if ( iL.z <= 0.f )
    return vec3(0.f);

  // Diffuse
  float FL = SchlickFresnel(iL.z);
  float FV = SchlickFresnel(iV.z);
  float FH = SchlickFresnel(dot(iL, iH));
  float Fd90 = .5f + 2.f * dot(iL, iH) * dot(iL, iH) * iMat._Roughness;
  float Fd = mix(1.f, Fd90, FL) * mix(1.f, Fd90, FV);

  // Fake Subsurface
  float Fss90 = dot(iL, iH) * dot(iL, iH) * iMat._Roughness;
  float Fss = mix(1.f, Fss90, FL) * mix(1., Fss90, FV);
  float ss = 1.25f * (Fss * (1.f / (iL.z + iV.z) - .5f) + .5f);

  // Sheen
  vec3 Fsheen = FH * iMat._Sheen * iCsheen;

  oPdf = iL.z * INV_PI;
  return (1.f - iMat._Metallic) * (1.f - iMat._SpecularTrans) * (INV_PI * mix(Fd, ss, iMat._Subsurface) * iMat._Albedo + Fsheen);
}

// ----------------------------------------------------------------------------
// SmithG
// ----------------------------------------------------------------------------
float SmithG( in float iNDotV, in float iAlphaG )
{
  float a = iAlphaG * iAlphaG;
  float b = iNDotV * iNDotV;
  return ( 2.f * iNDotV ) / ( iNDotV + sqrt( a + b - ( a * b ) ) );
}

// ----------------------------------------------------------------------------
// SmithGAniso
// ----------------------------------------------------------------------------
float SmithGAniso( in float iNDotV, in float iVDotX, in float iVDotY, in float iAx, in float iAy )
{
  float a = iVDotX * iAx;
  float b = iVDotY * iAy;
  float c = iNDotV;
  return ( 2.0 * iNDotV ) / ( iNDotV + sqrt(a * a + b * b + c * c) );
}

// ----------------------------------------------------------------------------
// GTR1
// ----------------------------------------------------------------------------
float GTR1( in float iNDotH, in float iA )
{
  if ( iA >= 1.f )
    return INV_PI;
  float a2 = iA * iA;
  float t = 1.f + ( a2 - 1.f ) * iNDotH * iNDotH;
  return ( a2 - 1.f ) / ( PI * log(a2) * t );
}

// ----------------------------------------------------------------------------
// GTR1
// ----------------------------------------------------------------------------
vec3 SampleGTR1( in float iRoughness , in float iR1 )
{
  float a = max(RESOLUTION, iRoughness);
  float a2 = a * a;

  float phi = iR1 * TWO_PI;

  float cosTheta = sqrt( (1.f - pow(a2, 1.f - iR1) ) / ( 1.f - a2 ) );
  float sinTheta = clamp(sqrt(1.f - (cosTheta * cosTheta)), 0.f, 1.f);
  float sinPhi = sin(phi);
  float cosPhi = cos(phi);

  return vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

// ----------------------------------------------------------------------------
// GTR2Aniso
// ----------------------------------------------------------------------------
float GTR2Aniso( in float iNDotH, in float iHDotX, in float iHDotY, in float iAx, in float iAy )
{
  float a = iHDotX / iAx;
  float b = iHDotY / iAy;
  float c = ( a * a ) + ( b * b ) + ( iNDotH * iNDotH );
  return 1.f / ( PI * iAx * iAy * c * c );
}

// ----------------------------------------------------------------------------
// SampleGGXVNDF
// ----------------------------------------------------------------------------
vec3 SampleGGXVNDF( in vec3 iV, in float iAx, in float iAy, float iR1, float iR2 )
{
  vec3 Vh = normalize(vec3(iAx * iV.x, iAy * iV.y, iV.z));

  float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
  vec3 T1 = ( lensq > 0.f ) ? ( vec3(-Vh.y, Vh.x, 0.f) * inversesqrt(lensq) ) : ( vec3(1.f, 0.f, 0.f) );
  vec3 T2 = cross(Vh, T1);

  float r = sqrt(iR1);
  float phi = TWO_PI * iR2;
  float t1 = r * cos(phi);
  float t2 = r * sin(phi);
  float s = .5f * (1.f + Vh.z);
  t2 = ( 1.f - s ) * sqrt(1.f - t1 * t1) + ( s * t2 );

  vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.f, 1.f - ( t1 * t1 ) - ( t2 * t2 ))) * Vh;

  return normalize(vec3(iAx * Nh.x, iAy * Nh.y, max(0.f, Nh.z)));
}

// ----------------------------------------------------------------------------
// EvalSpecReflection
// ----------------------------------------------------------------------------
vec3 EvalSpecReflection( in Material iMat, in float iEta, in vec3 iSpecCol, in vec3 iV, in vec3 iL, in vec3 iH, out float oPdf )
{
  oPdf = 0.f;
  if ( iL.z <= 0.f )
    return vec3(0.f);

  float FM = DisneyFresnel(iMat, iEta, dot(iL, iH), dot(iV, iH));
  vec3 F = mix(iSpecCol, vec3(1.f), FM);
  float D = GTR2Aniso(iH.z, iH.x, iH.y, iMat._Ax, iMat._Ay);
  float G1 = SmithGAniso(abs(iV.z), iV.x, iV.y, iMat._Ax, iMat._Ay);
  float G2 = G1 * SmithGAniso(abs(iL.z), iL.x, iL.y, iMat._Ax, iMat._Ay);

  oPdf = G1 * D / (4.f * iV.z);
  return F * D * G2 / (4.f * iL.z * iV.z);
}

// ----------------------------------------------------------------------------
// EvalSpecRefraction
// ----------------------------------------------------------------------------
vec3 EvalSpecRefraction( in Material iMat, in float iEta, in vec3 iV, in vec3 iL, in vec3 iH, out float oPdf )
{
  oPdf = 0.f;
  if ( iL.z >= 0.f )
    return vec3(0.f);

  float F = DielectricFresnel(abs(dot(iV, iH)), iEta);
  float D = GTR2Aniso(iH.z, iH.x, iH.y, iMat._Ax, iMat._Ay);
  float G1 = SmithGAniso(abs(iV.z), iV.x, iV.y, iMat._Ax, iMat._Ay);
  float G2 = G1 * SmithGAniso(abs(iL.z), iL.x, iL.y, iMat._Ax, iMat._Ay);
  float denom = dot(iL, iH) + dot(iV, iH) * iEta;
  denom *= denom;
  float eta2 = iEta * iEta;
  float jacobian = abs(dot(iL, iH)) / denom;

  oPdf = G1 * max(0.f, dot(iV, iH)) * D * jacobian / iV.z;

  return pow(iMat._Albedo, vec3(.5f)) * (1.f - iMat._Metallic) * iMat._SpecularTrans * (1.f - F) * D * G2 * abs(dot(iV, iH)) * jacobian * eta2 / abs(iL.z * iV.z);
}

// ----------------------------------------------------------------------------
// EvalClearcoat
// ----------------------------------------------------------------------------
vec3 EvalClearcoat( in Material iMat, in vec3 iV, in vec3 iL, in vec3 iH, out float oPdf )
{
  oPdf = 0.f;
  if ( iL.z <= 0.f )
    return vec3(0.f);

  float FH = DielectricFresnel(dot(iV, iH), 1.f / 1.5f);
  float F = mix(.04f, 1.f, FH);
  float D = GTR1(iH.z, iMat._ClearcoatRoughness);
  float G = SmithG(iL.z, .25f) * SmithG(iV.z, .25f);
  float jacobian = 1.f / (4.f * dot(iV, iH));

  oPdf = D * iH.z * jacobian;
  return vec3(.25f) * iMat._Clearcoat * F * D * G / (4.f * iL.z * iV.z);
}

// ----------------------------------------------------------------------------
// GetSpecColor
// ----------------------------------------------------------------------------
void GetSpecColor( in Material iMat, in float iEta, out vec3 oSpecCol, out vec3 oSheenCol )
{
  float lum = Luminance(iMat._Albedo);
  vec3 ctint = ( lum > 0.f ) ? ( iMat._Albedo / lum ) : ( vec3(1.f) );
  float F0 = (1.f - iEta) / (1.f + iEta);
  oSpecCol = mix(F0 * F0 * mix(vec3(1.f), ctint, iMat._SpecularTint), iMat._Albedo, iMat._Metallic);
  oSheenCol = mix(vec3(1.f), ctint, iMat._SheenTint);
}

// ----------------------------------------------------------------------------
// GetLobeProbabilities
// ----------------------------------------------------------------------------
void GetLobeProbabilities( in Material iMat, in float iEta, in vec3 iSpecCol, in float iApproxFresnel, out float oDiffuseWeight, out float oSpecReflectWeight, out float oSpecRefractWeight, out float oClearcoatWeight )
{
  oDiffuseWeight     = Luminance(iMat._Albedo) * (1.f - iMat._Metallic) * (1.f - iMat._SpecularTrans);
  oSpecReflectWeight = Luminance(mix(iSpecCol, vec3(1.f), iApproxFresnel));
  oSpecRefractWeight = (1.f - iApproxFresnel) * (1.f - iMat._Metallic) * iMat._SpecularTrans * Luminance(iMat._Albedo);
  oClearcoatWeight   = .25f * iMat._Clearcoat * (1.f - iMat._Metallic);
  float totalWeight = oDiffuseWeight + oSpecReflectWeight + oSpecRefractWeight + oClearcoatWeight;

  oDiffuseWeight     /= totalWeight;
  oSpecReflectWeight /= totalWeight;
  oSpecRefractWeight /= totalWeight;
  oClearcoatWeight   /= totalWeight;
}

// ----------------------------------------------------------------------------
// DisneySample
// ----------------------------------------------------------------------------
vec3 DisneySample( in HitPoint iHP, in Material iMat, in float iEta, in vec3 iV, out vec3 oL, out float oPdf )
{
  oPdf = 0.f;
  vec3 f = vec3(0.f);

  float r1 = rand();
  float r2 = rand();

  vec3 T, BT;
  ComputeOnB(iHP._Normal, T, BT);
  iV = ToLocal(T, BT, iHP._Normal, iV);

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(iMat, iEta, specCol, sheenCol);

  // Lobe weights
  float diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight;
  float approxFresnel = DisneyFresnel(iMat, iEta, iV.z, iV.z); // Note: Fresnel is approx and based on N and not H since H isn't available at this stage.
  GetLobeProbabilities(iMat, iEta, specCol, approxFresnel, diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight);

  // CDF for picking a lobe
  float cdf[4];
  cdf[0] = diffuseWeight;
  cdf[1] = cdf[0] + clearcoatWeight;
  cdf[2] = cdf[1] + specReflectWeight;
  cdf[3] = cdf[2] + specRefractWeight;

  if ( r1 < cdf[0] ) // Diffuse Reflection Lobe
  {
    r1 /= cdf[0];
    //oL = CosineSampleHemisphere(r1, r2);
    oL = UniformSampleHemisphere(r1, r2);

    vec3 H = normalize(oL + iV);

    f = EvalDiffuse(iMat, sheenCol, iV, oL, H, oPdf);
    oPdf *= diffuseWeight;
  }
  else if ( r1 < cdf[1] ) // Clearcoat Lobe
  {
    r1 = ( r1 - cdf[0] ) / ( cdf[1] - cdf[0] );

    vec3 H = SampleGTR1(iMat._ClearcoatRoughness, r1);

    if ( H.z < 0.f )
      H = -H;

    oL = normalize(reflect(-iV, H));

    f = EvalClearcoat(iMat, iV, oL, H, oPdf);
    oPdf *= clearcoatWeight;
  }
  else  // Specular Reflection/Refraction Lobes
  {
    r1 = ( r1 - cdf[1] ) / ( 1.0 - cdf[1] );
    vec3 H = SampleGGXVNDF(iV, iMat._Ax, iMat._Ay, r1, r2);

    if ( H.z < 0.f )
      H = -H;

    float fresnel = DisneyFresnel(iMat, iEta, dot(oL, H), dot(iV, H)); // TODO: Refactor into metallic BRDF and specular BSDF
    float F = 1.f - ((1.f - fresnel) * iMat._SpecularTrans * (1.f - iMat._Metallic));

    if ( rand() < F )
    {
      oL = normalize(reflect(-iV, H));

      f = EvalSpecReflection(iMat, iEta, specCol, iV, oL, H, oPdf);
      oPdf *= F;
    }
    else
    {
      oL = normalize(refract(-iV, H, iEta));

      f = EvalSpecRefraction(iMat, iEta, iV, oL, H, oPdf);
      oPdf *= 1.f - F;
    }

    oPdf *= specReflectWeight + specRefractWeight;
  }

  oL = ToWorld(T, BT, iHP._Normal, oL);
  return f * abs(dot(iHP._Normal, oL));
}

// ----------------------------------------------------------------------------
// DisneyEval
// ----------------------------------------------------------------------------
vec3 DisneyEval( in HitPoint iHP, in Material iMat, in float iEta, in vec3 iV, in vec3 iL, out float oPdf )
{
  oPdf = 0.f;
  vec3 f = vec3(0.f);

  vec3 T, BT;
  ComputeOnB(iHP._Normal, T, BT);
  iV = ToLocal(T, BT, iHP._Normal, iV);
  iL = ToLocal(T, BT, iHP._Normal, iL);

  vec3 H;
  if ( iL.z > 0.f )
    H = normalize(iL + iV);
  else
    H = normalize(iL + iV * iEta);

  if ( H.z < 0.f )
    H = -H;

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(iMat, iEta, specCol, sheenCol);

  // Lobe weights
  float diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight;
  float fresnel = DisneyFresnel(iMat, iEta, dot(iL, H), dot(iV, H));
  GetLobeProbabilities(iMat, iEta, specCol, fresnel, diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight);

  float pdf = 0.f;

  // Diffuse
  if ( ( diffuseWeight > 0.f ) && ( iL.z > 0.f ) )
  {
    f += EvalDiffuse(iMat, sheenCol, iV, iL, H, pdf);
    oPdf += pdf * diffuseWeight;
  }

  // Specular Reflection
  if ( ( specReflectWeight > 0.f ) && ( iL.z > 0.f ) && ( iV.z > 0.f ) )
  {
    f += EvalSpecReflection(iMat, iEta, specCol, iV, iL, H, pdf);
    oPdf += pdf * specReflectWeight;
  }

  // Specular Refraction
  if ( ( specRefractWeight > 0.f ) && ( iL.z < 0.f ) )
  {
    f += EvalSpecRefraction(iMat, iEta, iV, iL, H, pdf);
    oPdf += pdf * specRefractWeight;
  }

  // Clearcoat
  if ( ( clearcoatWeight > 0.f ) && ( iL.z > 0.f ) && ( iV.z > 0.f ) )
  {
    f += EvalClearcoat(iMat, iV, iL, H, pdf);
    oPdf += pdf * clearcoatWeight;
  }

  return f * abs(iL.z);
}

#endif
