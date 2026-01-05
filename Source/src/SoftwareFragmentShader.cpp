#pragma warning(disable : 4100) // unreferenced formal parameter

#include "SoftwareFragmentShader.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// UTILS
// ----------------------------------------------------------------------------

static constexpr float EPSILON    = 1e-9f;
static constexpr float RESOLUTION = 0.001f;
static constexpr float INV_PI     = 1 / static_cast<float>(M_PI);


void ComputeOnB( Vec3 iN, Vec3 & oT, Vec3 & oBT )
{
  Vec3 up = ( abs( iN.z ) < 0.999 ) ? Vec3( 0, 0, 1. ) : Vec3( 1., 0, 0 );

  oT = glm::normalize( glm::cross( up, iN ) );
  oBT = glm::cross( iN, oT );
}

// ----------------------------------------------------------------------------
// DistributionGGX
// GGX/Trowbridge-Reitz : Normal Distribution Function
// Larger the more micro-facets are aligned to H
// NDF = alpha^2 / ( PI * ( (n.h)^2  * ( aplha^2 -1 ) + 1 )^2 )
// ----------------------------------------------------------------------------
float DistributionGGX( float iAlpha, float iNdotH )
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
float G1( float iAlpha, float iNdotX )
{
  float k = iAlpha / 2.f;
  float denom = ( iNdotX * ( 1.f - k ) ) + k;

  return iNdotX / std::max(denom, EPSILON);
}

// ----------------------------------------------------------------------------
// GeometrySmith
// Smith Model
// Smaller the more micro-facets are shadowed by other micro-facets
// ----------------------------------------------------------------------------
float GeometrySmith( float iAlpha, float iNdotV, float iNdotL )
{
  return G1(iAlpha, iNdotV) * G1(iAlpha, iNdotL);
}

// ----------------------------------------------------------------------------
// FresnelSchlick
// Fresnel-Schlick Function
// Proportion of specular reflectance
// FSchlick = F0 + (1-F0)(1-(h.v))^5
// ----------------------------------------------------------------------------
Vec3 FresnelSchlick( const Vec3 & iF0, float iVdotH )
{
  return iF0 + (Vec3(1.f) - iF0) * pow(1.f - iVdotH, 5.f);
}

// ----------------------------------------------------------------------------
// Cook-Torrance Microfacet BRDF
// iN   : surface normal
// iV   : view direction
// iL   : light direction
// iMat : surface material
// ----------------------------------------------------------------------------
Vec3 BRDF( const Vec3 & iN, const Vec3 & iV, const Vec3 & iL, const Material & iMat, const Vec3 & iFO )
{
  Vec3 H = glm::normalize(iV + iL);

  float NdotV = glm::max(dot(iN, iV), 0.f);
  float NdotL = glm::max(dot(iN, iL), 0.f);
  float VdotH = glm::max(dot(iV, H), 0.f);
  float NdotH = glm::max(dot(iN, H), 0.f);

  float alpha = glm::max(iMat._Roughness, RESOLUTION);
  alpha *= alpha;
  Vec3  F = FresnelSchlick(iFO, VdotH);         // Fresnel reflectance (Schlick approximation)
  float D = DistributionGGX(alpha, NdotH);      // Normal distribution function
  float G = GeometrySmith(alpha, NdotV, NdotL); // Geometry term

  // Diffuse
  Vec3 Ks = F;
  Vec3 Kd = Vec3(1.f) - Ks;
  Kd *= ( 1.f - iMat._Metallic ); // pure metals have no diffuse light
  Vec3 lambert = iMat._Albedo * INV_PI;
  //vec3 diffuse = Kd * iMat._Albedo * INV_PI * DiffuseLambertOrenNayar(iN, iV, iL, NdotL, NdotV, alpha);

  // Specular (Cook-Torrance)
  Vec3 cookTorranceNum =  D * G * F;
  float cookTorranceDenom = 4.f * NdotV * NdotL; // 4(V.N)(L.N)
  Vec3 specular = cookTorranceNum / glm::max(cookTorranceDenom, EPSILON);

  return Kd * lambert + specular; // kd * fDiffuse + ks * fSpecular
  //return diffuse + specular;
}

// ----------------------------------------------------------------------------
// SetupMaterial
// ----------------------------------------------------------------------------
void SetupMaterial(const RasterData::Fragment& iFrag, const RasterData::DefaultUniform & iUniforms, int iMatID, Material & oMat, Vec3 & oF0 )
{
  oMat = (*iUniforms._Materials)[iMatID];

  // Albedo
  if (oMat._BaseColorTexId >= 0)
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._BaseColorTexId)];
    if ( tex )
    {
      if (iUniforms._BilinearSampling)
        oMat._Albedo = Vec3(tex->BiLinearSample(iFrag._Attrib._UV, iFrag._Attrib._LOD));
      else
        oMat._Albedo = Vec3(tex->Sample(iFrag._Attrib._UV));
    }
  }

  // Normal
  if ( oMat._NormalMapTexID >= 0 )
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._NormalMapTexID)];
    if ( tex )
    {
      Vec3 texNormal;
      //if (iUniforms._BilinearSampling)
      //  texNormal = Vec3(tex->BiLinearSample(iFrag._Attrib._UV));
      //else
        texNormal = Vec3(tex->Sample(iFrag._Attrib._UV));

      texNormal = glm::normalize(texNormal * 2.f - 1.f);

      Vec3 T, BT;
      ComputeOnB( iFrag._Attrib._Normal, T, BT );
      texNormal = glm::normalize(T * texNormal.x + BT * texNormal.y + iFrag._Attrib._Normal * texNormal.z);
    }
  }

  // Metallic/Roughness
  if (oMat._MetallicRoughnessTexID >= 0)
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._MetallicRoughnessTexID)];
    if ( tex )
    {
      Vec3 metallicRoughness;
      //if (iUniforms._BilinearSampling)
      //  metallicRoughness = Vec3(tex->BiLinearSample(iFrag._Attrib._UV));
      //else
        metallicRoughness = Vec3(tex->Sample(iFrag._Attrib._UV));

      oMat._Metallic = metallicRoughness.x;
      oMat._Roughness = metallicRoughness.y;
    }
  }
  
  // Emission
  if ( oMat._EmissionMapTexID >= 0 )
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._EmissionMapTexID)];
    if ( tex )
    {
      //if (iUniforms._BilinearSampling)
      //  metallicRoughness = Vec3(tex->BiLinearSample(iFrag._Attrib._UV));
      //else
        oMat._Emission = Vec3(tex->Sample(iFrag._Attrib._UV));
    }
  }

  //float aspect = sqrt(1.f - oMat._Anisotropic * .9f);
  //float Ax = std::max(0.001f, oMat._Roughness / aspect);
  //float Ay = std::max(0.001f, oMat._Roughness * aspect);

  // Base reflectance
  oF0 = Vec3(0.16f * pow(oMat._Reflectance, 2.f));
  oF0 = glm::mix(oF0, oMat._Albedo, oMat._Metallic);
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 BlinnPhongFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri)
{
  Vec4 albedo;
  if (iTri._MatID >= 0)
  {
    const Material& mat = (*_Uniforms._Materials)[iTri._MatID];
    if (mat._BaseColorTexId >= 0)
    {
      const Texture* tex = (*_Uniforms._Textures)[static_cast<int>(mat._BaseColorTexId)];
      if (_Uniforms._BilinearSampling)
        albedo = tex->BiLinearSample(iFrag._Attrib._UV, iFrag._Attrib._LOD);
      else
        albedo = tex->Sample(iFrag._Attrib._UV);
    }
    else
      albedo = Vec4(mat._Albedo, 1.f);
  }

  // Shading
  Vec3 normal = glm::normalize(iFrag._Attrib._Normal);

  Vec4 alpha(0.f, 0.f, 0.f, 0.f);
  for (const auto& light : _Uniforms._Lights)
  {
    float ambientStrength = .1f;
    float diffuse = 0.f;
    float specular = 0.f;

    Vec3 dirToLight = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);
    diffuse = std::max(0.f, glm::dot(normal, dirToLight));

    Vec3 viewDir = glm::normalize(_Uniforms._CameraPos - iFrag._Attrib._WorldPos);
    Vec3 reflectDir = glm::reflect(-dirToLight, normal);

    static float specularStrength = 0.5f;
    specular = static_cast<float>(pow(std::max(glm::dot(viewDir, reflectDir), 0.f), 32)) * specularStrength;

    alpha += std::min(diffuse + ambientStrength + specular, 1.f) * Vec4(glm::normalize(light._Emission), 1.f);
  }

  return MathUtil::Min(albedo * alpha, Vec4(1.f));
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 PBRFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri)
{
  if (iTri._MatID < 0)
    return Vec4(1.f);

  Material mat;
  Vec3 F0;
  SetupMaterial(iFrag, _Uniforms, iTri._MatID, mat, F0);

  Vec3 outColor(0.f);

  // Direct lighting
  float ambientStrength = .1f;
  Vec3 V = normalize(_Uniforms._CameraPos - iFrag._Attrib._WorldPos);
  for (const auto& light : _Uniforms._Lights)
  {
    Vec3 L = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);

    float distToLight = length(L);
    float invDistToLight = 1.f / glm::max(distToLight, EPSILON);
    L = L * invDistToLight;
    
    float irradiance = std::max(glm::dot(L, iFrag._Attrib._Normal), 0.f) * invDistToLight * invDistToLight + ambientStrength;
    if ( irradiance > 0.f )
      outColor += BRDF(iFrag._Attrib._Normal, V, L, mat, F0) * light._Emission * irradiance;
  }

  // Image based lighting
  //{
  //  Vec3 L = SampleHemisphere( iFrag._Attrib._Normal );
  //  
  //  float irradiance = glm::max( glm::dot( L, iFrag._Attrib._Normal ), 0.f ) * 0.1f;
  //  if ( irradiance > 0.f )
  //  {
  //    Vec3 envColor;
  //    if ( 0 != u_EnableSkybox )
  //      envColor = SampleSkybox( L, u_SkyboxTexture, u_SkyboxRotation );
  //    else
  //      envColor = u_BackgroundColor;
  //    outColor += BRDF(iFrag._Attrib._Normal, V, L, mat, F0) * envColor * irradiance;
  //  }
  //}

  outColor = glm::clamp(outColor, 0.f, 1.f);

  return Vec4(outColor, 1.f);
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 DepthFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri)
{
  return Vec4(Vec3(iFrag._FragCoords.z + 1.f) * .5f, 1.f);
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 NormalFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri)
{
  Vec3 normal = glm::abs(iFrag._Attrib._Normal);
  return Vec4(normal, 1.f);
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 WireFrameFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri)
{
  Vec2 P(iFrag._FragCoords);
  if ( (MathUtil::DistanceToSegment(iTri._V[0], iTri._V[1], P) <= 1.f)
    || (MathUtil::DistanceToSegment(iTri._V[1], iTri._V[2], P) <= 1.f)
    || (MathUtil::DistanceToSegment(iTri._V[2], iTri._V[0], P) <= 1.f))
  {
    return Vec4(1.f, 0.f, 0.f, 1.f);
  }
  
  return Vec4(0.f, 0.f, 0.f, 0.f);
}

}
