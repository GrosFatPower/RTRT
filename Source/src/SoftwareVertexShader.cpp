#include "SoftwareVertexShader.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
void DefaultVertexShader::Process(const RasterData::Vertex & iVertex, RasterData::ProjectedVertex & oProjectedVertex)
{
  oProjectedVertex._ProjPos          = _MVP * Vec4(iVertex._WorldPos, 1.f); // in clip space
  oProjectedVertex._Attrib._WorldPos = iVertex._WorldPos;
  oProjectedVertex._Attrib._UV       = iVertex._UV;
  oProjectedVertex._Attrib._Normal   = glm::normalize(Vec3(_InvM * Vec4(iVertex._Normal, 0.f)));
}


#ifdef SIMD_AVX2
// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
void DefaultVertexShaderAVX2::Process(const RasterData::Vertex& iVertex, RasterData::ProjectedVertex& oProjectedVertex)
{
  oProjectedVertex._ProjPos          = SIMDUtils::ApplyTransformAVX2(_SIMDMVP, Vec4(iVertex._WorldPos, 1.f)); // in clip space
  oProjectedVertex._Attrib._WorldPos = iVertex._WorldPos;
  oProjectedVertex._Attrib._UV       = iVertex._UV;
  oProjectedVertex._Attrib._Normal   = glm::normalize(Vec3(_InvM * Vec4(iVertex._Normal, 0.f)));
}
#endif

#ifdef SIMD_ARM_NEON
// ----------------------------------------------------------------------------
// Process
// ----------------------------------------------------------------------------
void DefaultVertexShaderARM::Process(const RasterData::Vertex& iVertex, RasterData::ProjectedVertex& oProjectedVertex)
{
  oProjectedVertex._ProjPos          = SIMDUtils::ApplyTransformARM(_SIMDMVP, Vec4(iVertex._WorldPos, 1.f)); // in clip space
  oProjectedVertex._Attrib._WorldPos = iVertex._WorldPos;
  oProjectedVertex._Attrib._UV       = iVertex._UV;
  oProjectedVertex._Attrib._Normal   = glm::normalize(Vec3(_InvM * Vec4(iVertex._Normal, 0.f)));
}
#endif

}
