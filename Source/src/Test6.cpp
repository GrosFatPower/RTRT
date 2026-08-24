#pragma warning(disable : 4100) // unreferenced formal parameter

#include "Test6.h"

#include "DeferredRenderer.h"
#include "DroppedFileUtils.h"
#include "FpsGameMap.h"
#include "PathTracer.h"
#include "PathUtils.h"
#include "SoftwareRasterizer.h"

#include "imgui.h"
#include "ImGuizmo.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <thread>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static constexpr int g_ScreenWidth  = 1280;
static constexpr int g_ScreenHeight = 720;

const char * Test6::GetTestHeader() { return "Basic FPS"; }

// ----------------------------------------------------------------------------
// ApplyMapSettings
// ----------------------------------------------------------------------------
static void ApplyMapSettings( const FpsGameMap & iMap, FpsGameSettings & ioSettings )
{
  if ( iMap._MaxProjectiles > 0 )
    ioSettings._MaxProjectiles = iMap._MaxProjectiles;
  if ( iMap._MaxProjectileAmmo >= 0 )
    ioSettings._MaxProjectileAmmo = iMap._MaxProjectileAmmo;
  if ( iMap._ProjectileAmmoRefillTime > 0.f )
    ioSettings._ProjectileAmmoRefillTime = iMap._ProjectileAmmoRefillTime;

  ioSettings._ShowViewWeapon = iMap._Weapon._Visible;
  ioSettings._ViewWeaponOffset = iMap._Weapon._Offset;
  ioSettings._ViewWeaponRotation = iMap._Weapon._Rotation;
  ioSettings._ViewWeaponScale = iMap._Weapon._Scale;
}

// ----------------------------------------------------------------------------
// ApplyMapRenderSettings
// ----------------------------------------------------------------------------
static void ApplyMapRenderSettings( const FpsGameMap & iMap, FpsGameSettings & ioGameSettings, RenderSettings & ioRenderSettings )
{
  if ( !iMap._RenderSettings._HasRenderSettings )
    return;

  const FpsMapRenderSettings & settings = iMap._RenderSettings;
  ioGameSettings._RendererMode = settings._RendererMode;
  ioGameSettings._CameraZNear = std::max(0.001f, settings._CameraZNear);
  ioGameSettings._CameraZFar = std::max(ioGameSettings._CameraZNear + 0.001f, settings._CameraZFar);
  ioGameSettings._CameraFOV = MathUtil::Clamp(settings._CameraFOV, 30.f, 140.f);

  ioRenderSettings._RenderScale = MathUtil::Clamp(settings._RenderScale, 25, 150);
  ioRenderSettings._ShowLights = settings._ShowLights;
  ioRenderSettings._ToneMapping = settings._ToneMapping;
  ioRenderSettings._Gamma = std::max(0.001f, settings._Gamma);
  ioRenderSettings._Exposure = std::max(0.f, settings._Exposure);

  ioRenderSettings._ShadowMapping = settings._ShadowMapping;
  ioRenderSettings._ShadowMapResolution = std::max(256, settings._ShadowMapResolution);
  ioRenderSettings._ShadowBias = std::max(0.f, settings._ShadowBias);
  ioRenderSettings._MaxShadowCastingLights = std::max(1, settings._MaxShadowCastingLights);

  ioRenderSettings._SSAO = settings._SSAO;
  ioRenderSettings._SSAOBlur = settings._SSAOBlur;
  ioRenderSettings._SSAORadius = std::max(0.001f, settings._SSAORadius);
  ioRenderSettings._SSAOBias = std::max(0.f, settings._SSAOBias);
  ioRenderSettings._SSAOIntensity = std::max(0.f, settings._SSAOIntensity);
  ioRenderSettings._SSAOKernelSize = MathUtil::Clamp(settings._SSAOKernelSize, 4, 32);

  ioRenderSettings._SSR = settings._SSR;
  ioRenderSettings._SSRIntensity = std::max(0.f, settings._SSRIntensity);
  ioRenderSettings._SSRMaxRoughness = MathUtil::Clamp(settings._SSRMaxRoughness, 0.f, 1.f);
  ioRenderSettings._SSRMaxSteps = std::max(1, settings._SSRMaxSteps);
  ioRenderSettings._SSRPixelStride = MathUtil::Clamp(settings._SSRPixelStride, 0.25f, 4.f);
  ioRenderSettings._SSRStartBias = MathUtil::Clamp(settings._SSRStartBias, 0.25f, 8.f);
  ioRenderSettings._SSRMaxDistance = std::max(0.001f, settings._SSRMaxDistance);
  ioRenderSettings._SSRThickness = std::max(0.001f, settings._SSRThickness);
  ioRenderSettings._SSRFade = MathUtil::Clamp(settings._SSRFade, 0.f, 1.f);
  ioRenderSettings._Transparency = settings._Transparency;
  ioRenderSettings._Refraction = settings._Refraction;
  ioRenderSettings._RefractionMaxSteps = MathUtil::Clamp(settings._RefractionMaxSteps, 4, 128);
  ioRenderSettings._RefractionPixelStride = MathUtil::Clamp(settings._RefractionPixelStride, 0.25f, 4.f);
  ioRenderSettings._RefractionStartBias = MathUtil::Clamp(settings._RefractionStartBias, 0.25f, 8.f);
  ioRenderSettings._RefractionMaxDistance = std::max(0.001f, settings._RefractionMaxDistance);
  ioRenderSettings._RefractionThickness = std::max(0.001f, settings._RefractionThickness);
  ioRenderSettings._RefractionEdgeFade = MathUtil::Clamp(settings._RefractionEdgeFade, 0.001f, 1.f);

  ioRenderSettings._PBRDirectLighting = settings._PBRDirectLighting;
  ioRenderSettings._DirectLightIntensity = std::max(0.f, settings._DirectLightIntensity);
  ioRenderSettings._SpecularIBLMaxRoughness = MathUtil::Clamp(settings._SpecularIBLMaxRoughness, 0.f, 1.f);
  ioRenderSettings._ShadingType = settings._ShadingType;

  ioRenderSettings._Bounces = std::max(1, settings._Bounces);
  ioRenderSettings._NbSamplesPerPixel = std::max(1, settings._NbSamplesPerPixel);
  ioRenderSettings._Denoise = settings._Denoise;
}

// ----------------------------------------------------------------------------
// CaptureMapRenderSettings
// ----------------------------------------------------------------------------
static void CaptureMapRenderSettings( FpsGameMap & ioMap, const FpsGameSettings & iGameSettings, const RenderSettings & iRenderSettings )
{
  FpsMapRenderSettings & settings = ioMap._RenderSettings;
  settings._HasRenderSettings = true;
  settings._RendererMode = iGameSettings._RendererMode;
  settings._CameraZNear = iGameSettings._CameraZNear;
  settings._CameraZFar = iGameSettings._CameraZFar;
  settings._CameraFOV = iGameSettings._CameraFOV;
  settings._RenderScale = iRenderSettings._RenderScale;
  settings._ShowLights = iRenderSettings._ShowLights;
  settings._ToneMapping = iRenderSettings._ToneMapping;
  settings._Gamma = iRenderSettings._Gamma;
  settings._Exposure = iRenderSettings._Exposure;
  settings._ShadowMapping = iRenderSettings._ShadowMapping;
  settings._ShadowMapResolution = iRenderSettings._ShadowMapResolution;
  settings._ShadowBias = iRenderSettings._ShadowBias;
  settings._MaxShadowCastingLights = iRenderSettings._MaxShadowCastingLights;
  settings._SSAO = iRenderSettings._SSAO;
  settings._SSAOBlur = iRenderSettings._SSAOBlur;
  settings._SSAORadius = iRenderSettings._SSAORadius;
  settings._SSAOBias = iRenderSettings._SSAOBias;
  settings._SSAOIntensity = iRenderSettings._SSAOIntensity;
  settings._SSAOKernelSize = iRenderSettings._SSAOKernelSize;
  settings._SSR = iRenderSettings._SSR;
  settings._SSRIntensity = iRenderSettings._SSRIntensity;
  settings._SSRMaxRoughness = iRenderSettings._SSRMaxRoughness;
  settings._SSRMaxSteps = iRenderSettings._SSRMaxSteps;
  settings._SSRPixelStride = iRenderSettings._SSRPixelStride;
  settings._SSRStartBias = iRenderSettings._SSRStartBias;
  settings._SSRMaxDistance = iRenderSettings._SSRMaxDistance;
  settings._SSRThickness = iRenderSettings._SSRThickness;
  settings._SSRFade = iRenderSettings._SSRFade;
  settings._Transparency = iRenderSettings._Transparency;
  settings._Refraction = iRenderSettings._Refraction;
  settings._RefractionMaxSteps = iRenderSettings._RefractionMaxSteps;
  settings._RefractionPixelStride = iRenderSettings._RefractionPixelStride;
  settings._RefractionStartBias = iRenderSettings._RefractionStartBias;
  settings._RefractionMaxDistance = iRenderSettings._RefractionMaxDistance;
  settings._RefractionThickness = iRenderSettings._RefractionThickness;
  settings._RefractionEdgeFade = iRenderSettings._RefractionEdgeFade;
  settings._PBRDirectLighting = iRenderSettings._PBRDirectLighting;
  settings._DirectLightIntensity = iRenderSettings._DirectLightIntensity;
  settings._SpecularIBLMaxRoughness = iRenderSettings._SpecularIBLMaxRoughness;
  settings._ShadingType = iRenderSettings._ShadingType;
  settings._Bounces = iRenderSettings._Bounces;
  settings._NbSamplesPerPixel = iRenderSettings._NbSamplesPerPixel;
  settings._Denoise = iRenderSettings._Denoise;
}

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void Test6::KeyCallback( GLFWwindow * iWindow, const int iKey, const int iScancode, const int iAction, const int iMods )
{
  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  if ( ( GLFW_PRESS == iAction ) || ( GLFW_RELEASE == iAction ) )
    this_ -> _KeyInput.AddEvent(iKey, iAction, iMods);
}

// ----------------------------------------------------------------------------
// MouseButtonCallback
// ----------------------------------------------------------------------------
void Test6::MouseButtonCallback( GLFWwindow * iWindow, const int iButton, const int iAction, const int iMods )
{
  if ( ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
    return;

  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  double mouseX = 0., mouseY = 0.;
  glfwGetCursorPos(iWindow, &mouseX, &mouseY);

  if ( ( GLFW_PRESS == iAction ) || ( GLFW_RELEASE == iAction ) )
    this_ -> _MouseInput.AddButtonEvent(iButton, iAction, mouseX, mouseY);
}

// ----------------------------------------------------------------------------
// MouseScrollCallback
// ----------------------------------------------------------------------------
void Test6::MouseScrollCallback( GLFWwindow * iWindow, const double iOffsetX, const double iOffsetY )
{
  if ( ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
    return;

  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  this_ -> _MouseInput.AddScrollEvent(iOffsetX, iOffsetY);
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void Test6::FramebufferSizeCallback( GLFWwindow * iWindow, const int iWidth, const int iHeight )
{
  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  if ( !iWidth || !iHeight )
    return;

  this_ -> SyncFramebufferResolution(true);
}

// ----------------------------------------------------------------------------
// DropCallback
// ----------------------------------------------------------------------------
void Test6::DropCallback( GLFWwindow * iWindow, int iCount, const char ** iPaths )
{
  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  this_ -> HandleDroppedFiles(iCount, iPaths);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Test6::Test6( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: BaseTest(iMainWindow, iScreenWidth, iScreenHeight)
{
  InitializeCpuTimings();

  _Settings._WindowResolution.x = g_ScreenWidth;
  _Settings._WindowResolution.y = g_ScreenHeight;
  _Settings._RenderResolution.x = _Settings._WindowResolution.x;
  _Settings._RenderResolution.y = _Settings._WindowResolution.y;
  _Settings._BackgroundColor = Vec3(0.015f, 0.018f, 0.022f);
  _Settings._EnableSkybox = true;
  _Settings._EnableBackGround = true;
  _Settings._RenderScale = 100;
  _Settings._SSAO = true;
  _Settings._SSR = true;
  _Settings._ShadowMapping = true;
  _Settings._ShadowBias = 0.002f;
  _Settings._MaxShadowCastingLights = 4;
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test6::~Test6()
{
  SetMouseCaptured(false);
}

// ----------------------------------------------------------------------------
// EnableAutomaticSoftwareBenchmark
// ----------------------------------------------------------------------------
void Test6::EnableAutomaticSoftwareBenchmark( const std::string & iLabel,
                                              bool iSIMD,
                                              unsigned int iTileSize,
                                              const std::string & iOptimization,
                                              const std::string & iPose )
{
  _AutomaticBenchmark = true;
  _AutomaticBenchmarkSIMD = iSIMD;
  _AutomaticBenchmarkTileSize = std::max(8u, iTileSize);
  _AutomaticBenchmarkOptimization = iOptimization;
  _GameSettings._RendererMode = FpsRendererMode::Software;
  _Benchmark.GetLabel() = iLabel;
  _Benchmark.GetWarmupFrames() = 10;
  _Benchmark.GetSampleFrames() = 120;
  _Benchmark.GetRepetitions() = 3;
  if ( iPose == "ground" )
    FpsGameBenchmark::SetPose(Vec3(17.065f, -0.229f, -45.187f), -147.34f, -35.f);
  else if ( iPose == "sky" )
    FpsGameBenchmark::SetPose(Vec3(17.065f, -0.229f, -45.187f), -147.34f, 45.f);
}

// ----------------------------------------------------------------------------
// SyncFramebufferResolution
// ----------------------------------------------------------------------------
void Test6::SyncFramebufferResolution( bool iNotifyRenderer )
{
  if ( !_MainWindow )
    return;

  int frameBufferWidth = 0;
  int frameBufferHeight = 0;
  glfwGetFramebufferSize(_MainWindow.get(), &frameBufferWidth, &frameBufferHeight);

  if ( frameBufferWidth <= 0 || frameBufferHeight <= 0 )
    return;

  _Settings._WindowResolution.x = frameBufferWidth;
  _Settings._WindowResolution.y = frameBufferHeight;
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * (_Settings._RenderScale * 0.01f));
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * (_Settings._RenderScale * 0.01f));

  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  if ( iNotifyRenderer && _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);
}

// ----------------------------------------------------------------------------
// SetMouseCaptured
// ----------------------------------------------------------------------------
void Test6::SetMouseCaptured( bool iCaptured )
{
  if ( !_MainWindow )
    return;

  _MouseCaptured = iCaptured;
  _HasLastMousePos = false;
  glfwSetInputMode(_MainWindow.get(), GLFW_CURSOR, _MouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

// ----------------------------------------------------------------------------
// HandleDroppedFiles
// ----------------------------------------------------------------------------
void Test6::HandleDroppedFiles( int iCount, const char ** iPaths )
{
  if ( !_Editor.IsEnabled() || !iPaths || ( iCount <= 0 ) )
    return;

  const FpsPlayer & player = _GameWorld.GetPlayer();
  const float yawRad = MathUtil::ToRadians(player._Yaw);
  const float pitchRad = MathUtil::ToRadians(player._Pitch);
  Vec3 forward(std::cos(yawRad) * std::cos(pitchRad),
               std::sin(pitchRad),
               std::sin(yawRad) * std::cos(pitchRad));
  if ( glm::length(forward) <= EPSILON )
    forward = Vec3(0.f, 0.f, 1.f);
  else
    forward = glm::normalize(forward);

  const Vec3 dropPosition = player.EyePosition(_GameSettings) + forward * 4.f;
  FpsGameEditorContext editorContext = MakeEditorContext();

  int droppedProps = 0;
  for ( int i = 0; i < iCount; ++i )
  {
    if ( !iPaths[i] || !iPaths[i][0] )
      continue;

    const std::filesystem::path filepath(DroppedFileUtils::NormalizeDroppedPath(iPaths[i]));
    if ( !DroppedFileUtils::IsDroppedPropPath(filepath) )
    {
      _Editor.SetStatus("Unsupported dropped file: " + DroppedFileUtils::DisplayName(filepath));
      continue;
    }

    if ( _Editor.AddDroppedProp(editorContext, filepath, dropPosition) )
      ++droppedProps;
  }

  if ( droppedProps > 1 )
    _Editor.SetStatus("Dropped " + std::to_string(droppedProps) + " props");
}

// ----------------------------------------------------------------------------
// ApplyRendererDefaults
// ----------------------------------------------------------------------------
void Test6::ApplyRendererDefaults()
{
  if ( FpsRendererMode::PhotoPathTracer == _GameSettings._RendererMode )
  {
    _Settings._Accumulate = true;
    _Settings._Denoise = false;
    _Settings._NbSamplesPerPixel = 1;
    _Settings._Bounces = 2;
    _Settings._LowResRatio = 1.f;
    _Settings._TiledRendering = false;
    SetMouseCaptured(false);
  }
  else if ( FpsRendererMode::Software == _GameSettings._RendererMode )
  {
    _Settings._NbThreads = std::max(1u, std::thread::hardware_concurrency());
    _Settings._ShadingType = ShadingType::PBR;
    _Settings._Sampling = SamplingMode::Bilinear;
    _Settings._TiledRendering = true;
    _Settings._Transparency = true;
  }
  else
  {
    _Settings._SSAO = true;
    _Settings._SSAOBlur = true;
    _Settings._SSR = true;
    _Settings._ShadowMapping = true;
    _Settings._SpecularIBL = false;
    _Settings._Transparency = false;
    _Settings._TiledRendering = false;
  }
}

// ----------------------------------------------------------------------------
// InitializeCpuTimings
// ----------------------------------------------------------------------------
void Test6::InitializeCpuTimings()
{
  _CpuTimings.assign(CpuTimingCount, FpsCpuTiming());
  _CpuTimings[CpuProcessInput]._Name = "Process input";
  _CpuTimings[CpuUpdateGame]._Name = "Update game";
  _CpuTimings[CpuUpdateBoids]._Name = "Update boids";
  _CpuTimings[CpuBoidsSimulation]._Name = "Boids simulation";
  _CpuTimings[CpuBoidsSceneSync]._Name = "Boids scene sync";
  _CpuTimings[CpuBoidsRendererNotify]._Name = "Boids renderer notify";
  _CpuTimings[CpuRendererUpdate]._Name = "Renderer update";
  _CpuTimings[CpuRenderToTexture]._Name = "Render to texture call";
  _CpuTimings[CpuRenderToScreen]._Name = "Render to screen call";
  _CpuTimings[CpuRenderToFile]._Name = "Render to file";
  _CpuTimings[CpuDrawUI]._Name = "Draw UI";
  _CpuTimings[CpuSwapBuffers]._Name = "Swap buffers";
  _DisplayedCpuTimings = _CpuTimings;
}

// ----------------------------------------------------------------------------
// ResetCpuTimings
// ----------------------------------------------------------------------------
void Test6::ResetCpuTimings()
{
  if ( _CpuTimings.size() != CpuTimingCount )
    InitializeCpuTimings();

  for ( FpsCpuTiming & timing : _CpuTimings )
  {
    timing._Seconds = 0.;
    timing._Enabled = false;
  }
}

// ----------------------------------------------------------------------------
// BeginCpuTiming
// ----------------------------------------------------------------------------
void Test6::BeginCpuTiming( int iTimingID )
{
  if ( ( iTimingID < 0 ) || ( iTimingID >= CpuTimingCount ) )
    return;

  if ( _CpuTimings.size() != CpuTimingCount )
    InitializeCpuTimings();

  _CpuTimings[iTimingID]._Enabled = true;
  _CpuTimingStart[iTimingID] = glfwGetTime();
}

// ----------------------------------------------------------------------------
// EndCpuTiming
// ----------------------------------------------------------------------------
void Test6::EndCpuTiming( int iTimingID )
{
  if ( ( iTimingID < 0 ) || ( iTimingID >= CpuTimingCount ) )
    return;

  if ( _CpuTimings.size() != CpuTimingCount )
    return;

  _CpuTimings[iTimingID]._Seconds += glfwGetTime() - _CpuTimingStart[iTimingID];
}

// ----------------------------------------------------------------------------
// SetCpuTimingEnabled
// ----------------------------------------------------------------------------
void Test6::SetCpuTimingEnabled( int iTimingID, bool iEnabled )
{
  if ( ( iTimingID < 0 ) || ( iTimingID >= CpuTimingCount ) )
    return;

  if ( _CpuTimings.size() != CpuTimingCount )
    InitializeCpuTimings();

  _CpuTimings[iTimingID]._Enabled = iEnabled;
  if ( !iEnabled )
    _CpuTimings[iTimingID]._Seconds = 0.;
}

// ----------------------------------------------------------------------------
// ApplyBenchmarkPose
// ----------------------------------------------------------------------------
void Test6::ApplyBenchmarkPose()
{
  FpsPlayer & player = _GameWorld.GetPlayer();
  player._Position = FpsGameBenchmark::GetPosition();
  player._Velocity = Vec3(0.f);
  player._Yaw = FpsGameBenchmark::GetYaw();
  player._Pitch = FpsGameBenchmark::GetPitch();
  player._Grounded = false;

  _GameWorld.ClearProjectiles();
  if ( _Scene )
  {
    _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings);
    _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings);
  }

  if ( _Renderer )
  {
    _Renderer -> Notify(DirtyState::SceneCamera);
    _Renderer -> Notify(DirtyState::SceneInstances);
  }
}

// ----------------------------------------------------------------------------
// StartBenchmark
// ----------------------------------------------------------------------------
void Test6::StartBenchmark()
{
  _Benchmark.Start(_GameSettings._RendererMode);
  SetMouseCaptured(false);
  _GameSettings._FreeLook = true;
  ApplyBenchmarkPose();

  std::cout << "Test6 " << FpsGameBenchmark::GetRendererName(_Benchmark.GetRendererMode()) << " benchmark started." << std::endl;
}

// ----------------------------------------------------------------------------
// UpdateBenchmark
// ----------------------------------------------------------------------------
void Test6::UpdateBenchmark()
{
  if ( !_Renderer )
    return;

  const bool wasRunning = _Benchmark.IsRunning();
  _Benchmark.Update(_CpuTimings, *_Renderer, _GameSettings._RendererMode, !_ReloadRenderer && !_ReloadScene);
  if ( wasRunning && !_Benchmark.IsRunning() && _Benchmark.IsCompleted() )
    FinishBenchmark();
  else if ( wasRunning && !_Benchmark.IsRunning() )
    std::cout << "Test6 benchmark " << _Benchmark.GetStatus() << "." << std::endl;
}

// ----------------------------------------------------------------------------
// FinishBenchmark
// ----------------------------------------------------------------------------
void Test6::FinishBenchmark()
{
  FpsGameMap sceneSnapshot = _Map;
  sceneSnapshot._Player._Position = FpsGameBenchmark::GetPosition();
  sceneSnapshot._Player._Yaw = FpsGameBenchmark::GetYaw();
  sceneSnapshot._Player._Pitch = FpsGameBenchmark::GetPitch();
  CaptureMapRenderSettings(sceneSnapshot, _GameSettings, _Settings);

  FpsGameBenchmarkSaveContext saveContext;
  saveContext._MapPath = _MapPath;
  saveContext._SceneSnapshot = sceneSnapshot;
  saveContext._Player = _GameWorld.GetPlayer();
  saveContext._Scene = _Scene.get();
  saveContext._Settings = &_Settings;
  saveContext._Renderer = _Renderer.get();

  if ( _Benchmark.SaveResult(saveContext) )
    std::cout << "Test6 benchmark saved: " << _Benchmark.GetResultPath() << std::endl;
  else
    std::cout << "Test6 benchmark completed but result saving failed." << std::endl;
}


// ----------------------------------------------------------------------------
// InitializeUI
// ----------------------------------------------------------------------------
int Test6::InitializeUI()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();
  io.Fonts -> AddFontDefault();

  const char * glslVersion = "#version 410";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  return 0;
}

// ----------------------------------------------------------------------------
// MakeEditorContext
// ----------------------------------------------------------------------------
FpsGameEditorContext Test6::MakeEditorContext()
{
  return FpsGameEditorContext(_MainWindow.get(), _Scene.get(), _Renderer.get(), _Settings, _KeyInput, _GameSettings, _GameWorld, _SceneBinding, _Map, _MapPath, _MapLoaded, _ReloadScene, _ReloadBoids,
                              _FrameRate, _FrameTime, _DeltaTime, _NbRenderedFrames,
                              _DebugMode, _DeferredDebugView, _DeferredShowWires, _SoftwareDebugView, _SoftwareShowWires,
                              _DisplayedCpuTimings);
}

// ----------------------------------------------------------------------------
// MakeHudContext
// ----------------------------------------------------------------------------
FpsGameHudContext Test6::MakeHudContext() const
{
  return FpsGameHudContext(_GameSettings, _GameWorld);
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test6::InitializeScene()
{
  std::unique_ptr<Scene> newScene(new Scene);
  if ( !newScene )
    return 1;

  const bool preserveEditorPose = _Editor.IsEnabled();
  FpsPlayer editorPlayer;
  if ( preserveEditorPose )
    editorPlayer = _GameWorld.GetPlayer();

  if ( _MapPath.empty() )
    _MapPath = PathUtils::GetAssetPath("FPSMaps/ccity.fpsmap");

  if ( !_MapLoaded )
  {
    _Map = FpsGameMap();
    _MapLoaded = FpsGameMapLoader::Load(_MapPath, _Map);
  }

  if ( _MapLoaded )
  {
    FpsGameMapLoader::SeedDefaultMaterials(_Map);
    ApplyMapSettings(_Map, _GameSettings);
    ApplyMapRenderSettings(_Map, _GameSettings, _Settings);
    if ( _Editor.IsEnabled() )
    {
      _GameSettings._ShowViewWeapon = false;
      _GameSettings._FreeLook = true;
    }
    if ( 0 != _GameWorld.Initialize(_GameSettings, _Map) )
      return 1;
  }
  else
  {
    std::cout << "Test6 : Failed to load FPS map, using fallback arena" << std::endl;
    if ( 0 != _GameWorld.Initialize(_GameSettings) )
      return 1;
  }

  if ( preserveEditorPose )
  {
    FpsPlayer & player = _GameWorld.GetPlayer();
    player._Position = editorPlayer._Position;
    player._Velocity = Vec3(0.f);
    player._Yaw = editorPlayer._Yaw;
    player._Pitch = editorPlayer._Pitch;
    player._Grounded = false;
  }

  if ( _MapLoaded )
  {
    if ( 0 != _SceneBinding.Attach(*newScene, _GameWorld, _GameSettings, _Map) )
      return 1;
  }
  else if ( 0 != _SceneBinding.Attach(*newScene, _GameWorld, _GameSettings) )
    return 1;

  const std::string envMap = _MapLoaded ? _Map._Environment : "syferfontein_18d_clear_1k.hdr";
  const std::string envPath = _MapLoaded ? PathUtils::GetAssetPath(envMap) : PathUtils::GetEnvMapPath(envMap);
  if ( !newScene -> LoadEnvMap(envPath) )
    std::cout << "Test6 : Failed to load default environment map" << std::endl;

  _Scene = std::move(newScene);
  if ( 0 != RefreshPropCollisionColliders() )
    return 1;

  if ( 0 != InitializeMapBoids(true) )
    return 1;

  FpsGameEditorContext editorContext = MakeEditorContext();
  _Editor.ApplyObjectVisibility(editorContext);
  _Editor.SetPathBuffers(_MapPath);
  _Editor.RefreshPropAssets();
  return 0;
}

// ----------------------------------------------------------------------------
// RefreshPropCollisionColliders
// ----------------------------------------------------------------------------
int Test6::RefreshPropCollisionColliders()
{
  if ( !_Scene )
  {
    _GameWorld.ClearPropCollisionColliders();
    return 0;
  }

  std::vector<FpsCollisionObb> colliders;
  if ( 0 != _SceneBinding.BuildPropCollisionColliders(*_Scene, _Map, colliders) )
    return 1;

  _GameWorld.SetPropCollisionColliders(colliders);
  return 0;
}

// ----------------------------------------------------------------------------
// InitializeMapBoids
// ----------------------------------------------------------------------------
int Test6::InitializeMapBoids( bool iResetSimulation )
{
  _BoidSettings.clear();
  _BoidSimulations.clear();
  _BoidBindings.clear();

  if ( !_Scene || !_MapLoaded )
    return 0;

  for ( const FpsMapBoids & mapBoids : _Map._Boids )
  {
    _BoidSettings.push_back(mapBoids._Settings);
    _BoidSimulations.push_back(BoidSimulation());
    _BoidBindings.push_back(BoidSceneBinding());

    BoidSettings & settings = _BoidSettings.back();
    settings._Paused = false;
    if ( !mapBoids._Visible )
      settings._Count = 0;

    BoidSimulation & simulation = _BoidSimulations.back();
    BoidSceneBinding & binding = _BoidBindings.back();

    if ( iResetSimulation )
      simulation.Reset(settings);
    else
      simulation.Resize(settings);

    if ( 0 != binding.Attach(*_Scene, settings) )
      return 1;

    if ( 0 != binding.SyncTransforms(*_Scene, simulation, settings) )
      return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RefreshMapBoids
// ----------------------------------------------------------------------------
int Test6::RefreshMapBoids()
{
  auto hideCurrentMapBoids = [this]()
  {
    if ( !_Scene )
      return;
    for ( int i = static_cast<int>(_BoidBindings.size()) - 1; i >= 0; --i )
      _BoidBindings[i].SetInstancesVisible(*_Scene, false);
  };

  if ( !_Scene || !_MapLoaded )
  {
    hideCurrentMapBoids();
    _BoidSettings.clear();
    _BoidSimulations.clear();
    _BoidBindings.clear();
    return 0;
  }

  if ( static_cast<int>(_Map._Boids.size()) != static_cast<int>(_BoidSettings.size()) )
  {
    hideCurrentMapBoids();
    if ( 0 != InitializeMapBoids(true) )
      return 1;

    if ( _Renderer )
    {
      _Renderer -> Notify(DirtyState::Textures);
      _Renderer -> Notify(DirtyState::SceneMaterials);
      _Renderer -> Notify(DirtyState::SceneInstances);
    }
    return 0;
  }

  int runtimeIndex = 0;
  bool materialsDirty = false;
  for ( const FpsMapBoids & mapBoids : _Map._Boids )
  {
    BoidSettings settings = mapBoids._Settings;
    settings._Paused = false;
    if ( !mapBoids._Visible )
      settings._Count = 0;

    if ( runtimeIndex >= static_cast<int>(_BoidSettings.size())
      || runtimeIndex >= static_cast<int>(_BoidSimulations.size())
      || runtimeIndex >= static_cast<int>(_BoidBindings.size()) )
      return 1;

    const bool resetSimulation = ( settings._Seed != _BoidSettings[runtimeIndex]._Seed );
    const bool materialDirty = glm::length(settings._Color - _BoidSettings[runtimeIndex]._Color) > 0.0001f;
    _BoidSettings[runtimeIndex] = settings;

    if ( resetSimulation )
    {
      if ( 0 != _BoidSimulations[runtimeIndex].Reset(settings) )
        return 1;
    }
    else if ( 0 != _BoidSimulations[runtimeIndex].Resize(settings) )
      return 1;

    if ( 0 != _BoidBindings[runtimeIndex].Attach(*_Scene, settings) )
      return 1;
    if ( 0 != _BoidBindings[runtimeIndex].SyncTransforms(*_Scene, _BoidSimulations[runtimeIndex], settings) )
      return 1;

    materialsDirty = materialsDirty || materialDirty;
    ++runtimeIndex;
  }

  if ( _Renderer )
  {
    if ( materialsDirty )
      _Renderer -> Notify(DirtyState::SceneMaterials);
    _Renderer -> Notify(DirtyState::SceneInstances);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeRenderer
// ----------------------------------------------------------------------------
int Test6::InitializeRenderer()
{
  if ( !_Scene )
    return 1;

  ApplyRendererDefaults();
  ApplyMapRenderSettings(_Map, _GameSettings, _Settings);

  if ( _AutomaticBenchmark )
  {
    _GameSettings._RendererMode = FpsRendererMode::Software;
    _Settings._NbThreads = 10;
    _Settings._ShadingType = ShadingType::PBR;
    _Settings._Sampling = SamplingMode::Bilinear;
    _Settings._WBuffer = true;
    _Settings._TiledRendering = true;
  }

  Renderer * newRenderer = nullptr;
  if ( FpsRendererMode::PhotoPathTracer == _GameSettings._RendererMode )
    newRenderer = new PathTracer(*_Scene, _Settings);
  else if ( FpsRendererMode::Software == _GameSettings._RendererMode )
    newRenderer = new SoftwareRasterizer(*_Scene, _Settings);
  else
    newRenderer = new DeferredRenderer(*_Scene, _Settings);

  if ( !newRenderer )
    return 1;

  _Renderer.reset(newRenderer);
  _Renderer -> Initialize();

  if ( _AutomaticBenchmark )
  {
    SoftwareRasterizer * software = _Renderer -> AsSoftwareRasterizer();
    if ( !software )
      return 1;
    software -> SetEnableSIMD(_AutomaticBenchmarkSIMD);
    software -> SetTileSize(_AutomaticBenchmarkTileSize);
    software -> SetEnableIncrementalRefresh(_AutomaticBenchmarkOptimization != "none");
    software -> SetEnableCompactHits((_AutomaticBenchmarkOptimization == "compact-hits") ||
                                     (_AutomaticBenchmarkOptimization == "direct-color") ||
                                     (_AutomaticBenchmarkOptimization == "frustum-culling") ||
                                     (_AutomaticBenchmarkOptimization == "pbo-upload"));
    software -> SetEnableDirectColorWrites((_AutomaticBenchmarkOptimization == "direct-color") ||
                                           (_AutomaticBenchmarkOptimization == "frustum-culling") ||
                                           (_AutomaticBenchmarkOptimization == "pbo-upload"));
    software -> SetEnableFrustumCulling((_AutomaticBenchmarkOptimization == "frustum-culling") ||
                                        (_AutomaticBenchmarkOptimization == "pbo-upload"));
    software -> SetEnablePBOUpload(_AutomaticBenchmarkOptimization == "pbo-upload");
  }

  _DebugMode = 0;
  _Renderer -> SetDebugMode(_DebugMode);
  SyncFramebufferResolution(true);

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessInput
// ----------------------------------------------------------------------------
int Test6::ProcessInput()
{
  double curMouseX = 0., curMouseY = 0.;
  glfwGetCursorPos(_MainWindow.get(), &curMouseX, &curMouseY);

  if ( _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) )
  {
    if ( _MouseCaptured )
      SetMouseCaptured(false);
    else
      return 1;
  }

  if ( _KeyInput.IsKeyReleased(GLFW_KEY_F1) )
    _ShowDebugPanel = !_ShowDebugPanel;

  if ( !ImGui::GetIO().WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_F2) )
    _GameSettings._FreeLook = !_GameSettings._FreeLook;

  if ( !ImGui::GetIO().WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_F3) )
  {
    FpsGameEditorContext editorContext = MakeEditorContext();
    if ( _Editor.SetMode(editorContext, !_Editor.IsEnabled()) )
    {
      _HasLastMousePos = false;
      if ( _Editor.IsEnabled() )
        SetMouseCaptured(false);
    }
  }

  if ( _Editor.IsEnabled() )
  {
    if ( !ImGui::GetIO().WantCaptureKeyboard )
    {
      FpsRendererMode requestedRendererMode = _GameSettings._RendererMode;
      if ( _KeyInput.IsKeyDown(GLFW_KEY_J) )
        requestedRendererMode = FpsRendererMode::Deferred;
      else if ( _KeyInput.IsKeyDown(GLFW_KEY_K) )
        requestedRendererMode = FpsRendererMode::Software;
      else if ( _KeyInput.IsKeyDown(GLFW_KEY_L) )
        requestedRendererMode = FpsRendererMode::PhotoPathTracer;

      if ( requestedRendererMode != _GameSettings._RendererMode )
      {
        _GameSettings._RendererMode = requestedRendererMode;
        CaptureMapRenderSettings(_Map, _GameSettings, _Settings);
        _ReloadRenderer = true;
        _Editor.MarkDirty();
      }

      if ( _KeyInput.IsKeyReleased(GLFW_KEY_DELETE) )
      {
        FpsGameEditorContext editorContext = MakeEditorContext();
        _Editor.DeleteSelectedItem(editorContext);
      }
    }

    const bool rightMouseHeld = !ImGui::GetIO().WantCaptureMouse
                             && ( GLFW_PRESS == glfwGetMouseButton(_MainWindow.get(), GLFW_MOUSE_BUTTON_RIGHT) )
                             && ( FpsRendererMode::PhotoPathTracer != _GameSettings._RendererMode );
    double mouseX = 0., mouseY = 0.;
    if ( !ImGui::GetIO().WantCaptureMouse
      && !rightMouseHeld
      && _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY)
      && !ImGuizmo::IsOver()
      && !ImGuizmo::IsUsing() )
    {
      _Editor.HandleMousePick(MakeEditorContext(), mouseX, mouseY);
    }

    if ( rightMouseHeld )
    {
      FpsPlayer & player = _GameWorld.GetPlayer();
      if ( _HasLastMousePos )
      {
        player._Yaw += static_cast<float>(curMouseX - _LastMouseX) * _GameSettings._MouseSensitivity;
        player._Pitch -= static_cast<float>(curMouseY - _LastMouseY) * _GameSettings._MouseSensitivity;
        player._Pitch = MathUtil::Clamp(player._Pitch, -89.f, 89.f);
      }
      _HasLastMousePos = true;

      if ( !ImGui::GetIO().WantTextInput )
      {
        const float yawRad = MathUtil::ToRadians(player._Yaw);
        const float pitchRad = MathUtil::ToRadians(player._Pitch);
        Vec3 forward(std::cos(yawRad) * std::cos(pitchRad),
                     std::sin(pitchRad),
                     std::sin(yawRad) * std::cos(pitchRad));
        forward = glm::normalize(forward);
        const Vec3 right = glm::normalize(glm::cross(forward, Vec3(0.f, 1.f, 0.f)));
        const Vec3 up(0.f, 1.f, 0.f);

        Vec3 move(0.f);
        if ( _KeyInput.IsKeyDown(GLFW_KEY_W) )
          move += forward;
        if ( _KeyInput.IsKeyDown(GLFW_KEY_S) )
          move -= forward;
        if ( _KeyInput.IsKeyDown(GLFW_KEY_D) )
          move += right;
        if ( _KeyInput.IsKeyDown(GLFW_KEY_A) )
          move -= right;
        if ( _KeyInput.IsKeyDown(GLFW_KEY_SPACE) )
          move += up;
        if ( _KeyInput.IsKeyDown(GLFW_KEY_LEFT_CONTROL) || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_CONTROL) )
          move -= up;

        if ( glm::length(move) > EPSILON )
        {
          const float speed = ( _KeyInput.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_SHIFT) )
                            ? _GameSettings._SprintSpeed
                            : _GameSettings._MoveSpeed;
          const float deltaTime = std::min(static_cast<float>(_DeltaTime), 0.05f);
          player._Position += glm::normalize(move) * speed * deltaTime;
        }
      }

      if ( _Scene )
        _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings);
      if ( _Renderer )
        _Renderer -> Notify(DirtyState::SceneCamera);
    }
    else
      _HasLastMousePos = false;

    if ( _KeyInput.IsKeyReleased(GLFW_KEY_C) )
    {
      _CaptureOutputPath = "./Test6_" + std::to_string(_NbRenderedFrames) + "frames.png";
      _RenderToFile = true;
    }

    _LastMouseX = curMouseX;
    _LastMouseY = curMouseY;
    _KeyInput.ClearLastEvents();
    _MouseInput.ClearLastEvents(curMouseX, curMouseY);

    glfwPollEvents();
    return 0;
  }

  if ( !_MouseCaptured && !ImGui::GetIO().WantCaptureMouse && _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1) )
    SetMouseCaptured(true);

  FpsGameInput input;
  if ( !ImGui::GetIO().WantCaptureMouse && _MouseCaptured )
  {
    double mouseX = 0.f, mouseY = 0.f;

    // LEFT CLICK
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY) )
    {
      if ( ( FpsRendererMode::PhotoPathTracer != _GameSettings._RendererMode ) && !_GameSettings._FreeLook )
        input._FirePressed = true;
    }
  }

  if ( !ImGui::GetIO().WantCaptureKeyboard )
  {
    if ( _KeyInput.IsKeyReleased(GLFW_KEY_PAGE_UP) )
      _GameSettings._MoveSpeed = std::clamp(_GameSettings._MoveSpeed + 5.f, 1.f, 100.f);
    if ( _KeyInput.IsKeyReleased(GLFW_KEY_PAGE_DOWN) )
      _GameSettings._MoveSpeed = std::clamp(_GameSettings._MoveSpeed - 5.f, 1.f, 100.f);

    FpsRendererMode requestedRendererMode = _GameSettings._RendererMode;
    if ( _KeyInput.IsKeyDown(GLFW_KEY_J) )
      requestedRendererMode = FpsRendererMode::Deferred;
    else if ( _KeyInput.IsKeyDown(GLFW_KEY_K) )
      requestedRendererMode = FpsRendererMode::Software;
    else if ( _KeyInput.IsKeyDown(GLFW_KEY_L) )
      requestedRendererMode = FpsRendererMode::PhotoPathTracer;

    if ( requestedRendererMode != _GameSettings._RendererMode )
    {
      _GameSettings._RendererMode = requestedRendererMode;
      CaptureMapRenderSettings(_Map, _GameSettings, _Settings);
      _ReloadRenderer = true;
    }

    input._MoveForward  = _KeyInput.IsKeyDown(GLFW_KEY_W);
    input._MoveBackward = _KeyInput.IsKeyDown(GLFW_KEY_S);
    input._MoveLeft     = _KeyInput.IsKeyDown(GLFW_KEY_A);
    input._MoveRight    = _KeyInput.IsKeyDown(GLFW_KEY_D);
    input._Sprint       = _KeyInput.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_SHIFT);
    if ( _GameSettings._FreeLook )
    {
      input._MoveUp = _KeyInput.IsKeyDown(GLFW_KEY_SPACE);
      input._MoveDown = _KeyInput.IsKeyDown(GLFW_KEY_LEFT_CONTROL) || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_CONTROL);
    }
    else
      input._JumpPressed = _KeyInput.IsKeyDown(GLFW_KEY_SPACE);
    input._ResetPressed = _KeyInput.IsKeyReleased(GLFW_KEY_R);
  }

  if ( _MouseCaptured && ( FpsRendererMode::PhotoPathTracer != _GameSettings._RendererMode ) )
  {
    if ( _HasLastMousePos )
    {
      input._MouseDeltaX = static_cast<float>(curMouseX - _LastMouseX);
      input._MouseDeltaY = static_cast<float>(curMouseY - _LastMouseY);
    }
    _HasLastMousePos = true;
  }

  _LastMouseX = curMouseX;
  _LastMouseY = curMouseY;

  if ( FpsRendererMode::PhotoPathTracer != _GameSettings._RendererMode )
  {
    if ( 0 != _GameWorld.Update(static_cast<float>(_DeltaTime), input, _GameSettings) )
      return 1;

    if ( 0 != _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings) )
      return 1;

    if ( _Renderer )
      _Renderer -> Notify(DirtyState::SceneCamera);

    const bool projectilesDirty = _GameWorld.ConsumeProjectilesDirty();
    if ( projectilesDirty || ( _GameSettings._ShowViewWeapon && _SceneBinding.HasViewWeapon() ) )
    {
      if ( 0 != _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings) )
        return 1;

      if ( _Renderer )
        _Renderer -> Notify(DirtyState::SceneInstances);
    }
  }

  if ( _KeyInput.IsKeyReleased(GLFW_KEY_C) )
  {
    _CaptureOutputPath = "./Test6_" + std::to_string(_NbRenderedFrames) + "frames.png";
    _RenderToFile = true;
  }

  _KeyInput.ClearLastEvents();
  _MouseInput.ClearLastEvents(curMouseX, curMouseY);

  glfwPollEvents();
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateGame
// ----------------------------------------------------------------------------
int Test6::UpdateGame()
{
  BeginCpuTiming(CpuUpdateGame);

  if ( _ReloadScene )
  {
    _ReloadScene = false;
    _ReloadBoids = false;
    _Renderer.reset();
    if ( 0 != InitializeScene() )
    {
      EndCpuTiming(CpuUpdateGame);
      return 1;
    }
    _ReloadRenderer = true;
  }

  if ( _ReloadRenderer )
  {
    _ReloadRenderer = false;
    if ( 0 != InitializeRenderer() )
    {
      EndCpuTiming(CpuUpdateGame);
      return 1;
    }
  }

  if ( _ReloadBoids )
  {
    _ReloadBoids = false;
    if ( 0 != RefreshMapBoids() )
    {
      EndCpuTiming(CpuUpdateGame);
      return 1;
    }
  }

  if ( 0 != UpdateBoids() )
  {
    EndCpuTiming(CpuUpdateGame);
    return 1;
  }

  EndCpuTiming(CpuUpdateGame);
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateBoids
// ----------------------------------------------------------------------------
int Test6::UpdateBoids()
{
  BeginCpuTiming(CpuUpdateBoids);

  if ( !_Scene || ( FpsRendererMode::PhotoPathTracer == _GameSettings._RendererMode ) )
  {
    EndCpuTiming(CpuUpdateBoids);
    SetCpuTimingEnabled(CpuUpdateBoids, false);
    return 0;
  }

  if ( _BoidSimulations.size() != _BoidBindings.size() )
  {
    EndCpuTiming(CpuUpdateBoids);
    return 1;
  }

  bool boidsDirty = false;
  bool boidsUpdated = false;
  for ( int i = 0; i < static_cast<int>(_BoidSimulations.size()); ++i )
  {
    if ( i >= static_cast<int>(_BoidSettings.size()) )
    {
      EndCpuTiming(CpuUpdateBoids);
      return 1;
    }

    if ( _BoidSettings[i]._Paused )
      continue;

    BeginCpuTiming(CpuBoidsSimulation);
    if ( 0 != _BoidSimulations[i].Update(static_cast<float>(_DeltaTime), _BoidSettings[i]) )
    {
      EndCpuTiming(CpuBoidsSimulation);
      EndCpuTiming(CpuUpdateBoids);
      return 1;
    }
    EndCpuTiming(CpuBoidsSimulation);

    BeginCpuTiming(CpuBoidsSceneSync);
    if ( 0 != _BoidBindings[i].SyncTransforms(*_Scene, _BoidSimulations[i], _BoidSettings[i]) )
    {
      EndCpuTiming(CpuBoidsSceneSync);
      EndCpuTiming(CpuUpdateBoids);
      return 1;
    }
    EndCpuTiming(CpuBoidsSceneSync);

    boidsUpdated = true;
    boidsDirty = true;
  }

  if ( boidsDirty && _Renderer )
  {
    BeginCpuTiming(CpuBoidsRendererNotify);
    _Renderer -> Notify(DirtyState::SceneInstances);
    EndCpuTiming(CpuBoidsRendererNotify);
  }

  EndCpuTiming(CpuUpdateBoids);
  if ( !boidsUpdated )
  {
    SetCpuTimingEnabled(CpuUpdateBoids, false);
    SetCpuTimingEnabled(CpuBoidsSimulation, false);
    SetCpuTimingEnabled(CpuBoidsSceneSync, false);
    SetCpuTimingEnabled(CpuBoidsRendererNotify, false);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
int Test6::DrawUI()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  if ( _ShowDebugPanel )
  {
    DrawDebugPanel();
    DrawRenderStatsUI();
    DrawRenderSettingsUI();
  }

  if ( _Editor.IsEnabled() )
  {
    FpsGameEditorContext editorContext = MakeEditorContext();
    _Editor.DrawDockspace();
    _Editor.DrawPanels(editorContext);
    _Editor.DrawOverlays(editorContext);
    _Editor.DrawGizmo(editorContext);
    _Hud.DrawCrosshair();
  }
  else
    _Hud.Draw(MakeHudContext());

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  return 0;
}

// ----------------------------------------------------------------------------
// DrawDebugPanel
// ----------------------------------------------------------------------------
void Test6::DrawDebugPanel()
{
  ImGui::Begin("Test6 FPS", nullptr, ImGuiWindowFlags_NoDocking);

  static const char * Renderers[] = { "Deferred", "Software", "Photo Path Tracer" };
  int rendererMode = (int)_GameSettings._RendererMode;
  if ( ImGui::Combo("Renderer", &rendererMode, Renderers, 3) )
  {
    if ( rendererMode != (int)_GameSettings._RendererMode )
    {
      _GameSettings._RendererMode = (FpsRendererMode)rendererMode;
      CaptureMapRenderSettings(_Map, _GameSettings, _Settings);
      _ReloadRenderer = true;
      if ( _Editor.IsEnabled() )
        _Editor.MarkDirty();
    }
  }

  if ( FpsRendererMode::PhotoPathTracer == _GameSettings._RendererMode )
    ImGui::Text("Photo mode: gameplay paused");
  else
    ImGui::Text("Click viewport to capture mouse. Esc releases capture.");
  ImGui::Text("Renderer shortcuts: J Deferred, K Software, L Photo");
  ImGui::Text("F1 toggles this panel. F2 toggles free look. F3 toggles editor.");
  ImGui::Text("Editor: %s", _Editor.IsEnabled() ? "on" : "off");

  ImGui::Checkbox("Free look", &_GameSettings._FreeLook);

  const FpsPlayer & player = _GameWorld.GetPlayer();
  ImGui::Text("Mode: %s", _GameSettings._FreeLook ? "Free look" : "Player physics");
  ImGui::Text("Position: %.2f %.2f %.2f", player._Position.x, player._Position.y, player._Position.z);
  ImGui::Text("Velocity: %.2f %.2f %.2f", player._Velocity.x, player._Velocity.y, player._Velocity.z);
  ImGui::Text("Yaw/Pitch: %.1f %.1f", player._Yaw, player._Pitch);
  ImGui::Text("Grounded: %s", player._Grounded ? "yes" : "no");
  ImGui::Text("Health/Armor: %d / %d", player._Health, player._Armor);
  ImGui::Text("Projectile ammo: %d / %d", _GameWorld.GetProjectileAmmo(), _GameSettings._MaxProjectileAmmo);
  ImGui::Text("Frame: %.2f ms / %.1f FPS", _FrameTime * 1000., _FrameRate);

  if ( ImGui::Button("Reset player") )
  {
    _GameWorld.Reset(_GameSettings);
    _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings);
    _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings);
    if ( _Renderer )
    {
      _Renderer -> Notify(DirtyState::SceneCamera);
      _Renderer -> Notify(DirtyState::SceneInstances);
    }
  }

  ImGui::SameLine();
  if ( ImGui::Button("Capture image") )
  {
    _CaptureOutputPath = "./Test6_" + std::to_string(_NbRenderedFrames) + "frames.png";
    _RenderToFile = true;
  }

  DrawBenchmarkUI();
  DrawGameSettingsUI();

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawBenchmarkUI
// ----------------------------------------------------------------------------
int Test6::DrawBenchmarkUI()
{
  if ( !ImGui::CollapsingHeader("Benchmark") )
    return 0;

  ImGui::Text("Active renderer: %s", FpsGameBenchmark::GetRendererName(_GameSettings._RendererMode));
  ImGui::Text("Pose: %.3f %.3f %.3f / yaw %.2f / pitch %.2f",
              FpsGameBenchmark::GetPosition().x,
              FpsGameBenchmark::GetPosition().y,
              FpsGameBenchmark::GetPosition().z,
              FpsGameBenchmark::GetYaw(),
              FpsGameBenchmark::GetPitch());

  char benchmarkLabel[128] = {};
  std::snprintf(benchmarkLabel, sizeof(benchmarkLabel), "%s", _Benchmark.GetLabel().c_str());
  if ( ImGui::InputText("Label", benchmarkLabel, sizeof(benchmarkLabel)) )
    _Benchmark.GetLabel() = benchmarkLabel;

  ImGui::InputInt("Warmup frames", &_Benchmark.GetWarmupFrames());
  ImGui::InputInt("Sample frames", &_Benchmark.GetSampleFrames());
  ImGui::InputInt("Repetitions", &_Benchmark.GetRepetitions());
  _Benchmark.GetWarmupFrames() = std::max(0, _Benchmark.GetWarmupFrames());
  _Benchmark.GetSampleFrames() = std::max(1, _Benchmark.GetSampleFrames());
  _Benchmark.GetRepetitions() = std::max(1, _Benchmark.GetRepetitions());

  if ( _Renderer )
  {
    if ( SoftwareRasterizer * software = _Renderer -> AsSoftwareRasterizer() )
    {
      bool simd = software -> GetEnableSIMD();
      if ( ImGui::Checkbox("SIMD", &simd) )
        software -> SetEnableSIMD(simd);
      int tileSize = static_cast<int>(software -> GetTileSize());
      if ( ImGui::InputInt("Effective tile size", &tileSize, 32, 64) )
        software -> SetTileSize(static_cast<unsigned int>(std::max(8, tileSize)));
      ImGui::Text("SIMD mode: %s", software -> GetSIMDMode());
      bool incrementalRefresh = software -> GetEnableIncrementalRefresh();
      if ( ImGui::Checkbox("Incremental instance refresh", &incrementalRefresh) )
        software -> SetEnableIncrementalRefresh(incrementalRefresh);
      bool compactHits = software -> GetEnableCompactHits();
      if ( ImGui::Checkbox("Compact hit buffer", &compactHits) )
        software -> SetEnableCompactHits(compactHits);
      bool directColorWrites = software -> GetEnableDirectColorWrites();
      if ( ImGui::Checkbox("Direct color writes", &directColorWrites) )
        software -> SetEnableDirectColorWrites(directColorWrites);
      bool frustumCulling = software -> GetEnableFrustumCulling();
      if ( ImGui::Checkbox("Instance frustum culling", &frustumCulling) )
        software -> SetEnableFrustumCulling(frustumCulling);
      bool pboUpload = software -> GetEnablePBOUpload();
      if ( ImGui::Checkbox("PBO color upload", &pboUpload) )
        software -> SetEnablePBOUpload(pboUpload);
    }
  }

  if ( !_Benchmark.IsRunning() )
  {
    if ( ImGui::Button("Run benchmark") )
      StartBenchmark();
  }
  else
  {
    ImGui::Text("Running repetition %d / %d: warmup %d / %d, samples %d / %d",
                _Benchmark.GetCurrentRepetition() + 1,
                _Benchmark.GetRepetitions(),
                _Benchmark.GetWarmupDone(),
                _Benchmark.GetWarmupFrames(),
                _Benchmark.GetSamplesDone(),
                _Benchmark.GetSampleFrames());
    if ( ImGui::Button("Cancel benchmark") )
      _Benchmark.Cancel();
  }

  if ( _Benchmark.IsCompleted() && _Benchmark.GetTotalSamples() > 0 )
  {
    const int samples = _Benchmark.GetTotalSamples();
    ImGui::Separator();
    ImGui::Text("Last %s result over %d frames", FpsGameBenchmark::GetRendererName(_Benchmark.GetRendererMode()), samples);
    if ( !_Benchmark.GetResultPath().empty() )
      ImGui::TextWrapped("Saved: %s", _Benchmark.GetResultPath().c_str());
    const FpsGameBenchmarkDistribution & distribution = _Benchmark.GetCpuFrameDistribution();
    ImGui::Text("CPU frame median / mean: %.3f / %.3f ms", distribution._Median * 1000., distribution._Mean * 1000.);
    ImGui::Text("Minimum / p95 / stddev: %.3f / %.3f / %.3f ms", distribution._Minimum * 1000., distribution._P95 * 1000., distribution._StandardDeviation * 1000.);

    if ( ImGui::TreeNode("CPU timings") )
    {
      for ( const FpsCpuTiming & timing : _Benchmark.GetCpuTotals() )
      {
        if ( timing._Enabled )
          ImGui::Text("%-24s : %.3f ms", timing._Name, timing._Seconds * 1000. / samples);
      }
      ImGui::TreePop();
    }

    if ( ImGui::TreeNode("Renderer pass timings") )
    {
      for ( const FpsGameBenchmarkPassTiming & timing : _Benchmark.GetRendererTotals() )
      {
        if ( timing._Enabled )
          ImGui::Text("%-24s : %.3f ms %s%s", timing._Name.c_str(), timing._Distribution._Median * 1000., timing._GPU ? "[GPU]" : "[CPU]", timing._Inclusive ? " [inclusive]" : "");
      }
      ImGui::TreePop();
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawGameSettingsUI
// ----------------------------------------------------------------------------
int Test6::DrawGameSettingsUI()
{
  if ( ImGui::CollapsingHeader("Game tuning") )
  {
    ImGui::SliderFloat("Move speed", &_GameSettings._MoveSpeed, 1.f, 100.f);
    ImGui::SliderFloat("Sprint speed", &_GameSettings._SprintSpeed, 1.f, 16.f);
    ImGui::SliderFloat("Mouse sensitivity", &_GameSettings._MouseSensitivity, 0.01f, 0.25f);
    ImGui::SliderFloat("Player radius", &_GameSettings._PlayerRadius, 0.15f, 0.8f);
    ImGui::SliderFloat("Player height", &_GameSettings._PlayerHeight, 1.f, 2.4f);
    ImGui::SliderFloat("Gravity", &_GameSettings._Gravity, 1.f, 35.f);
    ImGui::SliderFloat("Jump speed", &_GameSettings._JumpSpeed, 1.f, 12.f);

    if ( ImGui::TreeNode("Head bob") )
    {
      ImGui::Checkbox("Enabled", &_GameSettings._HeadBob._Enabled);
      ImGui::SliderFloat("Amplitude", &_GameSettings._HeadBob._Amplitude, 0.f, 0.08f, "%.3f");
      ImGui::SliderFloat("Frequency", &_GameSettings._HeadBob._Frequency, 0.5f, 4.f, "%.2f");
      ImGui::SliderFloat("Sway", &_GameSettings._HeadBob._Sway, 0.f, 0.05f, "%.3f");
      ImGui::SliderFloat("Smoothing", &_GameSettings._HeadBob._Smoothing, 1.f, 50.f, "%.1f");
      ImGui::TreePop();
    }
  }

  if ( ImGui::CollapsingHeader("Projectiles") )
  {
    ImGui::Text("Active: %d / %d", _GameWorld.GetActiveProjectileCount(), (int)_GameWorld.GetProjectiles().size());
    ImGui::Text("Ammo: %d / %d, refill %.2fs", _GameWorld.GetProjectileAmmo(), _GameSettings._MaxProjectileAmmo, _GameSettings._ProjectileAmmoRefillTime);
    ImGui::SliderFloat("Projectile speed", &_GameSettings._ProjectileSpeed, 1.f, 40.f);
    ImGui::SliderFloat("Projectile radius", &_GameSettings._ProjectileRadius, 0.04f, 0.35f);
    ImGui::SliderFloat("Projectile bounciness", &_GameSettings._ProjectileBounciness, 0.f, 1.f);
    ImGui::SliderFloat("Projectile lifetime", &_GameSettings._ProjectileLifetime, 0.5f, 20.f);
    ImGui::SliderFloat("Projectile cooldown", &_GameSettings._ProjectileCooldown, 0.02f, 1.f);
    ImGui::SliderFloat("Projectile gravity", &_GameSettings._ProjectileGravity, 0.f, 30.f);

    int maxProjectiles = _GameSettings._MaxProjectiles;
    if ( ImGui::SliderInt("Max projectiles", &maxProjectiles, 1, 128) )
    {
      _GameSettings._MaxProjectiles = std::max(1, maxProjectiles);
      _ReloadScene = true;
    }

    if ( ImGui::Button("Clear projectiles") )
    {
      _GameWorld.ClearProjectiles();
      _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings);
      if ( _Renderer )
        _Renderer -> Notify(DirtyState::SceneInstances);
    }
  }

  if ( ImGui::CollapsingHeader("View weapon debug", ImGuiTreeNodeFlags_DefaultOpen) )
  {
    constexpr Vec3 defaultOffset(0.44f, -0.33f, 0.83f);
    constexpr Vec3 defaultRotation(-6.5f, -10.5f, 3.f);
    constexpr float defaultScale = 1.24f;

    bool weaponDirty = false;
    weaponDirty |= ImGui::Checkbox("Show weapon", &_GameSettings._ShowViewWeapon);

    ImGui::Text("Camera-local transform");
    ImGui::PushItemWidth(260.f);
    weaponDirty |= ImGui::DragFloat3("Translation XYZ", &_GameSettings._ViewWeaponOffset.x, 0.01f, -3.f, 3.f, "%.3f");
    weaponDirty |= ImGui::DragFloat3("Rotation XYZ deg", &_GameSettings._ViewWeaponRotation.x, 0.5f, -360.f, 360.f, "%.2f");
    weaponDirty |= ImGui::DragFloat("Scale", &_GameSettings._ViewWeaponScale, 0.01f, 0.01f, 8.f, "%.3f");
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if ( ImGui::SmallButton("Reset weapon pose") )
    {
      _GameSettings._ViewWeaponOffset = defaultOffset;
      _GameSettings._ViewWeaponRotation = defaultRotation;
      _GameSettings._ViewWeaponScale = defaultScale;
      weaponDirty = true;
    }

    if ( ImGui::SmallButton("Copy weapon pose") )
    {
      char buffer[256];
      std::snprintf(buffer, sizeof(buffer),
                    "Offset = Vec3(%.3ff, %.3ff, %.3ff), Rotation = Vec3(%.2ff, %.2ff, %.2ff), Scale = %.3ff",
                    _GameSettings._ViewWeaponOffset.x, _GameSettings._ViewWeaponOffset.y, _GameSettings._ViewWeaponOffset.z,
                    _GameSettings._ViewWeaponRotation.x, _GameSettings._ViewWeaponRotation.y, _GameSettings._ViewWeaponRotation.z,
                    _GameSettings._ViewWeaponScale);
      ImGui::SetClipboardText(buffer);
    }

    ImGui::Text("Offset %.3f %.3f %.3f",
                _GameSettings._ViewWeaponOffset.x,
                _GameSettings._ViewWeaponOffset.y,
                _GameSettings._ViewWeaponOffset.z);
    ImGui::Text("Rotation %.2f %.2f %.2f, Scale %.3f",
                _GameSettings._ViewWeaponRotation.x,
                _GameSettings._ViewWeaponRotation.y,
                _GameSettings._ViewWeaponRotation.z,
                _GameSettings._ViewWeaponScale);

    if ( weaponDirty )
    {
      _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings);
      if ( _Renderer )
        _Renderer -> Notify(DirtyState::SceneInstances);
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawRenderSettingsUI
// ----------------------------------------------------------------------------
int Test6::DrawRenderSettingsUI()
{
  ImGui::Begin("Test6 Render Settings", nullptr, ImGuiWindowFlags_NoDocking);

  FpsGameEditorContext editorContext = MakeEditorContext();
  _Editor.DrawRenderSettingsUI(editorContext);

  ImGui::End();
  return 0;
}

// ----------------------------------------------------------------------------
// DrawRenderStatsUI
// ----------------------------------------------------------------------------
int Test6::DrawRenderStatsUI()
{
  ImGui::Begin("Test6 Rendering Stats", nullptr, ImGuiWindowFlags_NoDocking);

  FpsGameEditorContext editorContext = MakeEditorContext();
  _Editor.DrawPerformanceUI(editorContext);

  ImGui::End();
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test6::UpdateCPUTime()
{
  double oldCpuTime = _CPUTime;
  _CPUTime = glfwGetTime();
  _DeltaTime = _CPUTime - oldCpuTime;

  static int nbFrames = 0;
  nbFrames++;

  static double accu = 0.;
  accu += _DeltaTime;

  double nbSeconds = 0.;
  while ( accu >= 1. )
  {
    accu -= 1.;
    nbSeconds++;
  }

  if ( nbSeconds >= 1. )
  {
    nbSeconds += accu;
    _FrameRate = (double)nbFrames / nbSeconds;
    _FrameTime = ( _FrameRate ) ? ( 1. / _FrameRate ) : ( 0. );
    nbFrames = 0;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Run
// ----------------------------------------------------------------------------
int Test6::Run()
{
  int ret = 0;

  do
  {
    if ( !_MainWindow )
    {
      ret = 1;
      break;
    }

    glfwSetWindowTitle(_MainWindow.get(), GetTestHeader());
    glfwSetWindowUserPointer(_MainWindow.get(), this);
    glfwSetFramebufferSizeCallback(_MainWindow.get(), Test6::FramebufferSizeCallback);
    glfwSetMouseButtonCallback(_MainWindow.get(), Test6::MouseButtonCallback);
    glfwSetScrollCallback(_MainWindow.get(), Test6::MouseScrollCallback);
    glfwSetKeyCallback(_MainWindow.get(), Test6::KeyCallback);
    glfwSetDropCallback(_MainWindow.get(), Test6::DropCallback);

    glfwMakeContextCurrent(_MainWindow.get());
    glfwSwapInterval(0);

    glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    SyncFramebufferResolution();

    if ( 0 != InitializeUI() )
    {
      ret = 1;
      break;
    }

    glewExperimental = GL_TRUE;
    if ( glewInit() != GLEW_OK )
    {
      std::cout << "Failed to initialize GLEW!" << std::endl;
      ret = 1;
      break;
    }

    if ( 0 != InitializeScene() )
    {
      std::cout << "ERROR: Test6 scene initialization failed!" << std::endl;
      ret = 1;
      break;
    }

    if ( 0 != InitializeRenderer() || !_Renderer )
    {
      std::cout << "ERROR: Test6 renderer initialization failed!" << std::endl;
      ret = 1;
      break;
    }

    glDisable(GL_DEPTH_TEST);

    if ( _AutomaticBenchmark )
      StartBenchmark();

    while ( !glfwWindowShouldClose(_MainWindow.get()) )
    {
      UpdateCPUTime();
      ResetCpuTimings();

      if ( 0 != UpdateGame() )
        break;

      BeginCpuTiming(CpuProcessInput);
      if ( 0 != ProcessInput() )
      {
        EndCpuTiming(CpuProcessInput);
        break;
      }
      EndCpuTiming(CpuProcessInput);

      BeginCpuTiming(CpuRendererUpdate);
      _Renderer -> Update();
      EndCpuTiming(CpuRendererUpdate);

      BeginCpuTiming(CpuRenderToTexture);
      _Renderer -> RenderToTexture();
      EndCpuTiming(CpuRenderToTexture);

      BeginCpuTiming(CpuRenderToScreen);
      _Renderer -> RenderToScreen();
      EndCpuTiming(CpuRenderToScreen);

      if ( _RenderToFile )
      {
        BeginCpuTiming(CpuRenderToFile);
        _Renderer -> RenderToFile(_CaptureOutputPath);
        EndCpuTiming(CpuRenderToFile);
        _RenderToFile = false;
      }
      else
        SetCpuTimingEnabled(CpuRenderToFile, false);

      _Renderer -> Done();

      BeginCpuTiming(CpuDrawUI);
      DrawUI();
      EndCpuTiming(CpuDrawUI);

      BeginCpuTiming(CpuSwapBuffers);
      glfwSwapBuffers(_MainWindow.get());
      EndCpuTiming(CpuSwapBuffers);
      _DisplayedCpuTimings = _CpuTimings;
      UpdateBenchmark();
      if ( _AutomaticBenchmark && _Benchmark.IsCompleted() )
        glfwSetWindowShouldClose(_MainWindow.get(), GLFW_TRUE);
      _NbRenderedFrames++;
    }
  } while ( 0 );

  SetMouseCaptured(false);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwSetFramebufferSizeCallback(_MainWindow.get(), nullptr);
  glfwSetMouseButtonCallback(_MainWindow.get(), nullptr);
  glfwSetScrollCallback(_MainWindow.get(), nullptr);
  glfwSetKeyCallback(_MainWindow.get(), nullptr);
  glfwSetDropCallback(_MainWindow.get(), nullptr);

  return ret;
}

}
