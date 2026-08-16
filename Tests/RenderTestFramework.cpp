#include "RenderTestFramework.h"
#include "RenderTestImageUtil.h"
#include "RenderTestSIMDUtil.h"
#include "RenderTestGLTFUtil.h"

#include "DroppedFileUtils.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "PathUtils.h"
#include "Loader.h"
#include "Mesh.h"
#include "RasterData.h"
#include "SIMDUtils.h"
#include "SoftwareVertexShader.h"
#include "RenderTestOutputUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

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
  ioSettings._AntiAliasing = _AntiAliasing;
  ioSettings._TAAHistoryWeight = _TAAHistoryWeight;
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

bool ReadAntiAliasing( const Json & iObject, RenderTestCase & ioTestCase, std::string & oError )
{
  if ( !iObject.contains("anti_aliasing") )
    return true;
  if ( !iObject["anti_aliasing"].is_string() )
    return SetManifestError(oError, "'anti_aliasing' must be none, fxaa, smaa, or taa.");
  const std::string value = iObject["anti_aliasing"].get<std::string>();
  if ( "none" == value ) ioTestCase._AntiAliasing = AntiAliasingMode::None;
  else if ( "fxaa" == value ) ioTestCase._AntiAliasing = AntiAliasingMode::FXAA;
  else if ( "smaa" == value ) ioTestCase._AntiAliasing = AntiAliasingMode::SMAA;
  else if ( "taa" == value ) ioTestCase._AntiAliasing = AntiAliasingMode::TAA;
  else return SetManifestError(oError, "'anti_aliasing' must be none, fxaa, smaa, or taa.");
  return true;
}

bool ReadTAAHistoryWeight( const Json & iObject, RenderTestCase & ioTestCase, std::string & oError )
{
  if ( !iObject.contains("taa_history_weight") )
    return true;
  if ( !iObject["taa_history_weight"].is_number() )
    return SetManifestError(oError, "'taa_history_weight' must be between 0 and 0.98.");
  const float value = iObject["taa_history_weight"].get<float>();
  if ( value < 0.f || value > .98f )
    return SetManifestError(oError, "'taa_history_weight' must be between 0 and 0.98.");
  ioTestCase._TAAHistoryWeight = value;
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
      && ReadAntiAliasing(settings, ioTestCase, oError)
      && ReadTAAHistoryWeight(settings, ioTestCase, oError)
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

  if ( !RunUnitTest("gltf_static_import", [iQuiet]() { return GLTFTestUtil::CheckStaticImport(iQuiet); }) )
    return 1;
  if ( !RunUnitTest("obj_static_import", [iQuiet]() { return GLTFTestUtil::CheckObjImport(iQuiet); }) )
    return 1;
  if ( !RunUnitTest("external_prop_file_filter", []() {
    return DroppedFileUtils::IsDroppedPropPath("prop.obj")
        && DroppedFileUtils::IsDroppedPropPath("prop.gltf")
        && DroppedFileUtils::IsDroppedPropPath("prop.glb")
        && !DroppedFileUtils::IsDroppedPropPath("prop.scene");
  }) )
    return 1;

#if defined(SIMD_AVX2) || defined(SIMD_ARM_NEON)
  if ( !RunUnitTest("simd_transform", []() { return SIMDTestUtil::CheckSIMDTransforms(); }) )
    return 1;

  if ( !RunUnitTest("simd_interpolation", []() { return SIMDTestUtil::CheckSIMDInterpolation(); }) )
    return 1;

  if ( !RunUnitTest("simd_barycentric", []() { return SIMDTestUtil::CheckSIMDBarycentrics(); }) )
    return 1;

  if ( !RunUnitTest("simd_varying", []() { return SIMDTestUtil::CheckSIMDVarying(); }) )
    return 1;

  if ( !RunUnitTest("simd_loaded_scene_data", [iQuiet]() { return SIMDTestUtil::CheckSIMDLoadedSceneData(iQuiet); }) )
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
        "settings": { "specular_ibl": true, "ssr": false, "anti_aliasing": "taa", "taa_history_weight": 0.8 }
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
      && ( AntiAliasingMode::TAA == parsedCases[0]._AntiAliasing ) && ( .8f == parsedCases[0]._TAAHistoryWeight )
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
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "debug_mode": -1 }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 }, "settings": { "anti_aliasing": "invalid" } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 }, "settings": { "taa_history_weight": 1.0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })"
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
