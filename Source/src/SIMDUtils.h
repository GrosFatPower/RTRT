#ifndef _SIMDUtils_
#define _SIMDUtils_

#include "MATHUtil.h"

#include <iostream>
#include <iomanip>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#ifndef SIMD_ARM_NEON
#define SIMD_ARM_NEON
#endif
#elif defined(__AVX2__)
#include <immintrin.h>
#ifndef SIMD_AVX2
#define SIMD_AVX2
#endif
#elif defined(__SSE2__)
#include <emmintrin.h>
#define SIMD_SSE2
#else
#define SIMD_SCALAR
#endif

#ifdef _MSC_VER
#include <intrin.h> // For __cpuid
#pragma warning(disable: 4324)
#elif defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpadded"
#endif

#if defined(SIMD_ARM_NEON) || defined(SIMD_AVX2) || defined(SIMD_SSE2)
#define SIMD_ALIGN32 alignas(32)
#define SIMD_ALIGN64 alignas(64)
#else
#define SIMD_ALIGN32
#define SIMD_ALIGN64
#endif

namespace RTRT
{

namespace SIMDUtils
{
  bool HasAVX2Support();
  bool HasAVX512Support();

#ifdef SIMD_ARM_NEON
  float    GetVectorElement(float32x4_t& iVector, int iIndex);
  uint32_t GetVectorElement(uint32x4_t& iVector, int iIndex);
  float32x4_t SetVectorElement(float iValue, float32x4_t& iVector, int iIndex);
  uint32x4_t  SetVectorElement(uint32_t iValue, uint32x4_t& iVector, int iIndex);
#endif

#ifdef SIMD_AVX2
  void LoadMatrixAVX2(const Mat4x4& iMat, __m256 oMat[4]);
  Vec4 ApplyTransformAVX2(const __m256 iTransfo[4], const Vec4& iVec);
#endif
}

}

#include "SIMDUtils.hxx"

#endif /* _SIMDUtils_ */
