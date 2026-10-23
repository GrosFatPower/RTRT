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
// GGXDirectionalAlbedoCoeffs
// Piecewise-linear polynomial coefficients in u = sqrt(cosTheta) for the
// single-scattering GGX directional albedo E0 (F=1), E1 ((1-VdotH)^5), and
// Ediel (DielectricFresnel at IOR=1.5).
// ----------------------------------------------------------------------------
void GGXDirectionalAlbedoCoeffs( in float iRoughness, in float iEta, out float oC0[6], out float oC1[6], out float oCd[6] )
{
  const float rKnots[8] = float[8]( 0.0f, 0.05f, 0.15f, 0.25f, 0.375f, 0.50f, 0.75f, 1.00f );

  const float c0Table[48] = float[48](
    0.983025f,  0.177360f, -0.680226f,  1.215425f, -1.024522f,  0.329166f,
    0.891995f,  1.109645f, -4.205850f,  7.450774f, -6.240130f,  1.994898f,
    0.913979f, -0.778497f,  6.356089f, -14.830786f, 14.327348f, -4.992685f,
    1.093668f, -2.577747f, 10.463347f, -17.243378f, 12.906570f, -3.649855f,
    0.911925f,  0.392541f, -4.447142f, 12.398763f, -12.905666f,  4.626786f,
    0.863185f,  0.609349f, -3.916299f,  7.688230f, -5.929355f,  1.599765f,
    0.789910f,  0.002438f, -0.484328f, -0.073741f,  0.746420f, -0.353908f,
    0.611976f,  0.032272f, -0.817378f,  0.563827f, -0.020454f, -0.063452f
  );

  const float c1Table[48] = float[48](
    0.907012f,  1.474168f, -13.300937f, 20.881095f, -12.147096f,  2.182726f,
    0.798754f,  2.558008f, -17.348647f, 27.987280f, -18.063889f,  4.066699f,
    0.426632f,  3.506480f, -15.021402f, 17.701283f, -6.204770f, -0.414929f,
    0.430963f,  0.202737f,   1.071950f, -11.502676f, 17.165532f, -7.375587f,
    0.283875f, -0.314856f,   2.300399f, -8.899015f, 10.839371f, -4.211449f,
    0.160419f, -0.122712f,   0.742561f, -3.308640f,  4.100395f, -1.572253f,
    0.052501f, -0.002260f,  -0.092161f, -0.212337f,  0.454430f, -0.200184f,
    0.019263f,  0.009213f,  -0.131276f,  0.159891f, -0.056390f, -0.000686f
  );

  const float cdTable[48] = float[48](
    0.939694f,  1.046371f, -13.039773f, 24.468160f, -18.534768f,  5.160894f,
    0.830092f,  2.144563f, -17.140626f, 31.664613f, -24.523781f,  7.066961f,
    0.434506f,  3.399553f, -15.976567f, 23.291420f, -14.109386f,  2.997377f,
    0.429679f,  0.355101f,  -0.746918f, -5.016568f,  9.108552f, -4.094516f,
    0.290600f, -0.070721f,   0.572928f, -3.911482f,  5.178574f, -2.021399f,
    0.183668f,  0.010990f,  -0.095226f, -1.045651f,  1.620861f, -0.637734f,
    0.087437f,  0.006162f,  -0.160460f, -0.042625f,  0.252365f, -0.117202f,
    0.047542f,  0.009960f,  -0.163386f,  0.177391f, -0.058691f, -0.000132f
  );

  float r = clamp(iRoughness, 0.f, 1.f);
  int seg = 0;
  for ( int i = 0; i < 6; ++i )
  {
    if ( r >= rKnots[i + 1] )
      seg = i + 1;
  }
  float t = clamp(( r - rKnots[seg] ) / max(rKnots[seg + 1] - rKnots[seg], 1e-5f), 0.f, 1.f);

  float f0 = ( 1.f - iEta ) / ( 1.f + iEta );
  f0 = f0 * f0;

  int base0 = seg * 6;
  int base1 = ( seg + 1 ) * 6;
  for ( int k = 0; k < 6; ++k )
  {
    oC0[k] = mix(c0Table[base0 + k], c0Table[base1 + k], t);
    oC1[k] = mix(c1Table[base0 + k], c1Table[base1 + k], t);
    float cd15 = mix(cdTable[base0 + k], cdTable[base1 + k], t);
    if ( f0 <= 0.04f )
      oCd[k] = cd15 * ( f0 / 0.04f );
    else
      oCd[k] = mix(cd15, oC0[k], ( f0 - 0.04f ) / 0.96f);
  }
}

// ----------------------------------------------------------------------------
// EvalAlbedoPoly
// ----------------------------------------------------------------------------
float EvalAlbedoPoly( in float iC[6], in float iCosTheta )
{
  float u = sqrt(clamp(iCosTheta, 0.f, 1.f));
  float u2 = u * u;
  float u3 = u2 * u;
  float u4 = u2 * u2;
  float u5 = u4 * u;
  return clamp(iC[0] + iC[1] * u + iC[2] * u2 + iC[3] * u3 + iC[4] * u4 + iC[5] * u5, 0.f, 1.f);
}

// ----------------------------------------------------------------------------
// EvalAlbedoPolyAvg
// Exact hemispherical average 2 * int_0^1 E(mu) * mu d(mu)
// ----------------------------------------------------------------------------
float EvalAlbedoPolyAvg( in float iC[6] )
{
  float avg = iC[0]
    + ( 4.f / 5.f ) * iC[1]
    + ( 4.f / 6.f ) * iC[2]
    + ( 4.f / 7.f ) * iC[3]
    + ( 4.f / 8.f ) * iC[4]
    + ( 4.f / 9.f ) * iC[5];
  return clamp(avg, 0.f, 0.999f);
}

// ----------------------------------------------------------------------------
// EvalDiffuse
// ----------------------------------------------------------------------------
vec3 EvalDiffuse( in Material iMat, in float iEta, in vec3 iSpecCol, in vec3 iCsheen, in vec3 iV, in vec3 iL, in vec3 iH, out float oPdf )
{
  oPdf = 0.f;
  if ( iL.z <= 0.f )
    return vec3(0.f);

  float c0[6], c1[6], cd[6];
  GGXDirectionalAlbedoCoeffs(iMat._Roughness, iEta, c0, c1, cd);

  float E0_V = EvalAlbedoPoly(c0, iV.z);
  float E0_L = EvalAlbedoPoly(c0, iL.z);
  float E0_avg = EvalAlbedoPolyAvg(c0);

  float Ed_V = EvalAlbedoPoly(cd, iV.z);
  float Ed_L = EvalAlbedoPoly(cd, iL.z);
  float Ed_avg = EvalAlbedoPolyAvg(cd);

  // Diffuse
  float FL = SchlickFresnel(iL.z);
  float FV = SchlickFresnel(iV.z);
  float FH = SchlickFresnel(dot(iL, iH));
  float Fd = ( 1.f - Ed_V ) * ( 1.f - Ed_L ) / max(1.f - Ed_avg, 1e-4f);

  // Fake Subsurface
  float Fss90 = dot(iL, iH) * dot(iL, iH) * iMat._Roughness;
  float Fss = mix(1.f, Fss90, FL) * mix(1., Fss90, FV);
  float ss = 1.25f * (Fss * (1.f / (iL.z + iV.z) - .5f) + .5f);

  // Sheen
  vec3 Fsheen = FH * iMat._Sheen * iCsheen;

  vec3 dielectricDiff = ( 1.f - iMat._Metallic ) * ( 1.f - iMat._SpecTrans ) * ( INV_PI * mix(Fd, ss, iMat._Subsurface) * iMat._Albedo + Fsheen );

  // Kulla-Conty metallic multiple-scattering compensation lobe
  vec3 F_avg = iMat._Albedo + ( vec3(1.f) - iMat._Albedo ) * ( 1.f / 21.f );
  vec3 F_ms = iMat._Albedo * F_avg * E0_avg / max(vec3(1.f) - F_avg * ( 1.f - E0_avg ), vec3(1e-4f));
  // When iMat._Albedo == 1, F_avg == 1 and F_ms == 1
  F_ms = mix(F_ms, vec3(1.f), step(vec3(0.999f), iMat._Albedo));
  float msFactor = ( 1.f - E0_V ) * ( 1.f - E0_L ) / max(1.f - E0_avg, 1e-4f);
  vec3 metallicMS = iMat._Metallic * F_ms * ( INV_PI * msFactor );

  oPdf = iL.z * INV_PI;
  return dielectricDiff + metallicMS;
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
vec3 SampleGTR1( in float iRoughness, in float iR1, in float iR2 )
{
  float a = max(RESOLUTION, iRoughness);
  float a2 = a * a;

  float phi = iR2 * TWO_PI;

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

  float VdotH = max(dot(iV, iH), 0.f);
  float LdotH = max(dot(iL, iH), 0.f);
  vec3 dielectricF = DielectricFresnel(VdotH, iEta) * iSpecCol;
  vec3 metallicF = mix(iMat._Albedo, vec3(1.f), SchlickFresnel(LdotH));
  vec3 F = mix(dielectricF, metallicF, iMat._Metallic);

  float D = GTR2Aniso(iH.z, iH.x, iH.y, iMat._Ax, iMat._Ay);
  float G1 = SmithGAniso(abs(iV.z), iV.x, iV.y, iMat._Ax, iMat._Ay);
  float G2 = G1 * SmithGAniso(abs(iL.z), iL.x, iL.y, iMat._Ax, iMat._Ay);

  float denom = 4.f * max(iL.z, 1e-4f) * max(iV.z, 1e-4f);
  oPdf = max(G1 * D / (4.f * max(iV.z, 1e-4f)), 0.f);
  return F * D * G2 / denom;
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
  denom = max(denom * denom, 1e-5f);
  float eta2 = iEta * iEta;
  float jacobian = abs(dot(iL, iH)) / denom;

  float Vz = max(abs(iV.z), 1e-4f);
  float Lz = max(abs(iL.z), 1e-4f);
  oPdf = max(G1 * max(0.f, dot(iV, iH)) * D * jacobian / Vz, 0.f);

  return pow(iMat._Albedo, vec3(.5f)) * (1.f - iMat._Metallic) * iMat._SpecTrans * (1.f - F) * D * G2 * abs(dot(iV, iH)) * jacobian * eta2 / (Lz * Vz);
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
  float jacobian = 1.f / (4.f * max(dot(iV, iH), 1e-4f));

  oPdf = max(D * iH.z * jacobian, 0.f);
  return vec3(.25f) * iMat._Clearcoat * F * D * G / (4.f * max(iL.z, 1e-4f) * max(iV.z, 1e-4f));
}

// ----------------------------------------------------------------------------
// GetSpecColor
// ----------------------------------------------------------------------------
void GetSpecColor( in Material iMat, in float iEta, out vec3 oSpecCol, out vec3 oSheenCol )
{
  float lum = Luminance(iMat._Albedo);
  vec3 ctint = ( lum > 0.f ) ? ( iMat._Albedo / lum ) : ( vec3(1.f) );
  oSpecCol = mix(vec3(1.f), ctint, iMat._SpecTint);
  oSheenCol = mix(vec3(1.f), ctint, iMat._SheenTint);
}

// ----------------------------------------------------------------------------
// GetLobeProbabilities
// ----------------------------------------------------------------------------
void GetLobeProbabilities( in Material iMat, in float iEta, in vec3 iSpecCol, in float iCosTheta, out float oDiffuseWeight, out float oSpecReflectWeight, out float oSpecRefractWeight, out float oClearcoatWeight )
{
  float c0[6], c1[6], cd[6];
  GGXDirectionalAlbedoCoeffs(iMat._Roughness, iEta, c0, c1, cd);
  float E0_V = EvalAlbedoPoly(c0, iCosTheta);
  float Ed_V = EvalAlbedoPoly(cd, iCosTheta);

  float dielectricF = DielectricFresnel(max(iCosTheta, 0.f), iEta);
  float metallicF = SchlickFresnel(max(iCosTheta, 0.f));
  float F = mix(dielectricF, metallicF, iMat._Metallic);

  float dielDiffWeight = (1.f - Ed_V) * Luminance(iMat._Albedo) * (1.f - iMat._Metallic) * (1.f - iMat._SpecTrans);
  float metalMSWeight  = (1.f - E0_V) * Luminance(iMat._Albedo) * iMat._Metallic;

  oDiffuseWeight     = dielDiffWeight + metalMSWeight;
  oSpecReflectWeight = mix(Ed_V, E0_V * Luminance(mix(iMat._Albedo, vec3(1.f), metallicF)), iMat._Metallic);
  oSpecRefractWeight = (1.f - F) * (1.f - iMat._Metallic) * iMat._SpecTrans * Luminance(iMat._Albedo);
  oClearcoatWeight   = .25f * iMat._Clearcoat * (1.f - iMat._Metallic);
  float totalWeight = oDiffuseWeight + oSpecReflectWeight + oSpecRefractWeight + oClearcoatWeight;

  if ( totalWeight > 0.f )
  {
    oDiffuseWeight     /= totalWeight;
    oSpecReflectWeight /= totalWeight;
    oSpecRefractWeight /= totalWeight;
    oClearcoatWeight   /= totalWeight;
  }
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
  GetLobeProbabilities(iMat, iEta, specCol, max(iV.z, 0.f), diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight);

  float pdf = 0.f;

  // Diffuse
  if ( ( diffuseWeight > 0.f ) && ( iL.z > 0.f ) )
  {
    f += EvalDiffuse(iMat, iEta, specCol, sheenCol, iV, iL, H, pdf);
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

// ----------------------------------------------------------------------------
// DisneySample
// ----------------------------------------------------------------------------
vec3 DisneySample( in HitPoint iHP, in Material iMat, in float iEta, in vec3 iV, out vec3 oL, out float oPdf )
{
  oPdf = 0.f;
  oL = vec3(0.f);

  float r1 = rand();
  float r2 = rand();

  vec3 T, BT;
  ComputeOnB(iHP._Normal, T, BT);
  vec3 localV = ToLocal(T, BT, iHP._Normal, iV);

  // Specular and sheen color
  vec3 specCol, sheenCol;
  GetSpecColor(iMat, iEta, specCol, sheenCol);

  // Lobe weights
  float diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight;
  GetLobeProbabilities(iMat, iEta, specCol, max(localV.z, 0.f), diffuseWeight, specReflectWeight, specRefractWeight, clearcoatWeight);

  float totalWeight = diffuseWeight + clearcoatWeight + specReflectWeight + specRefractWeight;
  if ( totalWeight <= 0.f )
    return vec3(0.f);

  // CDF for picking a lobe
  float cdf[4];
  cdf[0] = diffuseWeight;
  cdf[1] = cdf[0] + clearcoatWeight;
  cdf[2] = cdf[1] + specReflectWeight;
  cdf[3] = cdf[2] + specRefractWeight;

  vec3 localL = vec3(0.f);

  if ( r1 < cdf[0] ) // Diffuse Reflection Lobe
  {
    r1 /= max(cdf[0], EPSILON);
    localL = CosineSampleHemisphere(r1, r2);
  }
  else if ( r1 < cdf[1] ) // Clearcoat Lobe
  {
    r1 = ( r1 - cdf[0] ) / max(cdf[1] - cdf[0], EPSILON);
    vec3 H = SampleGTR1(iMat._ClearcoatRoughness, r1, r2);
    if ( H.z < 0.f )
      H = -H;
    localL = normalize(reflect(-localV, H));
  }
  else if ( r1 < cdf[2] ) // Specular Reflection Lobe
  {
    r1 = ( r1 - cdf[1] ) / max(cdf[2] - cdf[1], EPSILON);
    vec3 H = SampleGGXVNDF(localV, iMat._Ax, iMat._Ay, r1, r2);
    if ( H.z < 0.f )
      H = -H;
    localL = normalize(reflect(-localV, H));
  }
  else // Specular Refraction Lobe
  {
    r1 = ( r1 - cdf[2] ) / max(cdf[3] - cdf[2], EPSILON);
    vec3 H = SampleGGXVNDF(localV, iMat._Ax, iMat._Ay, r1, r2);
    if ( H.z < 0.f )
      H = -H;
    vec3 refracted = refract(-localV, H, iEta);
    if ( dot(refracted, refracted) > 0.f )
      localL = normalize(refracted);
    else
      localL = normalize(reflect(-localV, H));
  }

  oL = ToWorld(T, BT, iHP._Normal, localL);
  return DisneyEval(iHP, iMat, iEta, iV, oL, oPdf);
}

#endif
