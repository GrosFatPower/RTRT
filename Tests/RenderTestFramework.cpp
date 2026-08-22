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
  ioSettings._ToneMapping = false;
  ioSettings._EnableSkybox = !_EnvironmentMapPath.empty();
  ioSettings._EnableBackGround = _Background;
  ioSettings._EnableUniformLight = _UniformLight;

  if ( RendererBackend::DeferredRenderer == _Backend )
  {
    ioSettings._SpecularIBL = _SpecularIBL;
    ioSettings._SSR = _SSR;
    ioSettings._SSAO = _SSAO;
    ioSettings._Transparency = _Transparency;
    ioSettings._Refraction = _Refraction;
    ioSettings._RefractionMaxSteps = std::max(4, std::min(128, _RefractionMaxSteps));
    ioSettings._RefractionStepSize = std::max(0.001f, _RefractionStepSize);
    ioSettings._RefractionMaxDistance = std::max(0.001f, _RefractionMaxDistance);
    ioSettings._RefractionThickness = std::max(0.001f, _RefractionThickness);
    ioSettings._RefractionEdgeFade = std::max(0.001f, _RefractionEdgeFade);
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

bool ReadPositiveFloat( const Json & iObject, const char * iName, float & oValue, std::string & oError )
{
  if ( !iObject.contains(iName) )
    return true;
  if ( !iObject[iName].is_number() || ( iObject[iName].get<float>() <= 0.f ) )
    return SetManifestError(oError, std::string("'") + iName + "' must be a positive number.");
  oValue = iObject[iName].get<float>();
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
      && ReadBool(settings, "ssao", ioTestCase._SSAO, oError)
      && ReadBool(settings, "background", ioTestCase._Background, oError)
      && ReadBool(settings, "uniform_light", ioTestCase._UniformLight, oError)
      && ReadBool(settings, "accumulate", ioTestCase._Accumulate, oError)
      && ReadBool(settings, "auto_scale", ioTestCase._AutoScale, oError)
      && ReadBool(settings, "tiled_rendering", ioTestCase._TiledRendering, oError)
      && ReadBool(settings, "w_buffer", ioTestCase._WBuffer, oError)
      && ReadBool(settings, "transparency", ioTestCase._Transparency, oError)
      && ReadBool(settings, "refraction", ioTestCase._Refraction, oError)
      && ReadPositiveInt(settings, "refraction_max_steps", ioTestCase._RefractionMaxSteps, oError, false)
      && ReadPositiveFloat(settings, "refraction_step_size", ioTestCase._RefractionStepSize, oError)
      && ReadPositiveFloat(settings, "refraction_max_distance", ioTestCase._RefractionMaxDistance, oError)
      && ReadPositiveFloat(settings, "refraction_thickness", ioTestCase._RefractionThickness, oError)
      && ReadPositiveFloat(settings, "refraction_edge_fade", ioTestCase._RefractionEdgeFade, oError)
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
        || !ReadBool(test, "clamp_comparison", testCase._ClampComparison, oError)
        || !ReadBool(test, "software_optimized", testCase._SoftwareOptimized, oError)
        || !ReadBool(test, "software_simd", testCase._SoftwareSIMD, oError)
        || !ReadBool(test, "software_fallback", testCase._SoftwareFallback, oError) )
        return false;

      std::string baseline;
      if ( !ReadString(test, "baseline", baseline, oError, false) )
        return false;
      testCase._BaselinePath = baseline.empty() ? ( std::filesystem::path("Tests/Baselines") / ( testCase._Name + ".pfm" ) ) : std::filesystem::path(baseline);

      std::string referenceBaseline;
      if ( !ReadString(test, "reference_baseline", referenceBaseline, oError, false) )
        return false;
      if ( test.contains("reference_baseline") && referenceBaseline.empty() )
        return SetManifestError(oError, "'reference_baseline' must not be empty.");
      testCase._ReferenceBaselinePath = referenceBaseline;

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
  if ( !RunUnitTest("scene_optional_mesh_warning", [&iArtifactsDir, iQuiet]() {
    std::error_code errorCode;
    std::filesystem::create_directories(iArtifactsDir, errorCode);
    const std::filesystem::path scenePath = iArtifactsDir / "optional_mesh.scene";
    std::ofstream file(scenePath);
    file << "mesh\n{\n  file missing.obj\n}\n";
    file.close();

    Scene scene;
    RenderSettings settings;
    bool loaded = false;
    {
      ScopedOutputSilencer outputSilencer(iQuiet);
      loaded = Loader::LoadScene(scenePath.string(), scene, settings);
    }
    return loaded && ( 0 == scene.GetNbMeshes() ) && ( 0 == scene.GetNbMeshInstances() );
  }) )
    return 1;
  if ( !RunUnitTest("scene_light_argument_diagnostic", [&iArtifactsDir, iQuiet]() {
    std::error_code errorCode;
    std::filesystem::create_directories(iArtifactsDir, errorCode);
    const std::filesystem::path scenePath = iArtifactsDir / "invalid_light.scene";
    std::ofstream file(scenePath);
    file << "light\n{\n  position 1.0 2.0\n}\n";
    file.close();

    Scene scene;
    RenderSettings settings;
    bool loaded = true;
    {
      ScopedOutputSilencer outputSilencer(iQuiet);
      loaded = Loader::LoadScene(scenePath.string(), scene, settings);
    }
    return !loaded && ( 0 == scene.GetNbLights() );
  }) )
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

  if ( !RunUnitTest("texture_bucket_catalog", [iQuiet]() {
    ScopedOutputSilencer outputSilencer(iQuiet);
    std::vector<unsigned char> smallPixels(256 * 128 * 4, 64);
    std::vector<unsigned char> widePixels(1024 * 850 * 4, 128);
    std::vector<unsigned char> oversizedPixels(2048 * 4, 192);
    std::vector<unsigned char> unsupportedPixels(16 * 16 * 3, 255);

    Scene scene;
    int smallID = scene.AddTexture("unit_small", smallPixels.data(), 256, 128, 4);
    int wideID = scene.AddTexture("unit_wide", widePixels.data(), 1024, 850, 4);
    int oversizedID = scene.AddTexture("unit_oversized", oversizedPixels.data(), 2048, 1, 4);
    scene.AddTexture("unit_unsupported", unsupportedPixels.data(), 16, 16, 3);

    Material material;
    material._BaseColorTexId = static_cast<float>(smallID);
    material._NormalMapTexID = static_cast<float>(wideID);
    material._EmissionMapTexID = static_cast<float>(oversizedID);
    scene.AddMaterial(material, "unit_bucket_material");
    scene.CompileMeshData(Vec2i(1024), true, false);

    const std::vector<TextureArrayMapping> & mappings = scene.GetTextureArrayMappings();
    const std::array<CompiledTextureBucket, S_TextureBucketCount> & buckets = scene.GetCompiledTextureBuckets();
    if ( ( mappings.size() != 4u )
      || ( mappings[smallID]._BucketID != 0 ) || ( mappings[smallID]._ContentWidth != 256 ) || ( mappings[smallID]._ContentHeight != 128 )
      || ( mappings[wideID]._BucketID != 2 ) || ( mappings[wideID]._ContentWidth != 1024 ) || ( mappings[wideID]._ContentHeight != 850 )
      || ( mappings[oversizedID]._BucketID != 2 ) || ( mappings[oversizedID]._ContentWidth != 1024 ) || ( mappings[oversizedID]._ContentHeight != 1 )
      || ( mappings[3]._BucketID != -1 ) || ( buckets[0]._LayerCount != 1 ) || ( buckets[2]._LayerCount != 2 ) )
    {
      std::cerr << "Unit test failed: texture bucket catalog." << std::endl;
      return false;
    }
    return true;
  }) )
    return 1;

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
        "settings": { "specular_ibl": true, "ssr": false, "refraction": true, "refraction_max_steps": 64,
          "refraction_step_size": 0.12, "refraction_max_distance": 24.0, "refraction_thickness": 0.3, "refraction_edge_fade": 0.1 }
      }
    },
    "tests": [
      { "name": "valid", "profile": "test", "scene": "test.scene", "frames": 4, "debug_mode": 16, "diagnostic_only": true, "clamp_comparison": true,
        "reference_baseline": "Tests/Baselines/reference.pfm",
        "camera": { "position": [1, 2, 3], "pivot": [0, 0, 0], "fov": 40, "near": 0.5 } }
    ]
  })";
  std::vector<RenderTestCase> parsedCases;
  std::string parseError;
  if ( !RunUnitTest("manifest_valid", [&validManifest, &parsedCases, &parseError]() {
    if ( ParseRenderTestCases(validManifest, parsedCases, parseError) && ( 1u == parsedCases.size() )
      && ( 4 == parsedCases[0]._FrameCount ) && parsedCases[0]._OverrideCamera && !parsedCases[0]._SSR
      && parsedCases[0]._Refraction && ( 64 == parsedCases[0]._RefractionMaxSteps )
      && ( std::abs(parsedCases[0]._RefractionStepSize - 0.12f) < 1e-6f )
      && ( std::abs(parsedCases[0]._RefractionMaxDistance - 24.f) < 1e-6f )
      && ( std::abs(parsedCases[0]._RefractionThickness - 0.3f) < 1e-6f )
      && ( std::abs(parsedCases[0]._RefractionEdgeFade - 0.1f) < 1e-6f )
      && ( 16 == parsedCases[0]._DebugMode ) && parsedCases[0]._DiagnosticOnly
      && parsedCases[0]._ClampComparison
      && ( "Tests/Baselines/valid.pfm" == parsedCases[0]._BaselinePath.generic_string() )
      && ( "Tests/Baselines/reference.pfm" == parsedCases[0]._ReferenceBaselinePath.generic_string() ) )
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
    R"({ "version": 1, "profiles": { "test": { "backend": "software", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "reference_baseline": "" }] })",
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [1, 1], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 }, "settings": { "refraction_step_size": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene" }] })"
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
