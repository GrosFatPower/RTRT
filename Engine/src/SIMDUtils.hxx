#ifndef _SIMDUtils_hxx_
#define _SIMDUtils_hxx_

namespace RTRT
{

namespace SIMDUtils
{
inline bool HasSIMDSupport()
{
#if defined (SIMD_AVX2)
  return true;
#elif defined(SIMD_ARM_NEON)
  return true;
#else
  return false;
#endif
}

//inline bool HasAVX2Support()
//{
//  static short S_HasAVX2Support = -1;
//
//  if ( S_HasAVX2Support < 0 )
//  {
//#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
//#if defined(_MSC_VER)
//    int cpuInfo[4];
//    __cpuid(cpuInfo, 1);
//    S_HasAVX2Support = ( (cpuInfo[2] & (1 << 5)) != 0 ) ? ( 1 ) : ( 0 );
//#else
//    unsigned int eax, ebx, ecx, edx;
//    __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
//    S_HasAVX2Support = ( (ecx & (1 << 5)) != 0 ) ? ( 1 ) : ( 0 );
//#endif
//#else
//    S_HasAVX2Support = 0;
//#endif
//  }
//
//  return ( S_HasAVX2Support > 0 );
//}

//inline bool HasAVX512Support()
//{
//  static short S_HasAVX512Support = -1;
//
//  if ( S_HasAVX512Support < 0 )
//  {
//#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
//#if defined(_MSC_VER)
//  int cpuInfo[4];
//  __cpuid(cpuInfo, 7);
//  S_HasAVX512Support = ( (cpuInfo[1] & (1 << 16)) != 0 ) ? ( 1 ) : ( 0 );
//#else
//    unsigned int eax, ebx, ecx, edx;
//    __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));
//    S_HasAVX512Support = ( (ebx & (1 << 16)) != 0 ) ? ( 1 ) : ( 0 );
//#endif
//#else
//    S_HasAVX512Support = 0;
//#endif
//  }
//
//  return ( S_HasAVX512Support > 0 );
//}

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

#ifdef SIMD_ARM_NEON
inline void InterpolateARM( const float32x4_t & iVal1, const float32x4_t & iVal2, const float32x4_t & iVal3, const float32x4_t iWeights[3], float32x4_t & oResult )
{
  //oResult = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(iWeights[0], iVal1), _mm256_mul_ps(iWeights[1], iVal2)), _mm256_mul_ps(iWeights[2], iVal3));
  oResult = vmlaq_f32(vmlaq_f32(vmulq_f32(iWeights[0], iVal1), iWeights[1], iVal2) ,iWeights[2], iVal3);
}

inline float32x4_t InterpolateARM( const float32x4_t & iVal1, const float32x4_t & iVal2, const float32x4_t & iVal3, const float32x4_t iWeights[3] )
{
  float32x4_t result;
  InterpolateARM(iVal1, iVal2, iVal3, iWeights, result);
  return result;
}
#endif

#ifdef SIMD_AVX2
inline void InterpolateAVX2( const __m256 & iVal1, const __m256 & iVal2, const __m256 & iVal3, const __m256 iWeights[3], __m256 & oResult )
{
  //oResult = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(iWeights[0], iVal1), _mm256_mul_ps(iWeights[1], iVal2)), _mm256_mul_ps(iWeights[2], iVal3));
  oResult = _mm256_fmadd_ps(iWeights[2], iVal3, _mm256_fmadd_ps(iWeights[1], iVal2, _mm256_mul_ps(iWeights[0], iVal1)));
}

inline __m256 InterpolateAVX2( const __m256 & iVal1, const __m256 & iVal2, const __m256 & iVal3, const __m256 iWeights[3] )
{
  __m256 result;
  InterpolateAVX2(iVal1, iVal2, iVal3, iWeights, result);
  return result;
}
#endif

#ifdef SIMD_ARM_NEON
inline uint32x4_t EvalBarycentricCoordinatesARM(const float32x4_t & iFragCoordX, const float32x4_t & iFragCoordY, const float iEdgeA[3], const float iEdgeB[3], const float iEdgeC[3], float32x4_t oBaryCoord[3])
{
  uint32x4_t mask = vdupq_n_f32(0);

  float32x4_t zeros = vdupq_n_f32(0.f);

  for (int i = 0; i < 3; ++i)
  {
    float32x4_t edgeA = {iEdgeA[i], iEdgeA[i], iEdgeA[i], iEdgeA[i]};
    float32x4_t edgeB = {iEdgeB[i], iEdgeB[i], iEdgeB[i], iEdgeB[i]};
    float32x4_t edgeC = {iEdgeC[i], iEdgeC[i], iEdgeC[i], iEdgeC[i]};

    oBaryCoord[i] = vmlaq_f32(vmlaq_f32(edgeC, edgeB, iFragCoordY), edgeA, iFragCoordX);

    // Test >= 0
    uint32x4_t ge_zero = vcgeq_f32(oBaryCoord[i], zeros);

    if (i == 0)
      mask = ge_zero;
    else
      mask = vandq_u32(mask, ge_zero);
  }

  return mask;
}
#endif

#ifdef SIMD_AVX2
inline __m256 EvalBarycentricCoordinatesAVX2(const __m256 & iFragCoordX, const __m256 & iFragCoordY, const float iEdgeA[3], const float iEdgeB[3], const float iEdgeC[3], __m256 oBaryCoord[3])
{
  __m256 mask = _mm256_setzero_ps();

  for (int i = 0; i < 3; ++i)
  {
    // bary = A * x + B * y + C
    //if ( i < 2 )
    //{
      __m256 edgeA = _mm256_set1_ps(iEdgeA[i]);
      __m256 edgeB = _mm256_set1_ps(iEdgeB[i]);
      __m256 edgeC = _mm256_set1_ps(iEdgeC[i]);

      //__m256 ax = _mm256_mul_ps(edgeA, iFragCoordX);
      //__m256 by = _mm256_mul_ps(edgeB, iFragCoordY);
      //oBaryCoord[i] = _mm256_add_ps(_mm256_add_ps(ax, by), edgeC);
      oBaryCoord[i] = _mm256_fmadd_ps(edgeA, iFragCoordX, _mm256_fmadd_ps(edgeB, iFragCoordY, edgeC));
    //}
    //else
    //  oBaryCoord[2] = _mm256_sub_ps(_mm256_sub_ps(_mm256_set1_ps(1.0f), oBaryCoord[0]), oBaryCoord[1]);

    // Test >= 0
    __m256 ge_zero = _mm256_cmp_ps(oBaryCoord[i], _mm256_setzero_ps(), _CMP_GE_OQ);

    if (i == 0)
      mask = ge_zero;
    else
      mask = _mm256_and_ps(mask, ge_zero);
  }

  return mask;
}
#endif // SIMD_AVX2

}

}

#endif /* _SIMDUtils_hxx_ */
