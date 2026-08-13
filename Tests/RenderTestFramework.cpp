#include "RenderTestFramework.h"

#include "RenderSettings.h"
#include "Scene.h"
#include "PathUtils.h"
#include "Loader.h"
#include "Mesh.h"
#include "RasterData.h"
#include "SIMDUtils.h"
#include "SoftwareVertexShader.h"

#include "stb_image_write.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace RTRT
{

namespace Tests
{

using Json = nlohmann::json;

namespace
{

constexpr float S_SIMDTestEpsilon = 1.e-5f;

int DuplicateFileDescriptor( int iDescriptor )
{
#if defined(_WIN32)
  return _dup(iDescriptor);
#else
  return dup(iDescriptor);
#endif
}

int RedirectFileDescriptor( int iSource, int iDestination )
{
#if defined(_WIN32)
  return _dup2(iSource, iDestination);
#else
  return dup2(iSource, iDestination);
#endif
}

void CloseFileDescriptor( int iDescriptor )
{
#if defined(_WIN32)
  _close(iDescriptor);
#else
  close(iDescriptor);
#endif
}

int OpenNullOutput()
{
#if defined(_WIN32)
  int descriptor = -1;
  return ( 0 == _sopen_s(&descriptor, "NUL", _O_WRONLY, _SH_DENYNO, 0) ) ? descriptor : -1;
#else
  return open("/dev/null", O_WRONLY);
#endif
}

int GetFileDescriptor( FILE * iFile )
{
#if defined(_WIN32)
  return _fileno(iFile);
#else
  return fileno(iFile);
#endif
}

class ScopedOutputSilencer
{
public:
  explicit ScopedOutputSilencer( bool iEnabled )
    : _Enabled(iEnabled)
  {
    if ( _Enabled )
    {
      _OldCout = std::cout.rdbuf(_Output.rdbuf());
      _OldCerr = std::cerr.rdbuf(_Output.rdbuf());
      std::fflush(stdout);
      std::fflush(stderr);
      _StdoutDescriptor = GetFileDescriptor(stdout);
      _StderrDescriptor = GetFileDescriptor(stderr);
      _OldStdout = DuplicateFileDescriptor(_StdoutDescriptor);
      _OldStderr = DuplicateFileDescriptor(_StderrDescriptor);
      _NullOutput = OpenNullOutput();
      if ( ( _OldStdout >= 0 ) && ( _OldStderr >= 0 ) && ( _NullOutput >= 0 ) )
      {
        RedirectFileDescriptor(_NullOutput, _StdoutDescriptor);
        RedirectFileDescriptor(_NullOutput, _StderrDescriptor);
      }
    }
  }

  ~ScopedOutputSilencer()
  {
    if ( _Enabled )
    {
      std::fflush(stdout);
      std::fflush(stderr);
      if ( _OldStdout >= 0 )
      {
        RedirectFileDescriptor(_OldStdout, _StdoutDescriptor);
        CloseFileDescriptor(_OldStdout);
      }
      if ( _OldStderr >= 0 )
      {
        RedirectFileDescriptor(_OldStderr, _StderrDescriptor);
        CloseFileDescriptor(_OldStderr);
      }
      if ( _NullOutput >= 0 )
        CloseFileDescriptor(_NullOutput);
      std::cout.rdbuf(_OldCout);
      std::cerr.rdbuf(_OldCerr);
    }
  }

private:
  bool _Enabled = false;
  std::ostringstream _Output;
  std::streambuf * _OldCout = nullptr;
  std::streambuf * _OldCerr = nullptr;
  int _StdoutDescriptor = -1;
  int _StderrDescriptor = -1;
  int _OldStdout = -1;
  int _OldStderr = -1;
  int _NullOutput = -1;
};

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

// ----------------------------------------------------------------------------
// RenderTestCase
// ----------------------------------------------------------------------------
void RenderTestCase::ApplySettings( RenderSettings & ioSettings ) const
{
  ioSettings._WindowResolution = Vec2i(_Width, _Height);
  ioSettings._RenderResolution = Vec2i(_Width, _Height);
  ioSettings._RenderScale = 100;
  ioSettings._FXAA = false;
  ioSettings._ToneMapping = false;
  ioSettings._EnableSkybox = !_EnvironmentMapPath.empty();

  if ( RendererBackend::DeferredRenderer == _Backend )
  {
    ioSettings._SpecularIBL = _SpecularIBL;
    ioSettings._SSR = _SSR;
  }
  else if ( RendererBackend::PathTracer == _Backend )
  {
    ioSettings._Accumulate = _Accumulate;
    ioSettings._AutoScale = _AutoScale;
    ioSettings._TiledRendering = _TiledRendering;
    ioSettings._TileResolution = _TiledRendering ? ioSettings._TileResolution : Vec2i(-1, -1);
    ioSettings._Denoise = _Denoise;
    ioSettings._NbSamplesPerPixel = _SamplesPerPixel;
    ioSettings._Bounces = _Bounces;
  }
  else if ( RendererBackend::SoftwareRasterizer == _Backend )
  {
    ioSettings._TiledRendering = _TiledRendering;
    ioSettings._WBuffer = _WBuffer;
    ioSettings._Transparency = _Transparency;
  }
}

// ----------------------------------------------------------------------------
// ApplyScene
// ----------------------------------------------------------------------------
bool RenderTestCase::ApplyScene( Scene & ioScene ) const
{
  if ( !_EnvironmentMapPath.empty() && !ioScene.LoadEnvMap(PathUtils::GetAssetPath(_EnvironmentMapPath)) )
    return false;

  if ( _OverrideCamera )
  {
    ioScene.GetCamera().Initialize(_CameraPosition, _CameraPivot, _CameraFOV);
    ioScene.GetCamera().SetZNearFar(_CameraNear, _CameraFar);
  }

  return true;
}

// ----------------------------------------------------------------------------
// WritePFM
// ----------------------------------------------------------------------------
bool WritePFM( const std::filesystem::path & iPath, const RenderImage & iImage )
{
  if ( !iImage.IsValid() )
    return false;

  std::error_code error;
  std::filesystem::create_directories(iPath.parent_path(), error);

  std::ofstream output(iPath, std::ios::binary);
  if ( !output )
    return false;

  output << "PF\n" << iImage._Width << " " << iImage._Height << "\n-1.0\n";
  // PFM stores scanlines from the bottom of the image upward.
  for ( int y = iImage._Height - 1; y >= 0; --y )
  {
    for ( int x = 0; x < iImage._Width; ++x )
    {
      const size_t index = ((size_t)y * (size_t)iImage._Width + (size_t)x) * 4u;
      output.write(reinterpret_cast<const char *>(&iImage._Pixels[index]), sizeof(float) * 3);
    }
  }

  return output.good();
}

// ----------------------------------------------------------------------------
// ReadPFM
// ----------------------------------------------------------------------------
bool ReadPFM( const std::filesystem::path & iPath, RenderImage & oImage )
{
  std::ifstream input(iPath, std::ios::binary);
  std::string header;
  int width = 0;
  int height = 0;
  float scale = 0.f;

  if ( !input || !(input >> header) || ( "PF" != header ) || !(input >> width >> height >> scale ) || ( width <= 0 ) || ( height <= 0 ) || ( scale >= 0.f ) )
    return false;

  input.get();
  oImage._Width = width;
  oImage._Height = height;
  oImage._Pixels.assign((size_t)width * (size_t)height * 4u, 1.f);

  for ( int y = height - 1; y >= 0; --y )
  {
    for ( int x = 0; x < width; ++x )
    {
      const size_t index = ((size_t)y * (size_t)width + (size_t)x) * 4u;
      input.read(reinterpret_cast<char *>(&oImage._Pixels[index]), sizeof(float) * 3);
      if ( !input )
        return false;
    }
  }

  return true;
}

// ----------------------------------------------------------------------------
// WriteDiagnosticPNG
// ----------------------------------------------------------------------------
bool WriteDiagnosticPNG( const std::filesystem::path & iPath, const RenderImage & iImage, float iScale )
{
  if ( !iImage.IsValid() )
    return false;

  std::error_code error;
  std::filesystem::create_directories(iPath.parent_path(), error);

  std::vector<unsigned char> pixels((size_t)iImage._Width * (size_t)iImage._Height * 4u);
  for ( size_t i = 0; i < (size_t)iImage._Width * (size_t)iImage._Height; ++i )
  {
    for ( int channel = 0; channel < 3; ++channel )
    {
      const float value = std::max(0.f, iImage._Pixels[i * 4u + (size_t)channel] * iScale);
      const float mapped = value / ( 1.f + value );
      pixels[i * 4u + (size_t)channel] = (unsigned char)(std::pow(mapped, 1.f / 2.2f) * 255.f + .5f);
    }
    pixels[i * 4u + 3u] = 255;
  }

  return ( 0 != stbi_write_png(iPath.string().c_str(), iImage._Width, iImage._Height, 4, pixels.data(), iImage._Width * 4) );
}

// ----------------------------------------------------------------------------
// WriteDiffPNG
// ----------------------------------------------------------------------------
bool WriteDiffPNG( const std::filesystem::path & iPath, const RenderImage & iActual, const RenderImage & iExpected )
{
  if ( !iActual.IsValid() || !iExpected.IsValid() || ( iActual._Width != iExpected._Width ) || ( iActual._Height != iExpected._Height ) )
    return false;

  RenderImage diff;
  diff._Width = iActual._Width;
  diff._Height = iActual._Height;
  diff._Pixels.assign(iActual._Pixels.size(), 1.f);

  for ( size_t i = 0; i < (size_t)diff._Width * (size_t)diff._Height; ++i )
  {
    const float error = std::max(std::abs(iActual._Pixels[i * 4u] - iExpected._Pixels[i * 4u]),
                                 std::max(std::abs(iActual._Pixels[i * 4u + 1u] - iExpected._Pixels[i * 4u + 1u]),
                                          std::abs(iActual._Pixels[i * 4u + 2u] - iExpected._Pixels[i * 4u + 2u])));
    diff._Pixels[i * 4u] = error * 16.f;
    diff._Pixels[i * 4u + 1u] = 0.f;
    diff._Pixels[i * 4u + 2u] = 0.f;
  }

  return WriteDiagnosticPNG(iPath, diff, 1.f);
}

// ----------------------------------------------------------------------------
// CompareImages
// ----------------------------------------------------------------------------
bool CompareImages( const RenderImage & iActual, const RenderImage & iExpected, float iPixelErrorThreshold, ImageMetrics & oMetrics )
{
  oMetrics = ImageMetrics();
  if ( !iActual.IsValid() || !iExpected.IsValid() || ( iActual._Width != iExpected._Width ) || ( iActual._Height != iExpected._Height ) )
    return false;

  const size_t pixelCount = (size_t)iActual._Width * (size_t)iActual._Height;
  float totalError = 0.f;
  for ( size_t i = 0; i < pixelCount; ++i )
  {
    float pixelError = 0.f;
    for ( int channel = 0; channel < 3; ++channel )
    {
      const float error = std::abs(iActual._Pixels[i * 4u + (size_t)channel] - iExpected._Pixels[i * 4u + (size_t)channel]);
      totalError += error;
      pixelError = std::max(pixelError, error);
      oMetrics._MaxAbsoluteError = std::max(oMetrics._MaxAbsoluteError, error);
    }

    if ( pixelError > iPixelErrorThreshold )
      ++oMetrics._MismatchCount;
  }

  oMetrics._MeanAbsoluteError = totalError / (float)(pixelCount * 3u);
  oMetrics._MismatchRatio = (float)oMetrics._MismatchCount / (float)pixelCount;
  return true;
}

// ----------------------------------------------------------------------------
// MatchesThresholds
// ----------------------------------------------------------------------------
bool MatchesThresholds( const ImageMetrics & iMetrics, const RenderTestCase & iTestCase )
{
  return ( iMetrics._MeanAbsoluteError <= iTestCase._MeanAbsoluteErrorThreshold ) &&
         ( iMetrics._MaxAbsoluteError <= iTestCase._MaxAbsoluteErrorThreshold ) &&
         ( iMetrics._MismatchRatio <= iTestCase._MismatchRatioThreshold );
}

namespace
{

bool SetManifestError( std::string & oError, const std::string & iMessage )
{
  oError = iMessage;
  return false;
}

bool ReadString( const Json & iObject, const char * iName, std::string & oValue, std::string & oError, bool iRequired = true )
{
  if ( !iObject.contains(iName) )
    return iRequired ? SetManifestError(oError, std::string("Missing '") + iName + "'.") : true;
  if ( !iObject[iName].is_string() || iObject[iName].get<std::string>().empty() )
    return SetManifestError(oError, std::string("'") + iName + "' must be a non-empty string.");
  oValue = iObject[iName].get<std::string>();
  return true;
}

bool ReadPositiveInt( const Json & iObject, const char * iName, int & oValue, std::string & oError, bool iRequired )
{
  if ( !iObject.contains(iName) )
    return iRequired ? SetManifestError(oError, std::string("Missing '") + iName + "'.") : true;
  if ( !iObject[iName].is_number_integer() || ( iObject[iName].get<int>() <= 0 ) )
    return SetManifestError(oError, std::string("'") + iName + "' must be a positive integer.");
  oValue = iObject[iName].get<int>();
  return true;
}

bool ReadNonNegativeInt( const Json & iObject, const char * iName, int & oValue, std::string & oError )
{
  if ( !iObject.contains(iName) )
    return true;
  if ( !iObject[iName].is_number_integer() || ( iObject[iName].get<int>() < 0 ) )
    return SetManifestError(oError, std::string("'") + iName + "' must be a non-negative integer.");
  oValue = iObject[iName].get<int>();
  return true;
}

bool ReadBool( const Json & iObject, const char * iName, bool & oValue, std::string & oError )
{
  if ( !iObject.contains(iName) )
    return true;
  if ( !iObject[iName].is_boolean() )
    return SetManifestError(oError, std::string("'") + iName + "' must be a boolean.");
  oValue = iObject[iName].get<bool>();
  return true;
}

bool ReadResolution( const Json & iObject, int & oWidth, int & oHeight, std::string & oError, bool iRequired )
{
  if ( !iObject.contains("resolution") )
    return iRequired ? SetManifestError(oError, "Missing 'resolution'.") : true;
  const Json & resolution = iObject["resolution"];
  if ( !resolution.is_array() || ( 2 != resolution.size() ) || !resolution[0].is_number_integer() || !resolution[1].is_number_integer() )
    return SetManifestError(oError, "'resolution' must contain two integers.");
  oWidth = resolution[0].get<int>();
  oHeight = resolution[1].get<int>();
  if ( ( oWidth <= 0 ) || ( oHeight <= 0 ) )
    return SetManifestError(oError, "'resolution' values must be positive.");
  return true;
}

bool ReadVec3( const Json & iValue, const char * iName, Vec3 & oValue, std::string & oError )
{
  if ( !iValue.is_array() || ( 3 != iValue.size() ) || !iValue[0].is_number() || !iValue[1].is_number() || !iValue[2].is_number() )
    return SetManifestError(oError, std::string("'") + iName + "' must contain three numbers.");
  oValue = Vec3(iValue[0].get<float>(), iValue[1].get<float>(), iValue[2].get<float>());
  return true;
}

bool ReadThresholds( const Json & iObject, RenderTestCase & ioTestCase, std::string & oError, bool iRequired )
{
  if ( !iObject.contains("thresholds") )
    return iRequired ? SetManifestError(oError, "Missing 'thresholds'.") : true;
  const Json & thresholds = iObject["thresholds"];
  if ( !thresholds.is_object() )
    return SetManifestError(oError, "'thresholds' must be an object.");

  const std::pair<const char *, float *> values[] =
  {
    { "mean_absolute_error", &ioTestCase._MeanAbsoluteErrorThreshold },
    { "max_absolute_error", &ioTestCase._MaxAbsoluteErrorThreshold },
    { "pixel_error", &ioTestCase._PixelErrorThreshold },
    { "mismatch_ratio", &ioTestCase._MismatchRatioThreshold }
  };
  for ( const auto & value : values )
  {
    if ( !thresholds.contains(value.first) )
    {
      if ( iRequired )
        return SetManifestError(oError, std::string("Missing threshold '") + value.first + "'.");
      continue;
    }
    if ( !thresholds[value.first].is_number() || ( thresholds[value.first].get<float>() < 0.f ) )
      return SetManifestError(oError, std::string("Threshold '") + value.first + "' must be non-negative.");
    *value.second = thresholds[value.first].get<float>();
  }
  return true;
}

bool ReadSettings( const Json & iObject, RenderTestCase & ioTestCase, std::string & oError )
{
  if ( !iObject.contains("settings") )
    return true;
  const Json & settings = iObject["settings"];
  if ( !settings.is_object() )
    return SetManifestError(oError, "'settings' must be an object.");

  return ReadBool(settings, "specular_ibl", ioTestCase._SpecularIBL, oError)
      && ReadBool(settings, "ssr", ioTestCase._SSR, oError)
      && ReadBool(settings, "accumulate", ioTestCase._Accumulate, oError)
      && ReadBool(settings, "auto_scale", ioTestCase._AutoScale, oError)
      && ReadBool(settings, "tiled_rendering", ioTestCase._TiledRendering, oError)
      && ReadBool(settings, "w_buffer", ioTestCase._WBuffer, oError)
      && ReadBool(settings, "transparency", ioTestCase._Transparency, oError)
      && ReadBool(settings, "denoise", ioTestCase._Denoise, oError)
      && ReadPositiveInt(settings, "samples_per_pixel", ioTestCase._SamplesPerPixel, oError, false)
      && ReadPositiveInt(settings, "bounces", ioTestCase._Bounces, oError, false);
}

bool ReadBackend( const Json & iProfile, RenderTestCase & ioTestCase, std::string & oError )
{
  std::string backend;
  if ( !ReadString(iProfile, "backend", backend, oError) )
    return false;
  if ( "software" == backend )
    ioTestCase._Backend = RendererBackend::SoftwareRasterizer;
  else if ( "deferred" == backend )
    ioTestCase._Backend = RendererBackend::DeferredRenderer;
  else if ( "pathtracer" == backend )
    ioTestCase._Backend = RendererBackend::PathTracer;
  else
    return SetManifestError(oError, "'backend' must be software, deferred, or pathtracer.");
  return true;
}

}

// ----------------------------------------------------------------------------
// ParseRenderTestCases
// ----------------------------------------------------------------------------
bool ParseRenderTestCases( const std::string & iContents, std::vector<RenderTestCase> & oTestCases, std::string & oError )
{
  oTestCases.clear();
  oError.clear();

  try
  {
    const Json manifest = Json::parse(iContents);
    if ( !manifest.is_object() || !manifest.contains("version") || !manifest["version"].is_number_integer() || ( 1 != manifest["version"].get<int>() ) )
      return SetManifestError(oError, "Manifest version must be the integer 1.");
    if ( !manifest.contains("profiles") || !manifest["profiles"].is_object() )
      return SetManifestError(oError, "Manifest must contain a 'profiles' object.");
    if ( !manifest.contains("tests") || !manifest["tests"].is_array() )
      return SetManifestError(oError, "Manifest must contain a 'tests' array.");

    const Json & profiles = manifest["profiles"];
    std::set<std::string> names;
    for ( const Json & test : manifest["tests"] )
    {
      if ( !test.is_object() )
        return SetManifestError(oError, "Each test must be an object.");

      RenderTestCase testCase;
      std::string profileName;
      if ( !ReadString(test, "name", testCase._Name, oError) || !ReadString(test, "profile", profileName, oError) || !ReadString(test, "scene", testCase._ScenePath, oError) )
        return false;
      if ( !names.insert(testCase._Name).second )
        return SetManifestError(oError, "Duplicate test name: " + testCase._Name);
      if ( !profiles.contains(profileName) || !profiles[profileName].is_object() )
        return SetManifestError(oError, "Unknown profile: " + profileName);

      const Json & profile = profiles[profileName];
      if ( !ReadBackend(profile, testCase, oError) || !ReadResolution(profile, testCase._Width, testCase._Height, oError, true)
        || !ReadPositiveInt(profile, "frames", testCase._FrameCount, oError, true) || !ReadThresholds(profile, testCase, oError, true)
        || !ReadSettings(profile, testCase, oError) )
        return false;

      if ( !ReadResolution(test, testCase._Width, testCase._Height, oError, false) || !ReadPositiveInt(test, "frames", testCase._FrameCount, oError, false)
        || !ReadThresholds(test, testCase, oError, false) || !ReadSettings(test, testCase, oError)
        || !ReadString(test, "environment_map", testCase._EnvironmentMapPath, oError, false)
        || !ReadNonNegativeInt(test, "debug_mode", testCase._DebugMode, oError) || !ReadBool(test, "diagnostic_only", testCase._DiagnosticOnly, oError)
        || !ReadBool(test, "software_optimized", testCase._SoftwareOptimized, oError)
        || !ReadBool(test, "software_simd", testCase._SoftwareSIMD, oError)
        || !ReadBool(test, "software_fallback", testCase._SoftwareFallback, oError) )
        return false;

      std::string baseline;
      if ( !ReadString(test, "baseline", baseline, oError, false) )
        return false;
      testCase._BaselinePath = baseline.empty() ? ( std::filesystem::path("Tests/Baselines") / ( testCase._Name + ".pfm" ) ) : std::filesystem::path(baseline);

      if ( test.contains("camera") )
      {
        const Json & camera = test["camera"];
        if ( !camera.is_object() || !camera.contains("position") || !camera.contains("pivot") || !camera.contains("fov")
          || !ReadVec3(camera["position"], "camera.position", testCase._CameraPosition, oError)
          || !ReadVec3(camera["pivot"], "camera.pivot", testCase._CameraPivot, oError)
          || !camera["fov"].is_number() || ( camera["fov"].get<float>() <= 0.f ) )
          return SetManifestError(oError, "Camera requires position, pivot, and a positive fov.");
        testCase._OverrideCamera = true;
        testCase._CameraFOV = camera["fov"].get<float>();
        if ( camera.contains("near") )
        {
          if ( !camera["near"].is_number() || ( camera["near"].get<float>() <= 0.f ) )
            return SetManifestError(oError, "Camera near plane must be positive.");
          testCase._CameraNear = camera["near"].get<float>();
        }
        if ( camera.contains("far") )
        {
          if ( !camera["far"].is_number() || ( camera["far"].get<float>() <= testCase._CameraNear ) )
            return SetManifestError(oError, "Camera far plane must be greater than near plane.");
          testCase._CameraFar = camera["far"].get<float>();
        }
      }

      oTestCases.push_back(testCase);
    }
  }
  catch ( const Json::exception & error )
  {
    return SetManifestError(oError, std::string("JSON parse error: ") + error.what());
  }

  return true;
}

// ----------------------------------------------------------------------------
// LoadRenderTestCases
// ----------------------------------------------------------------------------
bool LoadRenderTestCases( const std::filesystem::path & iPath, std::vector<RenderTestCase> & oTestCases, std::string & oError )
{
  std::ifstream input(iPath);
  if ( !input )
    return SetManifestError(oError, "Unable to open manifest: " + iPath.string());

  const std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  return ParseRenderTestCases(contents, oTestCases, oError);
}

// ----------------------------------------------------------------------------
// RunUnitTests
// ----------------------------------------------------------------------------
int RunUnitTests( const std::filesystem::path & iArtifactsDir, bool iUseColor, bool iQuiet )
{
  int passed = 0;
  const auto PrintPassed = [&passed, iUseColor]( const char * iName, double iSeconds )
  {
    if ( iUseColor )
      std::cout << "\x1b[32m";
    std::cout << "[PASS]";
    if ( iUseColor )
      std::cout << "\x1b[0m";
    std::cout << " unit_" << iName << " (" << iSeconds << " s)" << std::endl;
    ++passed;
  };
  const auto PrintFailed = [iUseColor]( const char * iName, double iSeconds )
  {
    if ( iUseColor )
      std::cerr << "\x1b[31m";
    std::cerr << "[FAIL]";
    if ( iUseColor )
      std::cerr << "\x1b[0m";
    std::cerr << " unit_" << iName << " (" << iSeconds << " s)" << std::endl;
  };
  const auto PrintSkipped = [iUseColor]( const char * iName )
  {
    if ( iUseColor )
      std::cout << "\x1b[33m";
    std::cout << "[SKIP]";
    if ( iUseColor )
      std::cout << "\x1b[0m";
    std::cout << " unit_" << iName << " (0 s) - SIMD backend unavailable" << std::endl;
  };
  const auto RunUnitTest = [&PrintPassed, &PrintFailed]( const char * iName, const auto & iTest )
  {
    const auto start = std::chrono::steady_clock::now();
    const bool result = iTest();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if ( result )
      PrintPassed(iName, seconds);
    else
      PrintFailed(iName, seconds);
    return result;
  };

#if defined(SIMD_AVX2) || defined(SIMD_ARM_NEON)
  if ( !RunUnitTest("simd_transform", []() { return CheckSIMDTransforms(); }) )
    return 1;

  if ( !RunUnitTest("simd_interpolation", []() { return CheckSIMDInterpolation(); }) )
    return 1;

  if ( !RunUnitTest("simd_barycentric", []() { return CheckSIMDBarycentrics(); }) )
    return 1;

  if ( !RunUnitTest("simd_varying", []() { return CheckSIMDVarying(); }) )
    return 1;

  if ( !RunUnitTest("simd_loaded_scene_data", [iQuiet]() { return CheckSIMDLoadedSceneData(iQuiet); }) )
    return 1;
#else
  PrintSkipped("simd_transform");
  PrintSkipped("simd_interpolation");
  PrintSkipped("simd_barycentric");
  PrintSkipped("simd_varying");
  PrintSkipped("simd_loaded_scene_data");
#endif

  RenderImage image;
  image._Width = 2;
  image._Height = 1;
  image._Pixels = { 1.f, .5f, .25f, 1.f, .25f, .5f, 1.f, 1.f };

  const std::filesystem::path imagePath = iArtifactsDir / "unit_image.pfm";
  RenderImage loaded;
  if ( !RunUnitTest("pfm_io", [&imagePath, &image, &loaded]() {
    if ( WritePFM(imagePath, image) && ReadPFM(imagePath, loaded) && ( loaded._Pixels == image._Pixels ) )
      return true;
    std::cerr << "Unit test failed: PFM read/write." << std::endl;
    return false;
  }) )
    return 1;

  RenderImage changed = image;
  changed._Pixels[0] = .5f;
  ImageMetrics metrics;
  if ( !RunUnitTest("image_comparison", [&changed, &image, &metrics]() {
    if ( CompareImages(changed, image, .1f, metrics) && ( metrics._MismatchCount == 1u ) && ( metrics._MaxAbsoluteError == .5f ) )
      return true;
    std::cerr << "Unit test failed: image comparison." << std::endl;
    return false;
  }) )
    return 1;

  if ( !RunUnitTest("diagnostic_image_output", [&iArtifactsDir, &changed, &image]() {
    if ( WriteDiffPNG(iArtifactsDir / "unit_diff.png", changed, image) )
      return true;
    std::cerr << "Unit test failed: diagnostic image output." << std::endl;
    return false;
  }) )
    return 1;

  const std::string validManifest = R"({
    "version": 1,
    "profiles": {
      "test": {
        "backend": "deferred",
        "resolution": [64, 32],
        "frames": 3,
        "thresholds": { "mean_absolute_error": 0.1, "max_absolute_error": 0.2, "pixel_error": 0.3, "mismatch_ratio": 0.4 },
        "settings": { "specular_ibl": true, "ssr": false }
      }
    },
    "tests": [
      { "name": "valid", "profile": "test", "scene": "test.scene", "frames": 4, "debug_mode": 16, "diagnostic_only": true,
        "camera": { "position": [1, 2, 3], "pivot": [0, 0, 0], "fov": 40, "near": 0.5 } }
    ]
  })";
  std::vector<RenderTestCase> parsedCases;
  std::string parseError;
  if ( !RunUnitTest("manifest_valid", [&validManifest, &parsedCases, &parseError]() {
    if ( ParseRenderTestCases(validManifest, parsedCases, parseError) && ( 1u == parsedCases.size() )
      && ( 4 == parsedCases[0]._FrameCount ) && parsedCases[0]._OverrideCamera && !parsedCases[0]._SSR
      && ( 16 == parsedCases[0]._DebugMode ) && parsedCases[0]._DiagnosticOnly
      && ( "Tests/Baselines/valid.pfm" == parsedCases[0]._BaselinePath.generic_string() ) )
      return true;
    std::cerr << "Unit test failed: valid render-test manifest. " << parseError << std::endl;
    return false;
  }) )
    return 1;

  const std::string invalidManifests[] =
  {
    R"({ "version": 2, "profiles": {}, "tests": [] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "invalid", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "missing", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }, { "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 0, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "camera": { "position": [0, 0], "pivot": [0, 0, 0], "fov": 40 } }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "debug_mode": -1 }] })"
  };
  if ( !RunUnitTest("manifest_validation", [&invalidManifests, &parsedCases, &parseError]() {
    for ( const std::string & invalidManifest : invalidManifests )
    {
      if ( ParseRenderTestCases(invalidManifest, parsedCases, parseError) )
      {
        std::cerr << "Unit test failed: invalid render-test manifest." << std::endl;
        return false;
      }
    }
    return true;
  }) )
    return 1;

  std::cout << "Unit summary: " << passed << " passed, 0 failed." << std::endl;
  return 0;
}

}

}
