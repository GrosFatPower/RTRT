#include "RenderTestSIMDUtil.h"

#include "Loader.h"
#include "Mesh.h"
#include "PathUtils.h"
#include "RasterData.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "SIMDUtils.h"
#include "SoftwareVertexShader.h"
#include "ScopedOutputSilencer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace RTRT
{

namespace Tests
{

namespace SIMDTestUtil
{

constexpr float S_SIMDTestEpsilon = 1.e-5f;

bool NearlyEqual( float iActual, float iExpected )
{
  const float difference = std::fabs(iActual - iExpected);
  const float scale = std::max(1.f, std::max(std::fabs(iActual), std::fabs(iExpected)));
  return difference <= (S_SIMDTestEpsilon * scale);
}

bool CheckSIMDFloat( const char * iTestName, const char * iField, int iLane, float iActual, float iExpected )
{
  if ( NearlyEqual(iActual, iExpected) )
    return true;

  std::cerr << "SIMD unit mismatch in " << iTestName << ", " << iField;
  if ( iLane >= 0 )
    std::cerr << ", lane " << iLane;
  std::cerr << ": expected " << iExpected << ", got " << iActual << std::endl;
  return false;
}

bool CheckSIMDVec4( const char * iTestName, const Vec4 & iActual, const Vec4 & iExpected )
{
  return CheckSIMDFloat(iTestName, "x", -1, iActual.x, iExpected.x)
      && CheckSIMDFloat(iTestName, "y", -1, iActual.y, iExpected.y)
      && CheckSIMDFloat(iTestName, "z", -1, iActual.z, iExpected.z)
      && CheckSIMDFloat(iTestName, "w", -1, iActual.w, iExpected.w);
}

bool CheckSIMDVarying( const char * iTestName, const RasterData::Varying & iActual, const RasterData::Varying & iExpected )
{
  return CheckSIMDFloat(iTestName, "world_pos.x", -1, iActual._WorldPos.x, iExpected._WorldPos.x)
      && CheckSIMDFloat(iTestName, "world_pos.y", -1, iActual._WorldPos.y, iExpected._WorldPos.y)
      && CheckSIMDFloat(iTestName, "world_pos.z", -1, iActual._WorldPos.z, iExpected._WorldPos.z)
      && CheckSIMDFloat(iTestName, "uv.x", -1, iActual._UV.x, iExpected._UV.x)
      && CheckSIMDFloat(iTestName, "uv.y", -1, iActual._UV.y, iExpected._UV.y)
      && CheckSIMDFloat(iTestName, "normal.x", -1, iActual._Normal.x, iExpected._Normal.x)
      && CheckSIMDFloat(iTestName, "normal.y", -1, iActual._Normal.y, iExpected._Normal.y)
      && CheckSIMDFloat(iTestName, "normal.z", -1, iActual._Normal.z, iExpected._Normal.z)
      && CheckSIMDFloat(iTestName, "lod", -1, iActual._LOD, iExpected._LOD);
}

bool CheckSIMDTransforms()
{
  const Vec4 vector(0.75f, -1.25f, 2.5f, 1.f);
  Mat4x4 matrices[4] = { Mat4x4(1.f), Mat4x4(1.f), Mat4x4(1.f), Mat4x4(0.f) };
  matrices[1][3] = Vec4(3.f, -2.f, 5.f, 1.f);
  matrices[2][0][0] = 2.f;
  matrices[2][1][1] = -3.f;
  matrices[2][2][2] = .5f;
  matrices[3][0] = Vec4(1.5f, .25f, -.5f, .1f);
  matrices[3][1] = Vec4(-.75f, 2.f, .5f, -.2f);
  matrices[3][2] = Vec4(.4f, -.3f, 1.25f, .6f);
  matrices[3][3] = Vec4(2.f, -1.f, .75f, 1.f);

  for ( int index = 0; index < 4; ++index )
  {
#ifdef SIMD_AVX2
    __m256 matrix[4];
    SIMDUtils::LoadMatrixAVX2(matrices[index], matrix);
    if ( !CheckSIMDVec4("transform", SIMDUtils::ApplyTransformAVX2(matrix, vector), matrices[index] * vector) )
      return false;
#elif defined(SIMD_ARM_NEON)
    float32x4_t matrix[4];
    SIMDUtils::LoadMatrixARM(matrices[index], matrix);
    if ( !CheckSIMDVec4("transform", SIMDUtils::ApplyTransformARM(matrix, vector), matrices[index] * vector) )
      return false;
#endif
  }
  return true;
}

bool CheckSIMDInterpolation()
{
#if defined(SIMD_AVX2) || defined(SIMD_ARM_NEON)
  const float first[8] = { -3.f, -1.5f, .25f, 2.f, 4.5f, 7.f, 9.25f, 12.f };
  const float second[8] = { 6.f, 4.f, 2.5f, .5f, -1.f, -3.5f, -6.f, -8.5f };
  const float third[8] = { .125f, 1.75f, 3.5f, 5.25f, 7.75f, 10.5f, 13.f, 16.25f };
  const float weightsToTest[4][3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f }, { .125f, .625f, .25f } };
  const int laneCount =
#ifdef SIMD_AVX2
    8;
#else
    4;
#endif

  for ( const auto & weights : weightsToTest )
  {
#ifdef SIMD_AVX2
    const __m256 values1 = _mm256_loadu_ps(first);
    const __m256 values2 = _mm256_loadu_ps(second);
    const __m256 values3 = _mm256_loadu_ps(third);
    const __m256 simdWeights[3] = { _mm256_set1_ps(weights[0]), _mm256_set1_ps(weights[1]), _mm256_set1_ps(weights[2]) };
    alignas(32) float actual[8];
    _mm256_store_ps(actual, SIMDUtils::InterpolateAVX2(values1, values2, values3, simdWeights));
#else
    const float32x4_t values1 = vld1q_f32(first);
    const float32x4_t values2 = vld1q_f32(second);
    const float32x4_t values3 = vld1q_f32(third);
    const float32x4_t simdWeights[3] = { vdupq_n_f32(weights[0]), vdupq_n_f32(weights[1]), vdupq_n_f32(weights[2]) };
    float actual[4];
    vst1q_f32(actual, SIMDUtils::InterpolateARM(values1, values2, values3, simdWeights));
#endif
    for ( int lane = 0; lane < laneCount; ++lane )
    {
      const float expected = first[lane] * weights[0] + second[lane] * weights[1] + third[lane] * weights[2];
      if ( !CheckSIMDFloat("interpolation", "value", lane, actual[lane], expected) )
        return false;
    }
  }
#endif
  return true;
}

bool CheckSIMDBarycentrics()
{
#if defined(SIMD_AVX2) || defined(SIMD_ARM_NEON)
  const float x[8] = { 0.f, .2f, .8f, .1f, .7f, 1.f, -.1f, .25f };
  const float y[8] = { 0.f, .3f, .1f, .9f, .4f, 0.f, .1f, .75f };
  const float edgeA[3] = { 1.f, 0.f, -1.f };
  const float edgeB[3] = { 0.f, 1.f, -1.f };
  const float edgeC[3] = { 0.f, 0.f, 1.f };
  const int laneCount =
#ifdef SIMD_AVX2
    8;
#else
    4;
#endif
  float actualWeights[3][8] = {};
  bool actualCoverage[8] = {};

#ifdef SIMD_AVX2
  __m256 weights[3];
  const __m256 mask = SIMDUtils::EvalBarycentricCoordinatesAVX2(_mm256_loadu_ps(x), _mm256_loadu_ps(y), edgeA, edgeB, edgeC, weights);
  alignas(32) float maskValues[8];
  _mm256_store_ps(maskValues, mask);
  for ( int weight = 0; weight < 3; ++weight )
    _mm256_storeu_ps(actualWeights[weight], weights[weight]);
  for ( int lane = 0; lane < laneCount; ++lane )
    actualCoverage[lane] = maskValues[lane] != 0.f;
#else
  float32x4_t weights[3];
  const uint32x4_t mask = SIMDUtils::EvalBarycentricCoordinatesARM(vld1q_f32(x), vld1q_f32(y), edgeA, edgeB, edgeC, weights);
  uint32_t maskValues[4];
  vst1q_u32(maskValues, mask);
  for ( int weight = 0; weight < 3; ++weight )
    vst1q_f32(actualWeights[weight], weights[weight]);
  for ( int lane = 0; lane < laneCount; ++lane )
    actualCoverage[lane] = 0u != maskValues[lane];
#endif

  for ( int lane = 0; lane < laneCount; ++lane )
  {
    float expectedWeights[3];
    const bool expectedCoverage = MathUtil::EvalBarycentricCoordinates(Vec3(x[lane], y[lane], 0.f), edgeA, edgeB, edgeC, expectedWeights);
    for ( int weight = 0; weight < 3; ++weight )
    {
      if ( !CheckSIMDFloat("barycentric", "weight", lane, actualWeights[weight][lane], expectedWeights[weight]) )
        return false;
    }
    if ( actualCoverage[lane] != expectedCoverage )
    {
      std::cerr << "SIMD unit mismatch in barycentric, coverage lane " << lane << std::endl;
      return false;
    }
  }
#endif
  return true;
}

bool CheckSIMDVarying()
{
#if defined(SIMD_AVX2) || defined(SIMD_ARM_NEON)
  RasterData::Varying attributes[3];
  attributes[0]._WorldPos = Vec3(-2.f, 3.5f, .25f);
  attributes[0]._UV = Vec2(.125f, .875f);
  attributes[0]._Normal = Vec3(.5f, -1.f, 2.f);
  attributes[0]._LOD = .75f;
  attributes[1]._WorldPos = Vec3(4.f, -1.25f, 8.f);
  attributes[1]._UV = Vec2(.625f, -.25f);
  attributes[1]._Normal = Vec3(-3.f, .75f, 1.5f);
  attributes[1]._LOD = 2.5f;
  attributes[2]._WorldPos = Vec3(1.5f, 6.f, -4.5f);
  attributes[2]._UV = Vec2(-.5f, .375f);
  attributes[2]._Normal = Vec3(2.25f, 4.f, -.5f);
  attributes[2]._LOD = -1.25f;
  const float weightsToTest[4][3] = { { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f }, { .125f, .625f, .25f } };

  for ( const auto & weights : weightsToTest )
  {
    const RasterData::Varying expected = RasterData::Varying::Interpolate(attributes[0], attributes[1], attributes[2], weights);
#ifdef SIMD_AVX2
    const RasterData::Varying actual = RasterData::Varying::InterpolateAVX2(attributes[0], attributes[1], attributes[2], weights);
#else
    const RasterData::Varying actual = RasterData::Varying::InterpolateARM(attributes[0], attributes[1], attributes[2], weights);
#endif
    if ( !CheckSIMDVarying("varying", actual, expected) )
      return false;
  }
#endif
  return true;
}

RasterData::Vertex BuildRasterVertex( const Mesh & iMesh, size_t iIndex )
{
  RasterData::Vertex vertex;
  vertex._WorldPos = iMesh.GetVertices()[iIndex];
  vertex._Normal = ( iIndex < iMesh.GetNormals().size() ) ? iMesh.GetNormals()[iIndex] : Vec3(0.f, 1.f, 0.f);
  vertex._UV = ( iIndex < iMesh.GetUVs().size() ) ? iMesh.GetUVs()[iIndex] : Vec2(0.f);
  return vertex;
}

RasterData::Varying BuildVarying( const RasterData::Vertex & iVertex )
{
  RasterData::Varying varying;
  varying._WorldPos = iVertex._WorldPos;
  varying._UV = iVertex._UV;
  varying._Normal = iVertex._Normal;
  return varying;
}

bool CheckSIMDLoadedSceneData( bool iQuiet )
{
#if defined(SIMD_AVX2) || defined(SIMD_ARM_NEON)
  const char * scenePaths[] = { "BarberShopChair_01.scene", "diningroom.scene" };
  const Mat4x4 model = glm::translate(Mat4x4(1.f), Vec3(.75f, -.5f, 1.25f)) * glm::scale(Mat4x4(1.f), Vec3(1.25f, .75f, 1.5f));
  const Mat4x4 view = glm::lookAt(Vec3(3.f, 2.f, 5.f), Vec3(0.f), Vec3(0.f, 1.f, 0.f));
  const Mat4x4 projection = glm::perspective(glm::radians(55.f), 16.f / 9.f, .1f, 200.f);

  for ( const char * scenePath : scenePaths )
  {
    Scene scene;
    RenderSettings settings;
    bool loaded = false;
    {
      ScopedOutputSilencer outputSilencer(iQuiet);
      loaded = Loader::LoadScene(PathUtils::GetAssetPath(scenePath), scene, settings);
    }
    if ( !loaded )
    {
      std::cerr << "Unable to load SIMD test scene: " << scenePath << std::endl;
      return false;
    }

    DefaultVertexShader scalarShader(model, view, projection);
#ifdef SIMD_AVX2
    DefaultVertexShaderAVX2 simdShader(model, view, projection);
#else
    DefaultVertexShaderARM simdShader(model, view, projection);
#endif
    size_t testedVertices = 0;
    for ( Mesh * mesh : scene.GetMeshes() )
    {
      if ( !mesh || mesh->GetVertices().empty() )
        continue;

      const size_t vertexCount = mesh->GetVertices().size();
      const size_t sampleCount = std::min<size_t>(32, vertexCount);
      for ( size_t sample = 0; sample < sampleCount; ++sample )
      {
        const size_t index = ( sample * ( vertexCount - 1 ) ) / std::max<size_t>(1, sampleCount - 1);
        const RasterData::Vertex vertex = BuildRasterVertex(*mesh, index);
        RasterData::ProjectedVertex expected;
        RasterData::ProjectedVertex actual;
        scalarShader.Process(vertex, expected);
        simdShader.Process(vertex, actual);
        if ( !CheckSIMDVec4(scenePath, actual._ProjPos, expected._ProjPos) || !CheckSIMDVarying(scenePath, actual._Attrib, expected._Attrib) )
          return false;
        ++testedVertices;
      }

      if ( vertexCount >= 3 )
      {
        const RasterData::Vertex first = BuildRasterVertex(*mesh, 0);
        const RasterData::Vertex middle = BuildRasterVertex(*mesh, vertexCount / 2);
        const RasterData::Vertex last = BuildRasterVertex(*mesh, vertexCount - 1);
        const RasterData::Varying firstVarying = BuildVarying(first);
        const RasterData::Varying middleVarying = BuildVarying(middle);
        const RasterData::Varying lastVarying = BuildVarying(last);
        const float weights[3] = { .125f, .625f, .25f };
        const RasterData::Varying expected = RasterData::Varying::Interpolate(firstVarying, middleVarying, lastVarying, weights);
#ifdef SIMD_AVX2
        const RasterData::Varying actual = RasterData::Varying::InterpolateAVX2(firstVarying, middleVarying, lastVarying, weights);
#else
        const RasterData::Varying actual = RasterData::Varying::InterpolateARM(firstVarying, middleVarying, lastVarying, weights);
#endif
        if ( !CheckSIMDVarying(scenePath, actual, expected) )
          return false;
      }
    }
    if ( 0 == testedVertices )
    {
      std::cerr << "SIMD test scene contains no mesh vertices: " << scenePath << std::endl;
      return false;
    }
  }
#endif
  return true;
}

}

}

}

