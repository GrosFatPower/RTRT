#ifndef _RasterData_
#define _RasterData_

#include "MathUtil.h"
#include "RGBA8.h"
#include "Light.h"
#include "Material.h"
#include "Texture.h"
#include "SIMDUtils.h"
#include "RenderSettings.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace RTRT
{

namespace RasterData
{
  struct CoverageTriangle
  {
    static const int _SubPixelBits = 8;
    static const int64_t _SubPixelScale = 1ll << _SubPixelBits;
    static const int64_t _SampleOffset = _SubPixelScale / 2;

    int64_t _X[3] = { 0, 0, 0 };
    int64_t _Y[3] = { 0, 0, 0 };
    int64_t _EdgeA[3] = { 0, 0, 0 };
    int64_t _EdgeB[3] = { 0, 0, 0 };
    int64_t _EdgeC[3] = { 0, 0, 0 };
    bool    _TopLeft[3] = { false, false, false };
    int     _PixelMinX = 0;
    int     _PixelMaxX = -1;
    int     _PixelMinY = 0;
    int     _PixelMaxY = -1;
    bool    _Valid = false;

    bool Initialize( const Vec3 iVertices[3] )
    {
      _Valid = false;
      for ( int i = 0; i < 3; ++i )
      {
        _X[i] = static_cast<int64_t>(std::llround(iVertices[i].x * static_cast<float>(_SubPixelScale)));
        _Y[i] = static_cast<int64_t>(std::llround(iVertices[i].y * static_cast<float>(_SubPixelScale)));
      }

      const int64_t area = EdgeValue(0, _X[0], _Y[0]);
      if ( area <= 0 )
        return false;

      int64_t minX = _X[0];
      int64_t maxX = _X[0];
      int64_t minY = _Y[0];
      int64_t maxY = _Y[0];
      for ( int i = 0; i < 3; ++i )
      {
        const int start = ( i + 1 ) % 3;
        const int end = ( i + 2 ) % 3;
        const int64_t dx = _X[end] - _X[start];
        const int64_t dy = _Y[end] - _Y[start];

        _EdgeA[i] = _Y[start] - _Y[end];
        _EdgeB[i] = _X[end] - _X[start];
        _EdgeC[i] = _X[start] * _Y[end] - _X[end] * _Y[start];
        _TopLeft[i] = ( dy < 0 ) || ( ( 0 == dy ) && ( dx > 0 ) );

        minX = std::min(minX, _X[i]);
        maxX = std::max(maxX, _X[i]);
        minY = std::min(minY, _Y[i]);
        maxY = std::max(maxY, _Y[i]);
      }

      _PixelMinX = static_cast<int>(CeilDiv(minX - _SampleOffset, _SubPixelScale));
      _PixelMaxX = static_cast<int>(FloorDiv(maxX - _SampleOffset, _SubPixelScale));
      _PixelMinY = static_cast<int>(CeilDiv(minY - _SampleOffset, _SubPixelScale));
      _PixelMaxY = static_cast<int>(FloorDiv(maxY - _SampleOffset, _SubPixelScale));

      _Valid = ( _PixelMinX <= _PixelMaxX ) && ( _PixelMinY <= _PixelMaxY );
      return _Valid;
    }

    bool CoversPixel( int iX, int iY ) const
    {
      return CoversPoint(static_cast<int64_t>(iX) * _SubPixelScale + _SampleOffset,
                         static_cast<int64_t>(iY) * _SubPixelScale + _SampleOffset);
    }

    bool CoversPoint( int64_t iX, int64_t iY ) const
    {
      for ( int i = 0; i < 3; ++i )
      {
        const int64_t edge = _EdgeA[i] * iX + _EdgeB[i] * iY + _EdgeC[i];
        if ( ( edge < 0 ) || ( ( 0 == edge ) && !_TopLeft[i] ) )
          return false;
      }

      return true;
    }

  private:
    static int64_t FloorDiv( int64_t iValue, int64_t iDivisor )
    {
      if ( iValue >= 0 )
        return iValue / iDivisor;
      return -(( -iValue + iDivisor - 1 ) / iDivisor);
    }

    static int64_t CeilDiv( int64_t iValue, int64_t iDivisor )
    {
      if ( iValue >= 0 )
        return ( iValue + iDivisor - 1 ) / iDivisor;
      return -(( -iValue ) / iDivisor);
    }

    int64_t EdgeValue( int iEdge, int64_t iX, int64_t iY ) const
    {
      const int start = ( iEdge + 1 ) % 3;
      const int end = ( iEdge + 2 ) % 3;
      return ( _X[end] - _X[start] ) * ( iY - _Y[start] ) - ( _Y[end] - _Y[start] ) * ( iX - _X[start] );
    }
  };

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
    float _LOD = 0.f; 

    Varying operator*(float t) const
    {
      auto copy = *this;
      copy._WorldPos *= t;
      copy._Normal *= t;
      copy._UV *= t;
      copy._LOD *= t;
      return copy;
    }

    Varying operator+(const Varying& iRhs) const
    {
      Varying copy = *this;

      copy._WorldPos += iRhs._WorldPos;
      copy._Normal += iRhs._Normal;
      copy._UV += iRhs._UV;
      copy._LOD += iRhs._LOD;

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
      SIMDUtils::InterpolateAVX2(iAttrib1, iAttrib2, iAttrib3, Weights, interpolResult);

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

      InterpolateAVX2(attrib1, attrib2, attrib3, iWeights, oResult);

      oResult._LOD = iAttrib1._LOD * iWeights[0] + iAttrib2._LOD * iWeights[1] + iAttrib3._LOD * iWeights[2];
    }

    static Varying InterpolateAVX2(const RasterData::Varying & iAttrib1, const RasterData::Varying & iAttrib2, const RasterData::Varying & iAttrib3, const float iWeights[3])
    {
      Varying result;
      Varying::InterpolateAVX2(iAttrib1, iAttrib2, iAttrib3, iWeights, result);
      return result;
    }
#endif

#ifdef SIMD_ARM_NEON
    static void InterpolateARM(const float32x4_t iAttrib1[2], const float32x4_t iAttrib2[2], const float32x4_t iAttrib3[2], const float iWeights[3], Varying & oResult)
    {
      float32x4_t Weights[3] = { vdupq_n_f32(iWeights[0]), vdupq_n_f32(iWeights[1]), vdupq_n_f32(iWeights[2]) };

      float32x4_t interpolResult1, interpolResult2;
      SIMDUtils::InterpolateARM(iAttrib1[0], iAttrib2[0], iAttrib3[0], Weights, interpolResult1);
      SIMDUtils::InterpolateARM(iAttrib1[1], iAttrib2[1], iAttrib3[1], Weights, interpolResult2);

      oResult._WorldPos.x = vgetq_lane_f32(interpolResult1, 0);
      oResult._WorldPos.y = vgetq_lane_f32(interpolResult1, 1);
      oResult._WorldPos.z = vgetq_lane_f32(interpolResult1, 2);
      oResult._UV.x       = vgetq_lane_f32(interpolResult1, 3);
      oResult._UV.y       = vgetq_lane_f32(interpolResult2, 0);
      oResult._Normal.x   = vgetq_lane_f32(interpolResult2, 1);
      oResult._Normal.y   = vgetq_lane_f32(interpolResult2, 2);
      oResult._Normal.z   = vgetq_lane_f32(interpolResult2, 3);
    }

    static void InterpolateARM(const RasterData::Varying & iAttrib1, const RasterData::Varying & iAttrib2, const RasterData::Varying & iAttrib3, const float iWeights[3], Varying & oResult)
    {
      float32x4_t attrib1[2] = { { iAttrib1._WorldPos.x, iAttrib1._WorldPos.y, iAttrib1._WorldPos.z, iAttrib1._UV.x } , { iAttrib1._UV.y, iAttrib1._Normal.x, iAttrib1._Normal.y, iAttrib1._Normal.z } };
      float32x4_t attrib2[2] = { { iAttrib2._WorldPos.x, iAttrib2._WorldPos.y, iAttrib2._WorldPos.z, iAttrib2._UV.x } , { iAttrib2._UV.y, iAttrib2._Normal.x, iAttrib2._Normal.y, iAttrib2._Normal.z } };
      float32x4_t attrib3[2] = { { iAttrib3._WorldPos.x, iAttrib3._WorldPos.y, iAttrib3._WorldPos.z, iAttrib3._UV.x } , { iAttrib3._UV.y, iAttrib3._Normal.x, iAttrib3._Normal.y, iAttrib3._Normal.z } };

      Varying::InterpolateARM(attrib1, attrib2, attrib3, iWeights, oResult);

      oResult._LOD = iAttrib1._LOD * iWeights[0] + iAttrib2._LOD * iWeights[1] + iAttrib3._LOD * iWeights[2];
    }

    static RasterData::Varying InterpolateARM(const RasterData::Varying & iAttrib1, const RasterData::Varying & iAttrib2, const RasterData::Varying & iAttrib3, const float iWeights[3])
    {
      Varying result;
      Varying::InterpolateARM(iAttrib1, iAttrib2, iAttrib3, iWeights, result);
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
    SamplingMode                 _Sampling = SamplingMode::Bilinear;
  };

  struct DefaultUniform
  {
    const std::vector<Material>* _Materials = nullptr;
    const std::vector<Texture*>* _Textures = nullptr;
    std::vector<Light>           _Lights;
    Vec3                         _CameraPos = { 0.f, 0.f, 0.f };
    SamplingMode                 _Sampling = SamplingMode::Bilinear;
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
    CoverageTriangle _Coverage;
    Vec3       _Normal;
    Vec3       _Tangent;
    Vec3       _Bitangent;
    int        _MatID;
    float      _LOD = 0.f;
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
