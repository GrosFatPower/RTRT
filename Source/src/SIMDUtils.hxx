#ifndef _SIMDUtils_hxx_
#define _SIMDUtils_hxx_

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
inline float GetVectorElement(float32x4_t& iVector, int iIndex)
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

inline uint32_t GetVectorElement(uint32x4_t& iVector, int iIndex)
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

inline float32x4_t SetVectorElement(float iValue, float32x4_t& iVector, int iIndex)
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

inline void LoadMatrixARM(const Mat4x4& iMat, float32x4_t oMat[4])
{
  SIMD_ALIGN32 float matTransposed[16];
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      matTransposed[i * 4 + j] = iMat[j][i];
    }
  }
  oMat[0] = vld1q_f32(&matTransposed[0]);
  oMat[1] = vld1q_f32(&matTransposed[4]);
  oMat[2] = vld1q_f32(&matTransposed[8]);
  oMat[3] = vld1q_f32(&matTransposed[12]);
}

inline Vec4 ApplyTransformARM(const float32x4_t iTransfo[4], const Vec4& iVec)
{
  const float32x4_t vec = { iVec.x, iVec.y, iVec.z, iVec.w };
  float32x4_t result0 = vmulq_f32(vec, iTransfo[0]);
  float32x4_t result1 = vmulq_f32(vec, iTransfo[1]);
  float32x4_t result2 = vmulq_f32(vec, iTransfo[2]);
  float32x4_t result3 = vmulq_f32(vec, iTransfo[3]);
  
  result0 = vpaddq_f32(result0, result1);
  result2 = vpaddq_f32(result2, result3);
  result0 = vpaddq_f32(result0, result2);

  SIMD_ALIGN32 float results[4];
  vst1q_f32(results, result0);

  return Vec4(results[0], results[1], results[2], results[3]);
}
#endif // SIMD_ARM_NEON


#ifdef SIMD_AVX2
inline void LoadMatrixAVX2(const Mat4x4& iMat, __m256 oMat[4])
{
  SIMD_ALIGN32 float matTransposed[16];
  for (int i = 0; i < 4; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      matTransposed[i * 4 + j] = iMat[j][i];
    }
  }
  oMat[0] = _mm256_load_ps(&matTransposed[0]);
  oMat[1] = _mm256_load_ps(&matTransposed[4]);
  oMat[2] = _mm256_load_ps(&matTransposed[8]);
  oMat[3] = _mm256_load_ps(&matTransposed[12]);
}

inline Vec4 ApplyTransformAVX2(const __m256 iTransfo[4], const Vec4& iVec)
{
  const __m256 vec = _mm256_set_ps(0, 0, 0, 0, iVec.w, iVec.z, iVec.y, iVec.x);

  __m256 result0 = _mm256_mul_ps(vec, iTransfo[0]);
  __m256 result1 = _mm256_mul_ps(vec, iTransfo[1]);
  __m256 result2 = _mm256_mul_ps(vec, iTransfo[2]);
  __m256 result3 = _mm256_mul_ps(vec, iTransfo[3]);

  result0 = _mm256_hadd_ps(result0, result1);
  result2 = _mm256_hadd_ps(result2, result3);
  result0 = _mm256_hadd_ps(result0, result2);

  SIMD_ALIGN32 float results[8];
  _mm256_store_ps(results, result0);

  return Vec4(results[0], results[1], results[2], results[3]);
}
#endif // SIMD_AVX2

}

}

#endif /* _SIMDUtils_hxx_ */
