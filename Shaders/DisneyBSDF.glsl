/*
 * This is a modified version of the original code : https://github.com/knightcrawler25/GLSL-PathTracer
 */

#ifndef _DISNEY_BRDF_GLSL_
#define _DISNEY_BRDF_GLSL_

#include Sampling.glsl
#include Material.glsl
#include ToneMapping.glsl

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
  float denom = dot(iL, H) + dot(iV, iH) * iEta;
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
  vec3 ctint = ( lum > 0.f ) ? ( Mat._Albedo / lum ) : ( vec3(1.f) );
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
  float totalWeight = diffuseWt + specReflectWt + specRefractWt + clearcoatWt;

  oDiffuseWeight     /= totalWeight;
  oSpecReflectWeight /= totalWeight;
  oSpecRefractWeight /= totalWeight;
  oClearcoatWeight   /= totalWeight;
}

// ----------------------------------------------------------------------------
// DisneySample
// ToDO
// ----------------------------------------------------------------------------
vec3 DisneySample(State state, vec3 V, vec3 N, out vec3 L, out float pdf)
{
    pdf = 0.0;
    vec3 f = vec3(0.0);

    float r1 = rand();
    float r2 = rand();

    // TODO: Tangent and bitangent should be calculated from mesh (provided, the mesh has proper uvs)
    vec3 T, B;
    Onb(N, T, B);
    V = ToLocal(T, B, N, V); // NDotL = L.z; NDotV = V.z; NDotH = H.z

    // Specular and sheen color
    vec3 specCol, sheenCol;
    GetSpecColor(state.mat, state.eta, specCol, sheenCol);

    // Lobe weights
    float diffuseWt, specReflectWt, specRefractWt, clearcoatWt;
    // Note: Fresnel is approx and based on N and not H since H isn't available at this stage.
    float approxFresnel = DisneyFresnel(state.mat, state.eta, V.z, V.z);
    GetLobeProbabilities(state.mat, state.eta, specCol, approxFresnel, diffuseWt, specReflectWt, specRefractWt, clearcoatWt);

    // CDF for picking a lobe
    float cdf[4];
    cdf[0] = diffuseWt;
    cdf[1] = cdf[0] + clearcoatWt;
    cdf[2] = cdf[1] + specReflectWt;
    cdf[3] = cdf[2] + specRefractWt;

    if (r1 < cdf[0]) // Diffuse Reflection Lobe
    {
        r1 /= cdf[0];
        L = CosineSampleHemisphere(r1, r2);

        vec3 H = normalize(L + V);

        f = EvalDiffuse(state.mat, sheenCol, V, L, H, pdf);
        pdf *= diffuseWt;
    }
    else if (r1 < cdf[1]) // Clearcoat Lobe
    {
        r1 = (r1 - cdf[0]) / (cdf[1] - cdf[0]);

        vec3 H = SampleGTR1(state.mat.clearcoatRoughness, r1, r2);

        if (H.z < 0.0)
            H = -H;

        L = normalize(reflect(-V, H));

        f = EvalClearcoat(state.mat, V, L, H, pdf);
        pdf *= clearcoatWt;
    }
    else  // Specular Reflection/Refraction Lobes
    {
        r1 = (r1 - cdf[1]) / (1.0 - cdf[1]);
        vec3 H = SampleGGXVNDF(V, state.mat._Ax, state.mat._Ay, r1, r2);

        if (H.z < 0.0)
            H = -H;

        // TODO: Refactor into metallic BRDF and specular BSDF
        float fresnel = DisneyFresnel(state.mat, state.eta, dot(L, H), dot(V, H));
        float F = 1.0 - ((1.0 - fresnel) * state.mat.specTrans * (1.0 - state.mat.metallic));

        if (rand() < F)
        {
            L = normalize(reflect(-V, H));

            f = EvalSpecReflection(state.mat, state.eta, specCol, V, L, H, pdf);
            pdf *= F;
        }
        else
        {
            L = normalize(refract(-V, H, state.eta));

            f = EvalSpecRefraction(state.mat, state.eta, V, L, H, pdf);
            pdf *= 1.0 - F;
        }

        pdf *= specReflectWt + specRefractWt;
    }

    L = ToWorld(T, B, N, L);
    return f * abs(dot(N, L));
}

// ----------------------------------------------------------------------------
// DisneyEval
// ToDo
// ----------------------------------------------------------------------------
vec3 DisneyEval(State state, vec3 V, vec3 N, vec3 L, out float bsdfPdf)
{
    bsdfPdf = 0.0;
    vec3 f = vec3(0.0);

    // TODO: Tangent and bitangent should be calculated from mesh (provided, the mesh has proper uvs)
    vec3 T, B;
    Onb(N, T, B);
    V = ToLocal(T, B, N, V); // NDotL = L.z; NDotV = V.z; NDotH = H.z
    L = ToLocal(T, B, N, L);

    vec3 H;
    if (L.z > 0.0)
        H = normalize(L + V);
    else
        H = normalize(L + V * state.eta);

    if (H.z < 0.0)
        H = -H;

    // Specular and sheen color
    vec3 specCol, sheenCol;
    GetSpecColor(state.mat, state.eta, specCol, sheenCol);

    // Lobe weights
    float diffuseWt, specReflectWt, specRefractWt, clearcoatWt;
    float fresnel = DisneyFresnel(state.mat, state.eta, dot(L, H), dot(V, H));
    GetLobeProbabilities(state.mat, state.eta, specCol, fresnel, diffuseWt, specReflectWt, specRefractWt, clearcoatWt);

    float pdf;

    // Diffuse
    if (diffuseWt > 0.0 && L.z > 0.0)
    {
        f += EvalDiffuse(state.mat, sheenCol, V, L, H, pdf);
        bsdfPdf += pdf * diffuseWt;
    }

    // Specular Reflection
    if (specReflectWt > 0.0 && L.z > 0.0 && V.z > 0.0)
    {
        f += EvalSpecReflection(state.mat, state.eta, specCol, V, L, H, pdf);
        bsdfPdf += pdf * specReflectWt;
    }

    // Specular Refraction
    if (specRefractWt > 0.0 && L.z < 0.0)
    {
        f += EvalSpecRefraction(state.mat, state.eta, V, L, H, pdf);
        bsdfPdf += pdf * specRefractWt;
    }

    // Clearcoat
    if (clearcoatWt > 0.0 && L.z > 0.0 && V.z > 0.0)
    {
        f += EvalClearcoat(state.mat, V, L, H, pdf);
        bsdfPdf += pdf * clearcoatWt;
    }

    return f * abs(L.z);
}

#endif
