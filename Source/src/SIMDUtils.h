#ifndef _SIMDUtils_
#define _SIMDUtils_

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

inline bool HasAVX2Support()
{
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
#if defined(_MSC_VER)
  int cpuInfo[4];
  __cpuid(cpuInfo, 1);
  return (cpuInfo[2] & (1 << 5)) != 0;
#else
  unsigned int eax, ebx, ecx, edx;
  __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
return (ecx & (1 << 5)) != 0;
#endif
#else
  return false; // ARM platforms don't support AVX2
#endif
}

inline bool HasAVX512Support()
{
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
#if defined(_MSC_VER)
  int cpuInfo[4];
  __cpuid(cpuInfo, 7);
  return (cpuInfo[1] & (1 << 16)) != 0;
#else
  unsigned int eax, ebx, ecx, edx;
  __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
  return (ebx & (1 << 16)) != 0;
#endif
#else
  return false; // ARM platforms don't support AVX512
#endif
}

#ifdef SIMD_ARM_NEON
inline float GetVectorElement(float32x4_t & iVector, int iIndex)
{
  switch (iIndex)
  {
  case 0: return vgetq_lane_f32(iVector, 0);
  case 1: return vgetq_lane_f32(iVector, 1);
  case 2: return vgetq_lane_f32(iVector, 2);
  case 3: return vgetq_lane_f32(iVector, 3);
  default: throw std::out_of_range("Invalid vector index");
  }
}

inline uint32_t GetVectorElement(uint32x4_t & iVector, int iIndex)
{
  switch (iIndex)
  {
  case 0: return vgetq_lane_u32(iVector, 0);
  case 1: return vgetq_lane_u32(iVector, 1);
  case 2: return vgetq_lane_u32(iVector, 2);
  case 3: return vgetq_lane_u32(iVector, 3);
  default: throw std::out_of_range("Invalid vector index");
  }
}

inline float32x4_t SetVectorElement(float iValue, float32x4_t & iVector, int iIndex)
{
  switch (iIndex)
  {
  case 0: return vsetq_lane_f32(iValue, iVector, 0);
  case 1: return vsetq_lane_f32(iValue, iVector, 1);
  case 2: return vsetq_lane_f32(iValue, iVector, 2);
  case 3: return vsetq_lane_f32(iValue, iVector, 3);
  default: throw std::out_of_range("Invalid vector index");
  }

  return iVector; // Should never reach here
}

inline uint32x4_t SetVectorElement(uint32_t iValue, uint32x4_t& iVector, int iIndex)
{
  switch (iIndex)
  {
  case 0: return vsetq_lane_u32(iValue, iVector, 0);
  case 1: return vsetq_lane_u32(iValue, iVector, 1);
  case 2: return vsetq_lane_u32(iValue, iVector, 2);
  case 3: return vsetq_lane_u32(iValue, iVector, 3);
  default: throw std::out_of_range("Invalid vector index");
  }

  return iVector; // Should never reach here
}
#endif // SIMD_ARM_NEON
}

}

#endif /* _SIMDUtils_ */
