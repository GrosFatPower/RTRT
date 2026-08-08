#include "RenderTestFramework.h"

#include "RenderSettings.h"
#include "RasterData.h"
#include "Scene.h"
#include "PathUtils.h"

#include "stb_image_write.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <set>

namespace RTRT
{

namespace Tests
{

using Json = nlohmann::json;

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
  else if ( RendererBackend::SoftwareRasterizer == _Backend )
  {
    ioSettings._TiledRendering = _TiledRendering;
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
        || !ReadThresholds(test, testCase, oError, false) || !ReadString(test, "environment_map", testCase._EnvironmentMapPath, oError, false) )
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
int RunUnitTests( const std::filesystem::path & iArtifactsDir, bool iUseColor )
{
  int passed = 0;
  const auto PrintPassed = [&passed, iUseColor]( const char * iName )
  {
    if ( iUseColor )
      std::cout << "\x1b[32m";
    std::cout << "[PASS] unit_" << iName;
    if ( iUseColor )
      std::cout << "\x1b[0m";
    std::cout << std::endl;
    ++passed;
  };
  const auto PrintFailed = [iUseColor]( const char * iName )
  {
    if ( iUseColor )
      std::cerr << "\x1b[31m";
    std::cerr << "[FAIL] unit_" << iName;
    if ( iUseColor )
      std::cerr << "\x1b[0m";
    std::cerr << std::endl;
  };

  RenderImage image;
  image._Width = 2;
  image._Height = 1;
  image._Pixels = { 1.f, .5f, .25f, 1.f, .25f, .5f, 1.f, 1.f };

  const std::filesystem::path imagePath = iArtifactsDir / "unit_image.pfm";
  RenderImage loaded;
  if ( !WritePFM(imagePath, image) || !ReadPFM(imagePath, loaded) || ( loaded._Pixels != image._Pixels ) )
  {
    std::cerr << "Unit test failed: PFM read/write." << std::endl;
    PrintFailed("pfm_io");
    return 1;
  }
  PrintPassed("pfm_io");

  RenderImage changed = image;
  changed._Pixels[0] = .5f;
  ImageMetrics metrics;
  if ( !CompareImages(changed, image, .1f, metrics) || ( metrics._MismatchCount != 1u ) || ( metrics._MaxAbsoluteError != .5f ) )
  {
    std::cerr << "Unit test failed: image comparison." << std::endl;
    PrintFailed("image_comparison");
    return 1;
  }
  PrintPassed("image_comparison");

  const Vec3 diagonalA[3] = { Vec3(0.f, 0.f, 0.f), Vec3(2.f, 0.f, 0.f), Vec3(0.f, 2.f, 0.f) };
  const Vec3 diagonalB[3] = { Vec3(2.f, 0.f, 0.f), Vec3(2.f, 2.f, 0.f), Vec3(0.f, 2.f, 0.f) };
  const Vec3 horizontalA[3] = { Vec3(0.f, 0.f, 0.f), Vec3(2.f, 0.f, 0.f), Vec3(0.f, 2.f, 0.f) };
  const Vec3 horizontalB[3] = { Vec3(0.f, 0.f, 0.f), Vec3(0.f, -2.f, 0.f), Vec3(2.f, 0.f, 0.f) };
  const Vec3 verticalA[3] = { Vec3(0.f, 0.f, 0.f), Vec3(2.f, 0.f, 0.f), Vec3(0.f, 2.f, 0.f) };
  const Vec3 verticalB[3] = { Vec3(0.f, 0.f, 0.f), Vec3(0.f, 2.f, 0.f), Vec3(-2.f, 0.f, 0.f) };
  const Vec3 bounds[3] = { Vec3(.25f, .25f, 0.f), Vec3(2.75f, .25f, 0.f), Vec3(.25f, 2.75f, 0.f) };
  const Vec3 degenerate[3] = { Vec3(0.f, 0.f, 0.f), Vec3(.001f, 0.f, 0.f), Vec3(0.f, .001f, 0.f) };
  RasterData::CoverageTriangle triangleDiagonalA;
  RasterData::CoverageTriangle triangleDiagonalB;
  RasterData::CoverageTriangle triangleHorizontalA;
  RasterData::CoverageTriangle triangleHorizontalB;
  RasterData::CoverageTriangle triangleVerticalA;
  RasterData::CoverageTriangle triangleVerticalB;
  RasterData::CoverageTriangle triangleBounds;
  RasterData::CoverageTriangle triangleDegenerate;
  const int64_t sample = RasterData::CoverageTriangle::_SubPixelScale;
  if ( !triangleDiagonalA.Initialize(diagonalA) || !triangleDiagonalB.Initialize(diagonalB)
    || !triangleHorizontalA.Initialize(horizontalA) || !triangleHorizontalB.Initialize(horizontalB)
    || !triangleVerticalA.Initialize(verticalA) || !triangleVerticalB.Initialize(verticalB)
    || !triangleBounds.Initialize(bounds) || triangleDegenerate.Initialize(degenerate)
    || ( triangleDiagonalA.CoversPixel(1, 0) == triangleDiagonalB.CoversPixel(1, 0) )
    || ( triangleHorizontalA.CoversPoint(sample, 0) == triangleHorizontalB.CoversPoint(sample, 0) )
    || ( triangleVerticalA.CoversPoint(0, sample) == triangleVerticalB.CoversPoint(0, sample) )
    || ( 0 != triangleBounds._PixelMinX ) || ( 2 != triangleBounds._PixelMaxX )
    || ( 0 != triangleBounds._PixelMinY ) || ( 2 != triangleBounds._PixelMaxY ) )
  {
    std::cerr << "Unit test failed: deterministic raster coverage." << std::endl;
    PrintFailed("raster_coverage");
    return 1;
  }
  PrintPassed("raster_coverage");

  if ( !WriteDiffPNG(iArtifactsDir / "unit_diff.png", changed, image) )
  {
    std::cerr << "Unit test failed: diagnostic image output." << std::endl;
    PrintFailed("diagnostic_image_output");
    return 1;
  }
  PrintPassed("diagnostic_image_output");

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
      { "name": "valid", "profile": "test", "scene": "test.scene", "frames": 4,
        "camera": { "position": [1, 2, 3], "pivot": [0, 0, 0], "fov": 40, "near": 0.5 } }
    ]
  })";
  std::vector<RenderTestCase> parsedCases;
  std::string parseError;
  if ( !ParseRenderTestCases(validManifest, parsedCases, parseError) || ( 1u != parsedCases.size() )
    || ( 4 != parsedCases[0]._FrameCount ) || !parsedCases[0]._OverrideCamera || parsedCases[0]._SSR
    || ( "Tests/Baselines/valid.pfm" != parsedCases[0]._BaselinePath.generic_string() ) )
  {
    std::cerr << "Unit test failed: valid render-test manifest. " << parseError << std::endl;
    PrintFailed("manifest_valid");
    return 1;
  }
  PrintPassed("manifest_valid");

  const std::string invalidManifests[] =
  {
    R"({ "version": 2, "profiles": {}, "tests": [] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "invalid", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "missing", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }, { "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 0, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "camera": { "position": [0, 0], "pivot": [0, 0, 0], "fov": 40 } }] })"
  };
  for ( const std::string & invalidManifest : invalidManifests )
  {
    if ( ParseRenderTestCases(invalidManifest, parsedCases, parseError) )
    {
      std::cerr << "Unit test failed: invalid render-test manifest." << std::endl;
      PrintFailed("manifest_validation");
      return 1;
    }
  }
  PrintPassed("manifest_validation");

  std::cout << "Unit summary: " << passed << " passed, 0 failed." << std::endl;
  return 0;
}

}

}
