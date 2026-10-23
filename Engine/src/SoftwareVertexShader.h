#ifndef _SoftwareVertexShader_
#define _SoftwareVertexShader_

#include "RasterData.h"
#include "MathUtil.h"
#include "SIMDUtils.h"

namespace RTRT
{

class SoftwareVertexShader
{
public:

  SoftwareVertexShader(const Mat4x4 & iM , const Mat4x4& iV, const Mat4x4& iP) : _M(iM), _V(iV), _P(iP) {}
  virtual ~SoftwareVertexShader() {}

  virtual void Process(const RasterData::Vertex& iVertex, RasterData::ProjectedVertex& oProjectedVertex) = 0;

protected:

  Mat4x4 _M;
  Mat4x4 _V;
  Mat4x4 _P;
};

class DefaultVertexShader : public SoftwareVertexShader
{
public:

  DefaultVertexShader(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP) : SoftwareVertexShader(iM, iV, iP)
  {
    _MVP = _P * _V * _M;
    _InvM = glm::inverse(_M);
  }

  virtual ~DefaultVertexShader(){}

  virtual void Process(const RasterData::Vertex& iVertex, RasterData::ProjectedVertex& oProjectedVertex);

protected:

  Mat4x4 _MVP;
  Mat4x4 _InvM;
};

#ifdef SIMD_AVX2
class DefaultVertexShaderAVX2 : public DefaultVertexShader
{
public:

  DefaultVertexShaderAVX2(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP) : DefaultVertexShader(iM, iV, iP)
  {
    SIMDUtils::LoadMatrixAVX2(_MVP, _SIMDMVP);
  }

  virtual ~DefaultVertexShaderAVX2() {}

  virtual void Process(const RasterData::Vertex& iVertex, RasterData::ProjectedVertex& oProjectedVertex);

protected:

  __m256 _SIMDMVP[4];
};
#endif

#ifdef SIMD_ARM_NEON
class DefaultVertexShaderARM : public DefaultVertexShader
{
public:
  DefaultVertexShaderARM(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP) : DefaultVertexShader(iM, iV, iP)
  {
    SIMDUtils::LoadMatrixARM(_MVP, _SIMDMVP);
  }

  virtual ~DefaultVertexShaderARM() {}

  virtual void Process(const RasterData::Vertex& iVertex, RasterData::ProjectedVertex& oProjectedVertex);

protected:
  float32x4_t _SIMDMVP[4];
};
#endif

}

#endif /* _SoftwareVertexShader_ */
