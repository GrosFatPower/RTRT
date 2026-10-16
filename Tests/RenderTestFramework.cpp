#include "RenderTestFramework.h"
#include "RenderTestImageUtil.h"
#include "RenderTestSIMDUtil.h"
#include "RenderTestGLTFUtil.h"

#include "DroppedFileUtils.h"
#include "Camera.h"
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
    ioSettings._SSRMaxSteps = std::max(4, std::min(128, _SSRMaxSteps));
    ioSettings._SSRPixelStride = std::max(0.25f, std::min(4.f, _SSRPixelStride));
    ioSettings._SSRStartBias = std::max(0.25f, std::min(8.f, _SSRStartBias));
    ioSettings._SSRMaxDistance = std::max(0.001f, _SSRMaxDistance);
    ioSettings._SSRThickness = std::max(0.001f, _SSRThickness);
    ioSettings._SSAO = _SSAO;
    ioSettings._Transparency = _Transparency;
    ioSettings._Refraction = _Refraction;
    ioSettings._RefractionMaxSteps = std::max(4, std::min(128, _RefractionMaxSteps));
    ioSettings._RefractionPixelStride = std::max(0.25f, std::min(4.f, _RefractionPixelStride));
    ioSettings._RefractionStartBias = std::max(0.25f, std::min(8.f, _RefractionStartBias));
    ioSettings._RefractionMaxDistance = std::max(0.001f, _RefractionMaxDistance);
    ioSettings._RefractionThickness = std::max(0.001f, _RefractionThickness);
    ioSettings._RefractionEdgeFade = std::max(0.001f, _RefractionEdgeFade);
    ioSettings._SpecularIBLIntensity = _SpecularIBLIntensity;
    ioSettings._SpecularIBLMaxRoughness = _SpecularIBLMaxRoughness;
    ioSettings._ShadowMapping = _Shadows;
    ioSettings._PBRDirectLighting = _PBRDirectLighting;
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

  if ( _Material._Enabled )
  {
    std::vector<Material> & materials = ioScene.GetMaterials();
    if ( materials.empty() )
      return false;

    int materialID = 0;
    if ( !_Material._Name.empty() )
    {
      materialID = ioScene.FindMaterialID(_Material._Name);
      if ( ( materialID < 0 ) || ( materialID >= static_cast<int>(materials.size()) ) )
        return false;
    }

    Material & material = materials[materialID];
    if ( _Material._HasAlbedo )
      material._Albedo = _Material._Albedo;
    if ( _Material._HasEmission )
      material._Emission = _Material._Emission;
    if ( _Material._HasMetallic )
      material._Metallic = _Material._Metallic;
    if ( _Material._HasRoughness )
      material._Roughness = _Material._Roughness;
    if ( _Material._HasReflectance )
      material._Reflectance = _Material._Reflectance;
    if ( _Material._HasSubsurface )
      material._Subsurface = _Material._Subsurface;
    if ( _Material._HasSpecTint )
      material._SpecTint = _Material._SpecTint;
    if ( _Material._HasAnisotropic )
      material._Anisotropic = _Material._Anisotropic;
    if ( _Material._HasSheen )
      material._Sheen = _Material._Sheen;
    if ( _Material._HasSheenTint )
      material._SheenTint = _Material._SheenTint;
    if ( _Material._HasClearcoat )
      material._Clearcoat = _Material._Clearcoat;
    if ( _Material._HasClearcoatGloss )
      material._ClearcoatGloss = _Material._ClearcoatGloss;
    if ( _Material._HasSpecTrans )
      material._SpecTrans = _Material._SpecTrans;
    if ( _Material._HasIOR )
      material._IOR = _Material._IOR;
    if ( _Material._HasOpacity )
      material._Opacity = _Material._Opacity;
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

bool ReadNonNegativeFloat( const Json & iObject, const char * iName, float & oValue, std::string & oError )
{
  if ( !iObject.contains(iName) )
    return true;
  if ( !iObject[iName].is_number() || ( iObject[iName].get<float>() < 0.f ) )
    return SetManifestError(oError, std::string("'") + iName + "' must be a non-negative number.");
  oValue = iObject[iName].get<float>();
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

  if ( !( ReadBool(settings, "specular_ibl", ioTestCase._SpecularIBL, oError)
      && ReadBool(settings, "ssr", ioTestCase._SSR, oError)
      && ReadPositiveInt(settings, "ssr_max_steps", ioTestCase._SSRMaxSteps, oError, false)
      && ReadPositiveFloat(settings, "ssr_max_distance", ioTestCase._SSRMaxDistance, oError)
      && ReadPositiveFloat(settings, "ssr_thickness", ioTestCase._SSRThickness, oError)
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
      && ReadPositiveFloat(settings, "refraction_max_distance", ioTestCase._RefractionMaxDistance, oError)
      && ReadPositiveFloat(settings, "refraction_thickness", ioTestCase._RefractionThickness, oError)
      && ReadPositiveFloat(settings, "refraction_edge_fade", ioTestCase._RefractionEdgeFade, oError)
      && ReadBool(settings, "denoise", ioTestCase._Denoise, oError)
      && ReadPositiveInt(settings, "samples_per_pixel", ioTestCase._SamplesPerPixel, oError, false)
      && ReadPositiveInt(settings, "bounces", ioTestCase._Bounces, oError, false)
      && ReadNonNegativeFloat(settings, "specular_ibl_intensity", ioTestCase._SpecularIBLIntensity, oError)
      && ReadNonNegativeFloat(settings, "specular_ibl_max_roughness", ioTestCase._SpecularIBLMaxRoughness, oError)
      && ReadBool(settings, "shadows", ioTestCase._Shadows, oError)
      && ReadBool(settings, "pbr_direct_lighting", ioTestCase._PBRDirectLighting, oError) ) )
    return false;

  float legacySSRStep = 0.18f;
  float legacyRefractionStep = 0.18f;
  if ( !ReadPositiveFloat(settings, "ssr_step_size", legacySSRStep, oError)
    || !ReadPositiveFloat(settings, "refraction_step_size", legacyRefractionStep, oError)
    || !ReadPositiveFloat(settings, "ssr_pixel_stride", ioTestCase._SSRPixelStride, oError)
    || !ReadPositiveFloat(settings, "ssr_start_bias", ioTestCase._SSRStartBias, oError)
    || !ReadPositiveFloat(settings, "refraction_pixel_stride", ioTestCase._RefractionPixelStride, oError)
    || !ReadPositiveFloat(settings, "refraction_start_bias", ioTestCase._RefractionStartBias, oError) )
    return false;

  if ( settings.contains("ssr_step_size") && !settings.contains("ssr_pixel_stride") )
    ioTestCase._SSRPixelStride = std::max(0.25f, std::min(4.f, legacySSRStep / 0.18f));
  if ( settings.contains("refraction_step_size") && !settings.contains("refraction_pixel_stride") )
    ioTestCase._RefractionPixelStride = std::max(0.25f, std::min(4.f, legacyRefractionStep / 0.18f));
  return true;
}

bool ReadUnitIntervalFloat( const Json & iObject, const char * iName, bool & oHasValue, float & oValue, std::string & oError )
{
  if ( !iObject.contains(iName) )
    return true;
  if ( !iObject[iName].is_number() )
    return SetManifestError(oError, std::string("'material.") + iName + "' must be a number.");
  const float value = iObject[iName].get<float>();
  if ( !std::isfinite(value) || ( value < 0.f ) || ( value > 1.f ) )
    return SetManifestError(oError, std::string("'material.") + iName + "' must be in [0, 1].");
  oHasValue = true;
  oValue = value;
  return true;
}

bool ReadNonNegativeVec3( const Json & iObject, const char * iName, bool & oHasValue, Vec3 & oValue, std::string & oError )
{
  if ( !iObject.contains(iName) )
    return true;
  if ( !ReadVec3(iObject[iName], ( std::string("material.") + iName ).c_str(), oValue, oError) )
    return false;
  if ( !std::isfinite(oValue.x) || !std::isfinite(oValue.y) || !std::isfinite(oValue.z)
    || ( oValue.x < 0.f ) || ( oValue.y < 0.f ) || ( oValue.z < 0.f ) )
    return SetManifestError(oError, std::string("'material.") + iName + "' must be finite and non-negative.");
  oHasValue = true;
  return true;
}

bool ReadMaterialOverride( const Json & iTest, MaterialOverride & ioMaterial, std::string & oError )
{
  if ( !iTest.contains("material") )
    return true;
  const Json & material = iTest["material"];
  if ( !material.is_object() )
    return SetManifestError(oError, "'material' must be an object.");

  ioMaterial._Enabled = true;
  if ( !ReadString(material, "name", ioMaterial._Name, oError, false)
    || !ReadNonNegativeVec3(material, "color", ioMaterial._HasAlbedo, ioMaterial._Albedo, oError)
    || !ReadNonNegativeVec3(material, "albedo", ioMaterial._HasAlbedo, ioMaterial._Albedo, oError)
    || !ReadNonNegativeVec3(material, "emission", ioMaterial._HasEmission, ioMaterial._Emission, oError)
    || !ReadUnitIntervalFloat(material, "metallic", ioMaterial._HasMetallic, ioMaterial._Metallic, oError)
    || !ReadUnitIntervalFloat(material, "roughness", ioMaterial._HasRoughness, ioMaterial._Roughness, oError)
    || !ReadUnitIntervalFloat(material, "reflectance", ioMaterial._HasReflectance, ioMaterial._Reflectance, oError)
    || !ReadUnitIntervalFloat(material, "subsurface", ioMaterial._HasSubsurface, ioMaterial._Subsurface, oError)
    || !ReadUnitIntervalFloat(material, "specular_tint", ioMaterial._HasSpecTint, ioMaterial._SpecTint, oError)
    || !ReadUnitIntervalFloat(material, "anisotropic", ioMaterial._HasAnisotropic, ioMaterial._Anisotropic, oError)
    || !ReadUnitIntervalFloat(material, "sheen", ioMaterial._HasSheen, ioMaterial._Sheen, oError)
    || !ReadUnitIntervalFloat(material, "sheen_tint", ioMaterial._HasSheenTint, ioMaterial._SheenTint, oError)
    || !ReadUnitIntervalFloat(material, "clearcoat", ioMaterial._HasClearcoat, ioMaterial._Clearcoat, oError)
    || !ReadUnitIntervalFloat(material, "clearcoat_gloss", ioMaterial._HasClearcoatGloss, ioMaterial._ClearcoatGloss, oError)
    || !ReadUnitIntervalFloat(material, "transmission", ioMaterial._HasSpecTrans, ioMaterial._SpecTrans, oError)
    || !ReadUnitIntervalFloat(material, "spec_trans", ioMaterial._HasSpecTrans, ioMaterial._SpecTrans, oError)
    || !ReadUnitIntervalFloat(material, "opacity", ioMaterial._HasOpacity, ioMaterial._Opacity, oError) )
    return false;

  if ( material.contains("ior") )
  {
    if ( !material["ior"].is_number() )
      return SetManifestError(oError, "'material.ior' must be a number.");
    const float ior = material["ior"].get<float>();
    if ( !std::isfinite(ior) || ( ior < 1.f ) )
      return SetManifestError(oError, "'material.ior' must be >= 1.0.");
    ioMaterial._HasIOR = true;
    ioMaterial._IOR = ior;
  }

  return true;
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
    if ( ( std::string::npos != iContents.find("\"ssr_step_size\"") )
      || ( std::string::npos != iContents.find("\"refraction_step_size\"") ) )
      std::cerr << "Deprecated SSR/refraction step-size setting; use pixel-stride settings.\n";
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
        || !ReadBool(test, "software_fallback", testCase._SoftwareFallback, oError)
        || !ReadMaterialOverride(test, testCase._Material, oError) )
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

      if ( test.contains("white_furnace") )
      {
        const Json & furnace = test["white_furnace"];
        if ( !furnace.is_object() )
          return SetManifestError(oError, "'white_furnace' must be an object.");
        testCase._WhiteFurnace._Enabled = true;
        if ( furnace.contains("max_channel") )
        {
          if ( !furnace["max_channel"].is_number() )
            return SetManifestError(oError, "'white_furnace.max_channel' must be a number.");
          testCase._WhiteFurnace._MaxChannel = furnace["max_channel"].get<float>();
        }
        if ( furnace.contains("min_channel") )
        {
          if ( !furnace["min_channel"].is_number() )
            return SetManifestError(oError, "'white_furnace.min_channel' must be a number.");
          testCase._WhiteFurnace._MinChannel = furnace["min_channel"].get<float>();
        }
        if ( testCase._WhiteFurnace._MaxChannel <= testCase._WhiteFurnace._MinChannel )
          return SetManifestError(oError, "'white_furnace.max_channel' must be greater than 'min_channel'.");
        if ( furnace.contains("probes") )
        {
          const Json & probes = furnace["probes"];
          if ( !probes.is_object() )
            return SetManifestError(oError, "'white_furnace.probes' must be an object.");
          for ( auto it = probes.begin(); it != probes.end(); ++it )
          {
            WhiteFurnaceProbe probe;
            probe._Name = it.key();
            const Json & probeJson = it.value();
            if ( !probeJson.is_object() || !probeJson.contains("rect") )
              return SetManifestError(oError, "Probe '" + probe._Name + "' must have a 'rect'.");
            const Json & rect = probeJson["rect"];
            if ( !rect.is_array() || ( 4 != rect.size() ) || !rect[0].is_number() || !rect[1].is_number() || !rect[2].is_number() || !rect[3].is_number() )
              return SetManifestError(oError, "Probe '" + probe._Name + "' rect must contain four numbers.");
            probe._RectX = rect[0].get<float>();
            probe._RectY = rect[1].get<float>();
            probe._RectW = rect[2].get<float>();
            probe._RectH = rect[3].get<float>();
            if ( ( probe._RectX < 0.f ) || ( probe._RectY < 0.f ) || ( probe._RectW <= 0.f ) || ( probe._RectH <= 0.f )
              || ( probe._RectX + probe._RectW > 1.f ) || ( probe._RectY + probe._RectH > 1.f ) )
              return SetManifestError(oError, "Probe '" + probe._Name + "' rect must be a normalized rectangle inside [0,1].");
            if ( probeJson.contains("expected") )
            {
              const Json & expected = probeJson["expected"];
              if ( !expected.is_array() || ( 3 != expected.size() ) || !expected[0].is_number() || !expected[1].is_number() || !expected[2].is_number() )
                return SetManifestError(oError, "Probe '" + probe._Name + "' expected must contain three numbers.");
              probe._Expected = Vec3(expected[0].get<float>(), expected[1].get<float>(), expected[2].get<float>());
              if ( !std::isfinite(probe._Expected.x) || !std::isfinite(probe._Expected.y) || !std::isfinite(probe._Expected.z)
                || ( probe._Expected.x < 0.f ) || ( probe._Expected.y < 0.f ) || ( probe._Expected.z < 0.f ) )
                return SetManifestError(oError, "Probe '" + probe._Name + "' expected must be finite and non-negative.");
              probe._HasExpected = true;
              if ( !probeJson.contains("tolerance") || !probeJson["tolerance"].is_number() || ( probeJson["tolerance"].get<float>() < 0.f ) )
                return SetManifestError(oError, "Probe '" + probe._Name + "' requires a non-negative 'tolerance' when 'expected' is set.");
              probe._Tolerance = probeJson["tolerance"].get<float>();
              if ( !std::isfinite(probe._Tolerance) )
                return SetManifestError(oError, "Probe '" + probe._Name + "' tolerance must be finite.");
            }
            testCase._WhiteFurnace._Probes.push_back(probe);
          }
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
// EvaluateWhiteFurnace
// ----------------------------------------------------------------------------
WhiteFurnaceResult EvaluateWhiteFurnace( const RenderImage & iImage, const WhiteFurnaceConfig & iConfig )
{
  WhiteFurnaceResult result;
  result._Passed = true;
  result._GlobalMinChannel = std::numeric_limits<float>::max();
  result._GlobalMaxChannel = -std::numeric_limits<float>::max();

  for ( size_t i = 0; i < (size_t)iImage._Width * (size_t)iImage._Height; ++i )
  {
    for ( int ch = 0; ch < 3; ++ch )
    {
      float value = iImage._Pixels[i * 4u + (size_t)ch];
      if ( value < result._GlobalMinChannel )
        result._GlobalMinChannel = value;
      if ( value > result._GlobalMaxChannel )
        result._GlobalMaxChannel = value;
    }
  }

  if ( ( result._GlobalMinChannel < iConfig._MinChannel ) || ( result._GlobalMaxChannel > iConfig._MaxChannel ) )
  {
    result._GlobalBoundsPassed = false;
    result._Passed = false;
  }

  for ( const WhiteFurnaceProbe & probe : iConfig._Probes )
  {
    WhiteFurnaceProbeResult probeResult;
    probeResult._Name = probe._Name;

    int x0 = std::max(0, (int)( probe._RectX * iImage._Width ));
    int y0 = std::max(0, (int)( probe._RectY * iImage._Height ));
    int x1 = std::min(iImage._Width, (int)( ( probe._RectX + probe._RectW ) * iImage._Width ));
    int y1 = std::min(iImage._Height, (int)( ( probe._RectY + probe._RectH ) * iImage._Height ));

    Vec3 sum(0.f);
    int count = 0;
    for ( int y = y0; y < y1; ++y )
    {
      for ( int x = x0; x < x1; ++x )
      {
        size_t idx = ( (size_t)y * (size_t)iImage._Width + (size_t)x ) * 4u;
        sum += Vec3(iImage._Pixels[idx], iImage._Pixels[idx + 1u], iImage._Pixels[idx + 2u]);
        ++count;
      }
    }

    if ( count > 0 )
    {
      probeResult._MeanRGB = sum / (float)count;
      probeResult._MeanLuminance = MathUtil::Luminance(probeResult._MeanRGB);
    }

    if ( probe._HasExpected )
    {
      Vec3 diff = glm::abs(probeResult._MeanRGB - probe._Expected);
      if ( ( diff.x > probe._Tolerance ) || ( diff.y > probe._Tolerance ) || ( diff.z > probe._Tolerance ) )
      {
        probeResult._Passed = false;
        probeResult._Violation = "target mismatch";
        result._Passed = false;
      }
    }

    if ( ( probeResult._MeanRGB.x < iConfig._MinChannel ) || ( probeResult._MeanRGB.y < iConfig._MinChannel ) || ( probeResult._MeanRGB.z < iConfig._MinChannel ) )
    {
      if ( probeResult._Passed )
      {
        probeResult._Passed = false;
        probeResult._Violation = "negative output";
        result._Passed = false;
      }
    }
    else if ( ( probeResult._MeanRGB.x > iConfig._MaxChannel ) || ( probeResult._MeanRGB.y > iConfig._MaxChannel ) || ( probeResult._MeanRGB.z > iConfig._MaxChannel ) )
    {
      if ( probeResult._Passed )
      {
        probeResult._Passed = false;
        probeResult._Violation = "over-unity output";
        result._Passed = false;
      }
    }

    result._ProbeResults.push_back(probeResult);
  }

  return result;
}

// ----------------------------------------------------------------------------
// WriteWhiteFurnaceMetrics
// ----------------------------------------------------------------------------
bool WriteWhiteFurnaceMetrics( const std::filesystem::path & iPath, const WhiteFurnaceResult & iResult )
{
  std::error_code error;
  std::filesystem::create_directories(iPath.parent_path(), error);
  std::ofstream output(iPath);
  if ( !output )
    return false;

  output << "Global channel range: [" << iResult._GlobalMinChannel << ", " << iResult._GlobalMaxChannel << "]\n";
  output << "Global bounds: " << ( iResult._GlobalBoundsPassed ? "PASS" : "FAIL" ) << "\n\n";

  for ( const WhiteFurnaceProbeResult & probe : iResult._ProbeResults )
  {
    output << "Probe: " << probe._Name << "\n";
    output << "  Mean RGB: (" << probe._MeanRGB.x << ", " << probe._MeanRGB.y << ", " << probe._MeanRGB.z << ")\n";
    output << "  Mean luminance: " << probe._MeanLuminance << "\n";
    output << "  Status: " << ( probe._Passed ? "PASS" : "FAIL" );
    if ( !probe._Violation.empty() )
      output << " - " << probe._Violation;
    output << "\n\n";
  }

  output << "Overall: " << ( iResult._Passed ? "PASS" : "FAIL" ) << "\n";
  return output.good();
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
  if ( !RunUnitTest("camera_projection", []() {
    Camera camera(Vec3(0.f, 0.f, -1.f), Vec3(0.f), 80.f);
    camera.SetZNearFar(1.f, 101.f);
    camera.SetProjection(CameraProjection::Orthographic);
    camera.SetOrthographicHeight(10.f);

    Mat4x4 projection(1.f);
    float top = 0.f;
    float right = 0.f;
    camera.ComputeProjMatrix(2.f, projection, &top, &right);
    const float epsilon = 0.00001f;
    if ( ( fabs(top - 5.f) > epsilon ) || ( fabs(right - 10.f) > epsilon )
      || ( fabs(projection[0][0] - .1f) > epsilon ) || ( fabs(projection[1][1] - .2f) > epsilon )
      || ( fabs(projection[2][2] + .02f) > epsilon ) || ( fabs(projection[3][2] + 1.02f) > epsilon ) )
      return false;

    camera.SetProjection(CameraProjection::Perspective);
    camera.ComputeProjMatrix(2.f, projection);
    return ( fabs(projection[2][3] + 1.f) <= epsilon ) && ( fabs(projection[3][3]) <= epsilon );
  }) )
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
  if ( !RunUnitTest("scene_camera_projection", [&iArtifactsDir, iQuiet]() {
    std::error_code errorCode;
    std::filesystem::create_directories(iArtifactsDir, errorCode);
    const std::filesystem::path scenePath = iArtifactsDir / "orthographic_camera.scene";
    std::ofstream file(scenePath);
    file << "camera\n{\n  position 0 0 -5\n  lookat 0 0 0\n  type orthographic\n  orthographicHeight 12\n  near 0.25\n  far 75\n}\n";
    file.close();

    Scene scene;
    RenderSettings settings;
    bool loaded = false;
    {
      ScopedOutputSilencer outputSilencer(iQuiet);
      loaded = Loader::LoadScene(scenePath.string(), scene, settings);
    }
    float nearPlane = 0.f;
    float farPlane = 0.f;
    scene.GetCamera().GetZNearFar(nearPlane, farPlane);
    return loaded && scene.GetCamera().IsOrthographic()
      && ( fabs(scene.GetCamera().GetOrthographicHeight() - 12.f) < 0.00001f )
      && ( fabs(nearPlane - .25f) < 0.00001f ) && ( fabs(farPlane - 75.f) < 0.00001f );
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
        "settings": { "specular_ibl": true, "ssr": false, "ssr_step_size": 0.01,
          "refraction": true, "refraction_max_steps": 64,
          "refraction_step_size": 0.12, "refraction_pixel_stride": 1.5, "refraction_start_bias": 2.0,
          "refraction_max_distance": 24.0, "refraction_thickness": 0.3, "refraction_edge_fade": 0.1 }
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
      && ( std::abs(parsedCases[0]._SSRPixelStride - 0.25f) < 1e-6f )
      && parsedCases[0]._Refraction && ( 64 == parsedCases[0]._RefractionMaxSteps )
      && ( std::abs(parsedCases[0]._RefractionPixelStride - 1.5f) < 1e-6f )
      && ( std::abs(parsedCases[0]._RefractionStartBias - 2.f) < 1e-6f )
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

  const std::string validFurnaceManifest = R"({
    "version": 1,
    "profiles": {
      "test": {
        "backend": "deferred",
        "resolution": [64, 32],
        "frames": 1,
        "thresholds": { "mean_absolute_error": 0.0, "max_absolute_error": 0.0, "pixel_error": 0.0, "mismatch_ratio": 0.0 }
      }
    },
    "tests": [
      {
        "name": "furnace_test",
        "profile": "test",
        "scene": "test.scene",
        "material": {
          "name": "furnace_probe",
          "albedo": [0.2, 0.2, 0.2],
          "roughness": 0.25,
          "metallic": 1.0,
          "ior": 1.5
        },
        "white_furnace": {
          "max_channel": 1.05,
          "min_channel": -0.001,
          "probes": {
            "center": { "rect": [0.25, 0.25, 0.5, 0.5], "expected": [0.5, 0.5, 0.5], "tolerance": 0.1 },
            "energy": { "rect": [0.1, 0.1, 0.1, 0.1] }
          }
        }
      }
    ]
  })";
  if ( !RunUnitTest("white_furnace_manifest_valid", [&validFurnaceManifest, &parsedCases, &parseError]() {
    if ( !ParseRenderTestCases(validFurnaceManifest, parsedCases, parseError) || ( 1u != parsedCases.size() )
      || !parsedCases[0]._Material._Enabled || ( "furnace_probe" != parsedCases[0]._Material._Name )
      || !parsedCases[0]._Material._HasAlbedo || ( std::abs(parsedCases[0]._Material._Albedo.x - 0.2f) >= 1e-6f )
      || !parsedCases[0]._Material._HasRoughness || ( std::abs(parsedCases[0]._Material._Roughness - 0.25f) >= 1e-6f )
      || !parsedCases[0]._Material._HasMetallic || ( std::abs(parsedCases[0]._Material._Metallic - 1.f) >= 1e-6f )
      || !parsedCases[0]._Material._HasIOR || ( std::abs(parsedCases[0]._Material._IOR - 1.5f) >= 1e-6f )
      || !parsedCases[0]._WhiteFurnace._Enabled || ( 2u != parsedCases[0]._WhiteFurnace._Probes.size() )
      || ( std::abs(parsedCases[0]._WhiteFurnace._MaxChannel - 1.05f) >= 1e-6f )
      || !parsedCases[0]._WhiteFurnace._Probes[0]._HasExpected
      || parsedCases[0]._WhiteFurnace._Probes[1]._HasExpected )
    {
      std::cerr << "Unit test failed: valid white-furnace manifest. " << parseError << std::endl;
      return false;
    }

    Scene scene;
    Material mat;
    scene.AddMaterial(mat, "furnace_probe");
    if ( !parsedCases[0].ApplyScene(scene) )
      return false;
    const Material & applied = scene.GetMaterials()[0];
    return ( std::abs(applied._Albedo.x - 0.2f) < 1e-6f )
      && ( std::abs(applied._Roughness - 0.25f) < 1e-6f )
      && ( std::abs(applied._Metallic - 1.f) < 1e-6f )
      && ( std::abs(applied._IOR - 1.5f) < 1e-6f );
  }) )
    return 1;

  const std::string invalidFurnaceManifests[] =
  {
    // Out of bounds rect
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [64, 32], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "white_furnace": { "probes": { "bad": { "rect": [0.9, 0.9, 0.2, 0.2] } } } }] })",
    // Negative tolerance
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [64, 32], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "white_furnace": { "probes": { "bad": { "rect": [0.1, 0.1, 0.1, 0.1], "expected": [0.5, 0.5, 0.5], "tolerance": -0.1 } } } }] })",
    // max_channel <= min_channel
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [64, 32], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "white_furnace": { "max_channel": 0.5, "min_channel": 1.0, "probes": {} } }] })",
    // Expected without tolerance
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [64, 32], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "white_furnace": { "probes": { "bad": { "rect": [0.1, 0.1, 0.1, 0.1], "expected": [0.5, 0.5, 0.5] } } } }] })",
    // Invalid material roughness > 1
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [64, 32], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "material": { "roughness": 1.5 } }] })",
    // Invalid material IOR < 1
    R"({ "version": 1, "profiles": { "test": { "backend": "deferred", "resolution": [64, 32], "frames": 1, "thresholds": { "mean_absolute_error": 0, "max_absolute_error": 0, "pixel_error": 0, "mismatch_ratio": 0 } } }, "tests": [{ "name": "test", "profile": "test", "scene": "test.scene", "material": { "ior": 0.8 } }] })"
  };
  if ( !RunUnitTest("white_furnace_manifest_validation", [&invalidFurnaceManifests, &parsedCases, &parseError]() {
    for ( const std::string & m : invalidFurnaceManifests )
    {
      if ( ParseRenderTestCases(m, parsedCases, parseError) )
      {
        std::cerr << "Unit test failed: invalid white-furnace manifest accepted." << std::endl;
        return false;
      }
    }
    return true;
  }) )
    return 1;

  if ( !RunUnitTest("white_furnace_evaluator", []() {
    RenderImage testImage;
    testImage._Width = 4;
    testImage._Height = 4;
    testImage._Pixels.assign(4 * 4 * 4, 0.f);
    for ( size_t i = 0; i < 4 * 4; ++i )
    {
      testImage._Pixels[i * 4 + 0] = 0.5f;
      testImage._Pixels[i * 4 + 1] = 0.5f;
      testImage._Pixels[i * 4 + 2] = 0.5f;
      testImage._Pixels[i * 4 + 3] = 1.f;
    }

    WhiteFurnaceConfig config;
    config._Enabled = true;
    config._MaxChannel = 1.05f;
    config._MinChannel = -0.001f;
    WhiteFurnaceProbe probe;
    probe._Name = "center";
    probe._RectX = 0.25f;
    probe._RectY = 0.25f;
    probe._RectW = 0.5f;
    probe._RectH = 0.5f;
    probe._HasExpected = true;
    probe._Expected = Vec3(0.5f);
    probe._Tolerance = 0.01f;
    config._Probes.push_back(probe);

    WhiteFurnaceResult result = EvaluateWhiteFurnace(testImage, config);
    if ( !result._Passed || !result._GlobalBoundsPassed || ( 1u != result._ProbeResults.size() ) || !result._ProbeResults[0]._Passed )
    {
      std::cerr << "Unit test failed: white-furnace evaluator (pass case)." << std::endl;
      return false;
    }

    // Mismatch case: change expected to something different
    config._Probes[0]._Expected = Vec3(0.9f);
    result = EvaluateWhiteFurnace(testImage, config);
    if ( result._Passed || result._ProbeResults[0]._Passed )
    {
      std::cerr << "Unit test failed: white-furnace evaluator (mismatch case)." << std::endl;
      return false;
    }

    // Over-unity case: image has pixels > max_channel
    for ( size_t i = 0; i < 4 * 4; ++i )
    {
      testImage._Pixels[i * 4 + 0] = 1.2f;
      testImage._Pixels[i * 4 + 1] = 1.2f;
      testImage._Pixels[i * 4 + 2] = 1.2f;
    }
    config._Probes[0]._Expected = Vec3(1.2f);
    config._Probes[0]._Tolerance = 0.01f;
    result = EvaluateWhiteFurnace(testImage, config);
    if ( result._Passed || result._GlobalBoundsPassed )
    {
      std::cerr << "Unit test failed: white-furnace evaluator (over-unity case)." << std::endl;
      return false;
    }

    // Negative case: image has negative pixels
    for ( size_t i = 0; i < 4 * 4; ++i )
    {
      testImage._Pixels[i * 4 + 0] = -0.1f;
      testImage._Pixels[i * 4 + 1] = -0.1f;
      testImage._Pixels[i * 4 + 2] = -0.1f;
    }
    config._Probes[0]._Expected = Vec3(0.f);
    config._Probes[0]._Tolerance = 0.2f;
    result = EvaluateWhiteFurnace(testImage, config);
    if ( result._Passed || result._GlobalBoundsPassed )
    {
      std::cerr << "Unit test failed: white-furnace evaluator (negative case)." << std::endl;
      return false;
    }

    return true;
  }) )
    return 1;

  std::cout << "Unit summary: " << passed << " passed, 0 failed." << std::endl;
  return 0;
}

}

}
