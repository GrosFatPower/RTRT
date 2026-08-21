#pragma warning(disable : 4100) // unreferenced formal parameter

#include "SoftwareFragmentShader.h"
#include "EnvMap.h"

namespace rd = RTRT::RasterData;

namespace RTRT
{

float DistributionGGX( float iAlpha, float iNdotH );
float GeometrySmith( float iAlpha, float iNdotV, float iNdotL );
Vec3 FresnelSchlick( const Vec3 & iF0, float iVdotH );

namespace
{

Vec3 SampleEnvironment( const rd::DefaultUniform & iUniforms, const Vec3 & iDirection )
{
  if ( !iUniforms._EnableEnvMap || !iUniforms._EnvMap || !iUniforms._EnvMap -> IsInitialized() )
    return Vec3(0.f);

  const Vec3 direction = glm::normalize(iDirection);
  const float theta = std::asin(MathUtil::Clamp(direction.y, -1.f, 1.f));
  const float phi = std::atan2(direction.z, direction.x);
  const Vec2 uv = Vec2(.5f + phi * M_1_PI * .5f, .5f - theta * M_1_PI)
    + Vec2(iUniforms._EnvMapRotation, 0.f);
  if ( iUniforms._Sampling >= SamplingMode::Bilinear )
    return Vec3(iUniforms._EnvMap -> BiLinearSample(uv));
  return Vec3(iUniforms._EnvMap -> Sample(uv));
}

float TransmissionF0( float iIOR )
{
  const float ior = std::max(iIOR, 1.001f);
  const float ratio = ( ior - 1.f ) / ( ior + 1.f );
  return ratio * ratio;
}

void EvaluatePBRComponents( const Vec3 & iN, const Vec3 & iV, const Vec3 & iL,
                            const Material & iMat, const Vec3 & iF0,
                            Vec3 & oDiffuse, Vec3 & oSpecular )
{
  const Vec3 H = glm::normalize(iV + iL);
  const float NdotV = glm::max(glm::dot(iN, iV), 0.f);
  const float NdotL = glm::max(glm::dot(iN, iL), 0.f);
  const float VdotH = glm::max(glm::dot(iV, H), 0.f);
  const float NdotH = glm::max(glm::dot(iN, H), 0.f);
  float alpha = glm::max(iMat._Roughness, RESOLUTION);
  alpha *= alpha;
  const Vec3 F = FresnelSchlick(iF0, VdotH);
  const float D = DistributionGGX(alpha, NdotH);
  const float G = GeometrySmith(alpha, NdotV, NdotL);
  const Vec3 Kd = ( Vec3(1.f) - F ) * ( 1.f - iMat._Metallic );
  oDiffuse = Kd * iMat._Albedo * INV_PI;
  oSpecular = D * G * F / glm::max(4.f * NdotV * NdotL, EPSILON);
}

}

// ----------------------------------------------------------------------------
// ResolveMaterialOpacity
// ----------------------------------------------------------------------------
float ResolveMaterialOpacity( const Material & iMaterial,
                              const std::vector<Texture*> & iTextures,
                              SamplingMode iSampling,
                              const Vec2 & iUV,
                              float iLOD )
{
  float opacity = MathUtil::Clamp(iMaterial._Opacity, 0.f, 1.f);
  const int textureID = static_cast<int>(iMaterial._BaseColorTexId);
  if ( ( textureID < 0 ) || ( textureID >= static_cast<int>(iTextures.size()) ) )
    return opacity;

  const Texture * texture = iTextures[textureID];
  if ( !texture )
    return opacity;

  Vec4 texel;
  if ( iSampling >= SamplingMode::Bilinear )
    texel = texture -> BiLinearSample(iUV, iLOD, iSampling == SamplingMode::Trilinear);
  else
    texel = texture -> Sample(iUV);
  return opacity * MathUtil::Clamp(texel.a, 0.f, 1.f);
}

// ----------------------------------------------------------------------------
// ProcessTransparent
// ----------------------------------------------------------------------------
TransparentShadingResult SoftwareFragmentShader::ProcessTransparent(const rd::Fragment& iFrag,
                                                                    const rd::RasterTriangle & iTri,
                                                                    MaterialPass iMaterialPass)
{
  const Vec4 straightColor = Process(iFrag, iTri);
  TransparentShadingResult result;
  result._Alpha = MathUtil::Clamp(straightColor.a, 0.f, 1.f);
  result._PremultipliedColor = Vec3(straightColor) * result._Alpha;
  return result;
}

// ----------------------------------------------------------------------------
// UTILS
// ----------------------------------------------------------------------------

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
void SetupMaterial(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle & iTri, const RasterData::DefaultUniform & iUniforms, int iMatID, Material & oMat, Vec3 & oF0, Vec3 & oNormal )
{
  oMat = (*iUniforms._Materials)[iMatID];

  // Albedo
  if (oMat._BaseColorTexId >= 0)
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._BaseColorTexId)];
    if ( tex )
    {
      Vec4 texel;
      if (iUniforms._Sampling >= SamplingMode::Bilinear)
        texel = tex->BiLinearSample(iFrag._Attrib._UV, iFrag._Attrib._LOD, (iUniforms._Sampling == SamplingMode::Trilinear));
      else
        texel = tex->Sample(iFrag._Attrib._UV);
      oMat._Albedo = Vec3(texel);
      oMat._Opacity *= texel.a;
    }
  }

  // Normal
  if ( oMat._NormalMapTexID >= 0 )
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._NormalMapTexID)];
    if ( tex )
    {
      Vec3 texNormal;
      //if (iUniforms._Sampling >= SamplingMode::Bilinear)
      //  texNormal = Vec3(tex->BiLinearSample(iFrag._Attrib._UV));
      //else
        texNormal = Vec3(tex->Sample(iFrag._Attrib._UV));

      texNormal = glm::normalize(texNormal * 2.f - 1.f);

      oNormal = glm::normalize(iTri._Tangent * texNormal.x + iTri._Bitangent * texNormal.y + oNormal * texNormal.z);
    }
  }

  // Metallic/Roughness
  if (oMat._MetallicRoughnessTexID >= 0)
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._MetallicRoughnessTexID)];
    if ( tex )
    {
      Vec3 metallicRoughness;
      //if (iUniforms._Sampling >= SamplingMode::Bilinear)
      //  metallicRoughness = Vec3(tex->BiLinearSample(iFrag._Attrib._UV));
      //else
        metallicRoughness = Vec3(tex->Sample(iFrag._Attrib._UV));

      oMat._Metallic = MathUtil::Clamp(oMat._Metallic * metallicRoughness.b, 0.f, 1.f);
      oMat._Roughness = MathUtil::Clamp(oMat._Roughness * metallicRoughness.g, 0.f, 1.f);
    }
  }
  
  // Emission
  if ( oMat._EmissionMapTexID >= 0 )
  {
    const Texture* tex = (*iUniforms._Textures)[static_cast<int>(oMat._EmissionMapTexID)];
    if ( tex )
    {
      //if (iUniforms._Sampling >= SamplingMode::Bilinear)
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
  Vec4 albedo(1.f);
  float opacity = 1.f;
  if (iTri._MatID >= 0)
  {
    const Material& mat = (*_Uniforms._Materials)[iTri._MatID];
    opacity = ResolveMaterialOpacity(mat, *_Uniforms._Textures, _Uniforms._Sampling, iFrag._Attrib._UV, iFrag._Attrib._LOD);
    if (mat._BaseColorTexId >= 0)
    {
      const Texture* tex = (*_Uniforms._Textures)[static_cast<int>(mat._BaseColorTexId)];
      if (_Uniforms._Sampling >= SamplingMode::Bilinear)
        albedo = tex->BiLinearSample(iFrag._Attrib._UV, iFrag._Attrib._LOD, (_Uniforms._Sampling == SamplingMode::Trilinear));
      else
        albedo = tex->Sample(iFrag._Attrib._UV);
    }
    else
      albedo = Vec4(mat._Albedo, opacity);
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

  Vec4 color = MathUtil::Min(albedo * alpha, Vec4(1.f));
  color.a = ( iTri._MatID >= 0 ) && ( AlphaMode::Blend == MaterialAlphaMode((*_Uniforms._Materials)[iTri._MatID]) ) ? opacity : 1.f;
  return color;
}

// ----------------------------------------------------------------------------
// ProcessTransparent
// ----------------------------------------------------------------------------
TransparentShadingResult BlinnPhongFragmentShader::ProcessTransparent(const rd::Fragment& iFrag,
                                                                      const rd::RasterTriangle & iTri,
                                                                      MaterialPass iMaterialPass)
{
  if ( MaterialPass::Transmission != iMaterialPass )
    return SoftwareFragmentShader::ProcessTransparent(iFrag, iTri, iMaterialPass);

  TransparentShadingResult result;
  if ( iTri._MatID < 0 )
    return result;

  Material mat;
  Vec3 F0;
  Vec3 N = glm::normalize(iFrag._Attrib._Normal);
  SetupMaterial(iFrag, iTri, _Uniforms, iTri._MatID, mat, F0, N);

  const float specTrans = MathUtil::Clamp(mat._SpecTrans, 0.f, 1.f);
  result._Alpha = MathUtil::Clamp(mat._Opacity, 0.f, 1.f) * ( 1.f - specTrans );
  const Vec3 V = glm::normalize(_Uniforms._CameraPos - iFrag._Attrib._WorldPos);
  Vec3 diffuse(0.f);
  Vec3 specular(0.f);
  for ( const Light & light : _Uniforms._Lights )
  {
    const Vec3 L = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);
    const float NdotL = std::max(glm::dot(N, L), 0.f);
    if ( NdotL <= 0.f )
      continue;
    const Vec3 radiance = light._Emission;
    diffuse += mat._Albedo * radiance * NdotL * ( 1.f - specTrans );
    const Vec3 R = glm::reflect(-L, N);
    const float specularFactor = std::pow(std::max(glm::dot(V, R), 0.f), 32.f);
    specular += Vec3(TransmissionF0(mat._IOR)) * radiance * specularFactor * .5f;
  }
  const Vec3 environment = SampleEnvironment(_Uniforms, glm::reflect(-V, N));
  const float fresnel = TransmissionF0(mat._IOR)
    + ( 1.f - TransmissionF0(mat._IOR) ) * std::pow(1.f - std::max(glm::dot(N, V), 0.f), 5.f);
  const float roughnessFade = 1.f - glm::smoothstep(_Uniforms._SpecularIBLMaxRoughness * .75f,
                                                    _Uniforms._SpecularIBLMaxRoughness,
                                                    MathUtil::Clamp(mat._Roughness, 0.f, 1.f));
  result._PremultipliedColor = ( mat._Emission + diffuse ) * result._Alpha + specular
    + environment * fresnel * roughnessFade * _Uniforms._SpecularIBLIntensity;
  return result;
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
  Vec3 normal = glm::normalize(iFrag._Attrib._Normal);
  SetupMaterial(iFrag, iTri, _Uniforms, iTri._MatID, mat, F0, normal);

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
    
    float irradiance = std::max(glm::dot(L, normal), 0.f) * invDistToLight * invDistToLight + ambientStrength;
    if ( irradiance > 0.f )
      outColor += BRDF(normal, V, L, mat, F0) * light._Emission * irradiance;
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

  const float opacity = ( AlphaMode::Blend == MaterialAlphaMode(mat) ) ? MathUtil::Clamp(mat._Opacity, 0.f, 1.f) : 1.f;
  return Vec4(outColor, opacity);
}

// ----------------------------------------------------------------------------
// ProcessTransparent
// ----------------------------------------------------------------------------
TransparentShadingResult PBRFragmentShader::ProcessTransparent(const rd::Fragment& iFrag,
                                                               const rd::RasterTriangle & iTri,
                                                               MaterialPass iMaterialPass)
{
  if ( MaterialPass::Transmission != iMaterialPass )
    return SoftwareFragmentShader::ProcessTransparent(iFrag, iTri, iMaterialPass);

  TransparentShadingResult result;
  if ( iTri._MatID < 0 )
    return result;

  Material mat;
  Vec3 unusedF0;
  Vec3 N = glm::normalize(iFrag._Attrib._Normal);
  SetupMaterial(iFrag, iTri, _Uniforms, iTri._MatID, mat, unusedF0, N);
  const float specTrans = MathUtil::Clamp(mat._SpecTrans, 0.f, 1.f);
  result._Alpha = MathUtil::Clamp(mat._Opacity, 0.f, 1.f) * ( 1.f - specTrans );
  const Vec3 dielectricF0(TransmissionF0(mat._IOR));
  const Vec3 F0 = glm::mix(dielectricF0, mat._Albedo, MathUtil::Clamp(mat._Metallic, 0.f, 1.f));
  const Vec3 V = glm::normalize(_Uniforms._CameraPos - iFrag._Attrib._WorldPos);
  Vec3 directDiffuse(0.f);
  Vec3 directSpecular(0.f);
  for ( const Light & light : _Uniforms._Lights )
  {
    const Vec3 L = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);
    const float NdotL = std::max(glm::dot(N, L), 0.f);
    if ( NdotL <= 0.f )
      continue;
    Vec3 diffuse;
    Vec3 specular;
    EvaluatePBRComponents(N, V, L, mat, F0, diffuse, specular);
    directDiffuse += diffuse * light._Emission * NdotL * ( 1.f - specTrans );
    directSpecular += specular * light._Emission * NdotL;
  }

  const float NdotV = std::max(glm::dot(N, V), 0.f);
  const Vec3 fresnel = F0 + ( glm::max(Vec3(1.f - mat._Roughness), F0) - F0 )
    * std::pow(1.f - NdotV, 5.f);
  const float roughnessFade = 1.f - glm::smoothstep(_Uniforms._SpecularIBLMaxRoughness * .75f,
                                                    _Uniforms._SpecularIBLMaxRoughness,
                                                    MathUtil::Clamp(mat._Roughness, 0.f, 1.f));
  const Vec3 environment = SampleEnvironment(_Uniforms, glm::reflect(-V, N));
  result._PremultipliedColor = ( mat._Emission + directDiffuse ) * result._Alpha + directSpecular
    + environment * fresnel * roughnessFade * _Uniforms._SpecularIBLIntensity;
  return result;
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
  Material mat;
  Vec3 F0;
  Vec3 normal = glm::normalize(iFrag._Attrib._Normal);
  if ( iTri._MatID >= 0 )
    SetupMaterial(iFrag, iTri, _Uniforms, iTri._MatID, mat, F0, normal);
  normal = glm::abs(normal);
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
