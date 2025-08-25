#include "SoftwareFragmentShader.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 BlinnPhongFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle& iRasterTri)
{
  Vec4 albedo;
  if (iFrag._MatID >= 0)
  {
    const Material& mat = (*_Uniforms._Materials)[iFrag._MatID];
    if (mat._BaseColorTexId >= 0)
    {
      const Texture* tex = (*_Uniforms._Textures)[static_cast<int>(mat._BaseColorTexId)];
      if (_Uniforms._BilinearSampling)
        albedo = tex->BiLinearSample(iFrag._Attrib._UV);
      else
        albedo = tex->Sample(iFrag._Attrib._UV);
    }
    else
      albedo = Vec4(mat._Albedo, 1.f);
  }

  // Shading
  Vec4 alpha(0.f, 0.f, 0.f, 0.f);
  for (const auto& light : _Uniforms._Lights)
  {
    float ambientStrength = .1f;
    float diffuse = 0.f;
    float specular = 0.f;

    Vec3 dirToLight = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);
    diffuse = std::max(0.f, glm::dot(iFrag._Attrib._Normal, dirToLight));

    Vec3 viewDir = glm::normalize(_Uniforms._CameraPos - iFrag._Attrib._WorldPos);
    Vec3 reflectDir = glm::reflect(-dirToLight, iFrag._Attrib._Normal);

    static float specularStrength = 0.5f;
    specular = static_cast<float>(pow(std::max(glm::dot(viewDir, reflectDir), 0.f), 32)) * specularStrength;

    alpha += std::min(diffuse + ambientStrength + specular, 1.f) * Vec4(glm::normalize(light._Emission), 1.f);
  }

  return MathUtil::Min(albedo * alpha, Vec4(1.f));
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 DepthFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle& iRasterTri)
{
  return Vec4(Vec3(iFrag._FragCoords.z + 1.f) * .5f, 1.f);
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 NormalFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle& iRasterTri)
{
  return Vec4(glm::abs(iFrag._Attrib._Normal), 1.f);
}

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
Vec4 WireFrameFragmentShader::Process(const RasterData::Fragment& iFrag, const RasterData::RasterTriangle& iRasterTri)
{
  Vec2 P(iFrag._FragCoords);
  if ( (MathUtil::DistanceToSegment(iRasterTri._V[0], iRasterTri._V[1], P) <= 1.f)
    || (MathUtil::DistanceToSegment(iRasterTri._V[1], iRasterTri._V[2], P) <= 1.f)
    || (MathUtil::DistanceToSegment(iRasterTri._V[2], iRasterTri._V[0], P) <= 1.f))
  {
    return Vec4(1.f, 0.f, 0.f, 1.f);
  }
  
  return Vec4(0.f, 0.f, 0.f, 0.f);
}

}
