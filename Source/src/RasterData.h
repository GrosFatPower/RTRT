#ifndef _RasterData_
#define _RasterData_

#include "MathUtil.h"
#include "RGBA8.h"
#include "Light.h"
#include "Material.h"
#include "Texture.h"
#include "SIMDUtils.h"
#include <vector>

namespace RTRT
{

namespace RasterData
{
  struct SIMD_ALIGN64 FrameBuffer
  {
    SIMD_ALIGN64 std::vector<RGBA8> _ColorBuffer;
    SIMD_ALIGN64 std::vector<float> _DepthBuffer;
  };

  struct Varying
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;

    Varying operator*(float t) const
    {
      auto copy = *this;
      copy._WorldPos *= t;
      copy._Normal *= t;
      copy._UV *= t;
      return copy;
    }

    Varying operator+(const Varying& iRhs) const
    {
      Varying copy = *this;

      copy._WorldPos += iRhs._WorldPos;
      copy._Normal += iRhs._Normal;
      copy._UV += iRhs._UV;

      return copy;
    }

    static void Interpolate(const Varying & iAttrib1, const Varying & iAttrib2, const Varying & iAttrib3, const float iWeights[3], Varying & oResult)
    {
      oResult = iAttrib1 * iWeights[0] + iAttrib2 * iWeights[1] + iAttrib3 * iWeights[2];
    }

    static Varying Interpolate(const Varying & iAttrib1, const Varying & iAttrib2, const Varying & iAttrib3, const float iWeights[3])
    {
      Varying result;
      Varying::Interpolate(iAttrib1, iAttrib2, iAttrib3, iWeights, result);
      return result;
    }

#ifdef SIMD_AVX2
    static void InterpolateAVX2(const __m256 & iAttrib1, const __m256 & iAttrib2, const __m256 & iAttrib3, const float iWeights[3], Varying & oResult)
    {
      __m256 Weights[3] = { _mm256_set1_ps(iWeights[0]), _mm256_set1_ps(iWeights[1]), _mm256_set1_ps(iWeights[2]) };

      __m256 interpolResult;
      MathUtil::Interpolate(iAttrib1, iAttrib2, iAttrib3, Weights, interpolResult);

      oResult._WorldPos.x = interpolResult.m256_f32[7];
      oResult._WorldPos.y = interpolResult.m256_f32[6];
      oResult._WorldPos.z = interpolResult.m256_f32[5];
      oResult._UV.x       = interpolResult.m256_f32[4];
      oResult._UV.y       = interpolResult.m256_f32[3];
      oResult._Normal.x   = interpolResult.m256_f32[2];
      oResult._Normal.y   = interpolResult.m256_f32[1];
      oResult._Normal.z   = interpolResult.m256_f32[0];
    }

    static void InterpolateAVX2(const RasterData::Varying & iAttrib1, const RasterData::Varying & iAttrib2, const RasterData::Varying & iAttrib3, const float iWeights[3], Varying & oResult)
    {
      __m256 attrib1 = _mm256_set_ps(iAttrib1._WorldPos.x, iAttrib1._WorldPos.y, iAttrib1._WorldPos.z, iAttrib1._UV.x, iAttrib1._UV.y, iAttrib1._Normal.x, iAttrib1._Normal.y, iAttrib1._Normal.z);
      __m256 attrib2 = _mm256_set_ps(iAttrib2._WorldPos.x, iAttrib2._WorldPos.y, iAttrib2._WorldPos.z, iAttrib2._UV.x, iAttrib2._UV.y, iAttrib2._Normal.x, iAttrib2._Normal.y, iAttrib2._Normal.z);
      __m256 attrib3 = _mm256_set_ps(iAttrib3._WorldPos.x, iAttrib3._WorldPos.y, iAttrib3._WorldPos.z, iAttrib3._UV.x, iAttrib3._UV.y, iAttrib3._Normal.x, iAttrib3._Normal.y, iAttrib3._Normal.z);

      Varying::InterpolateAVX2(attrib1, attrib2, attrib3, iWeights, oResult);
    }

    static RasterData::Varying InterpolateAVX2(const RasterData::Varying & iAttrib1, const RasterData::Varying & iAttrib2, const RasterData::Varying & iAttrib3, const float iWeights[3])
    {
      Varying result;
      Varying::InterpolateAVX2(iAttrib1, iAttrib2, iAttrib3, iWeights, result);
      return result;
    }
#endif
  };

  struct Uniform
  {
    const std::vector<Material>* _Materials = nullptr;
    const std::vector<Texture*>* _Textures = nullptr;
    std::vector<Light>           _Lights;
    Vec3                         _CameraPos = { 0.f, 0.f, 0.f };
    bool                         _BilinearSampling = true;
  };

  struct DefaultUniform
  {
    const std::vector<Material>* _Materials = nullptr;
    const std::vector<Texture*>* _Textures = nullptr;
    std::vector<Light>           _Lights;
    Vec3                         _CameraPos = { 0.f, 0.f, 0.f };
    bool                         _BilinearSampling = true;
  };

  struct Vertex
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;

    bool operator==(const Vertex& iRhs) const
    {
      return ((_WorldPos == iRhs._WorldPos)
        && (_Normal == iRhs._Normal)
        && (_UV == iRhs._UV));
    }
  };

  struct Triangle
  {
    int   _Indices[3];
    Vec3  _Normal;
    int   _MatID;
  };

  struct ProjectedVertex
  {
    Vec4    _ProjPos;
    Varying _Attrib;
  };

  struct SIMD_ALIGN64 RasterTriangle
  {
    int        _Indices[3];
    Vec3       _V[3];
    float      _EdgeA[3];
    float      _EdgeB[3];
    float      _EdgeC[3];
    float      _InvW[3];
    float      _InvArea;
    AABB<Vec2> _BBox;
    Vec3       _Normal;
    int        _MatID;
  };

  struct Fragment
  {
    Vec3    _FragCoords;
    Vec2i   _PixelCoords;
    Vec2i   _RasterTriIdx;
    float   _Weights[3];
    Varying _Attrib;
  };

  struct SIMD_ALIGN64 Tile
  {
    int         _X;
    int         _Y;
    int         _Width;
    int         _Height;
    FrameBuffer _LocalFB;
    SIMD_ALIGN64 std::vector<std::vector<RasterTriangle*>> _RasterTrisBins;
    SIMD_ALIGN64 std::vector<Fragment> _Fragments;
    SIMD_ALIGN64 std::vector<bool> _CoveredPixels;
  };

}

}

#endif /* _RasterData_ */
