#pragma warning(disable : 4100) // unreferenced formal parameter

#include "FpsGameApp.h"
#include "Localization.h"

#include "DeferredRenderer.h"
#include "DroppedFileUtils.h"
#include "NativeFileDialog.h"
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
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <thread>

namespace FPS
{
using namespace RTRT;

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static constexpr int g_ScreenWidth  = 1280;
static constexpr int g_ScreenHeight = 720;

const char * FpsGameApp::GetTestHeader() { return "Basic FPS"; }
const char * FpsGameApp::GetAppHeader() { return "Basic FPS"; }

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
  ioSettings._WeaponSlots[0]._View._Offset = iMap._Weapon._Offset;
  ioSettings._WeaponSlots[0]._View._Rotation = iMap._Weapon._Rotation;
  ioSettings._WeaponSlots[0]._View._Scale = iMap._Weapon._Scale;
}

// ----------------------------------------------------------------------------
// GetWeaponFireSoundId / GetWeaponReloadSoundId
// ----------------------------------------------------------------------------
static const char * GetWeaponFireSoundId( int iSlot )
{
  if ( iSlot == (int)FpsWeaponId::Mark23Pistol )
    return "fps_fire_mark23";
  if ( iSlot == (int)FpsWeaponId::CarbineRifle )
    return "fps_fire_carbine";
  return "fps_fire_launcher";
}

static const char * GetWeaponReloadSoundId( int iSlot )
{
  if ( iSlot == (int)FpsWeaponId::Mark23Pistol )
    return "fps_reload_mark23";
  if ( iSlot == (int)FpsWeaponId::CarbineRifle )
    return "fps_reload_carbine";
  return nullptr;
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
  ioGameSettings._CameraOrthographic = settings._CameraOrthographic;
  ioGameSettings._CameraOrthographicHeight = std::max(0.001f, settings._CameraOrthographicHeight);

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
  ioRenderSettings._SSAOMaxDistance = std::max(0.1f, settings._SSAOMaxDistance);
  ioRenderSettings._SSAOFadeDistance = std::max(0.001f, settings._SSAOFadeDistance);

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
  ioRenderSettings._DenoisingMethod = settings._DenoisingMethod;
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
  settings._CameraOrthographic = iGameSettings._CameraOrthographic;
  settings._CameraOrthographicHeight = iGameSettings._CameraOrthographicHeight;
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
  settings._SSAOMaxDistance = iRenderSettings._SSAOMaxDistance;
  settings._SSAOFadeDistance = iRenderSettings._SSAOFadeDistance;
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
  settings._DenoisingMethod = iRenderSettings._DenoisingMethod;
}

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void FpsGameApp::KeyCallback( GLFWwindow * iWindow, const int iKey, const int iScancode, const int iAction, const int iMods )
{
  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  if ( ( GLFW_PRESS == iAction ) || ( GLFW_RELEASE == iAction ) )
  {
    this_ -> _KeyInput.AddEvent(iKey, iAction, iMods);

    const bool isAltKey = ( GLFW_KEY_LEFT_ALT == iKey ) || ( GLFW_KEY_RIGHT_ALT == iKey );
    if ( isAltKey && ( GLFW_PRESS == iAction ) )
      this_ -> _AltMenuToggleArmed = true;
    else if ( !isAltKey && ( GLFW_PRESS == iAction )
      && ( this_ -> _KeyInput.IsKeyDown(GLFW_KEY_LEFT_ALT) || this_ -> _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_ALT) ) )
    {
      this_ -> _AltMenuToggleArmed = false;
    }
    else if ( isAltKey && ( GLFW_RELEASE == iAction ) )
    {
      if ( this_ -> _AltMenuToggleArmed
        && !this_ -> _KeyInput.IsKeyDown(GLFW_KEY_LEFT_ALT)
        && !this_ -> _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_ALT) )
      {
        this_ -> _ShowMenuBar = !this_ -> _ShowMenuBar;
      }
      this_ -> _AltMenuToggleArmed = false;
    }
  }

  // Menu navigation and pause requests
  const bool menuActive = ( FpsAppState::TitleScreen == this_ -> _AppState ) || this_ -> _Paused;
  const bool inGame = ( FpsAppState::InGame == this_ -> _AppState );
  const bool editorOn = this_ -> _Editor.IsEnabled();
  const bool photoModeOn = ( FpsPhotoModeState::Inactive != this_ -> _PhotoModeState );

  if ( menuActive && ( ( GLFW_PRESS == iAction ) || ( GLFW_REPEAT == iAction ) ) )
  {
    if ( ( GLFW_KEY_UP == iKey ) || ( GLFW_KEY_W == iKey ) )
      this_ -> _MenuInput._NavUp = true;
    if ( ( GLFW_KEY_DOWN == iKey ) || ( GLFW_KEY_S == iKey ) )
      this_ -> _MenuInput._NavDown = true;
    if ( ( GLFW_KEY_LEFT == iKey ) || ( GLFW_KEY_A == iKey ) )
      this_ -> _MenuInput._NavLeft = true;
    if ( ( GLFW_KEY_RIGHT == iKey ) || ( GLFW_KEY_D == iKey ) )
      this_ -> _MenuInput._NavRight = true;
    if ( GLFW_KEY_Q == iKey )
      this_ -> _MenuInput._PrevTab = true;
    if ( GLFW_KEY_E == iKey )
      this_ -> _MenuInput._NextTab = true;
  }

  if ( GLFW_PRESS == iAction )
  {
    if ( menuActive )
    {
      if ( ( GLFW_KEY_ENTER == iKey ) || ( GLFW_KEY_KP_ENTER == iKey ) || ( GLFW_KEY_SPACE == iKey ) )
        this_ -> _MenuInput._Confirm = true;
      if ( ( GLFW_KEY_ESCAPE == iKey ) || ( GLFW_KEY_BACKSPACE == iKey ) || ( inGame && ( GLFW_KEY_P == iKey ) ) )
        this_ -> _MenuInput._Back = true;
    }
    else if ( inGame && !editorOn && !photoModeOn && ( ( GLFW_KEY_ESCAPE == iKey ) || ( GLFW_KEY_P == iKey ) ) )
      this_ -> _PauseRequested = true;

    if ( ( GLFW_KEY_F1 == iKey ) && !editorOn && !photoModeOn )
    {
      if ( !inGame )
        this_ -> _Hud.SetMenuPage(( FpsMenuPage::Options == this_ -> _Hud.GetMenuPage() ) ? FpsMenuPage::MainMenu : FpsMenuPage::Options);
      else if ( !this_ -> _Paused )
      {
        this_ -> _PauseRequested = true;
        this_ -> _Hud.SetSettingsOpen(true);
      }
      else
        this_ -> _Hud.SetSettingsOpen(!this_ -> _Hud.IsSettingsOpen());
    }

    // Toggle Fullscreen
    if ( ( GLFW_KEY_F11 == iKey ) || ( ( GLFW_KEY_ENTER == iKey ) && ( iMods & GLFW_MOD_ALT ) ) )
    {
      GameMenuUI::ToggleWindowFullscreen(iWindow, this_->_Settings);
    }
  }
}

// ----------------------------------------------------------------------------
// MouseButtonCallback
// ----------------------------------------------------------------------------
void FpsGameApp::MouseButtonCallback( GLFWwindow * iWindow, const int iButton, const int iAction, const int iMods )
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
void FpsGameApp::MouseScrollCallback( GLFWwindow * iWindow, const double iOffsetX, const double iOffsetY )
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
void FpsGameApp::FramebufferSizeCallback( GLFWwindow * iWindow, const int iWidth, const int iHeight )
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
void FpsGameApp::DropCallback( GLFWwindow * iWindow, int iCount, const char ** iPaths )
{
  auto * const this_ = static_cast<Test6*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  this_ -> HandleDroppedFiles(iCount, iPaths);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
FpsGameApp::FpsGameApp( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow), _ScreenWidth(iScreenWidth), _ScreenHeight(iScreenHeight)
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
FpsGameApp::~FpsGameApp()
{
  SetMouseCaptured(false);
  _Preferences.CaptureFrom(_Settings, &_Audio);
  _Preferences._Language = Loc::GetLanguage();
  _Preferences.SaveToFile(UserPreferences::GetDefaultFilePath("fps_preferences.json"));
}

// ----------------------------------------------------------------------------
// EnableAutomaticSoftwareBenchmark
// ----------------------------------------------------------------------------
void FpsGameApp::EnableAutomaticSoftwareBenchmark( const std::string & iLabel,
                                              bool iSIMD,
                                              unsigned int iTileSize,
                                              const std::string & iOptimization,
                                              const std::string & iPose )
{
  _AutomaticBenchmark = true;
  _AppState = FpsAppState::InGame;
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
void FpsGameApp::SyncFramebufferResolution( bool iNotifyRenderer )
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
  const Vec2i baseRes = ( _Settings._CustomRenderResolution.x > 0 && _Settings._CustomRenderResolution.y > 0 )
                        ? _Settings._CustomRenderResolution
                        : _Settings._WindowResolution;
  _Settings._RenderResolution.x = int(baseRes.x * (_Settings._RenderScale * 0.01f));
  _Settings._RenderResolution.y = int(baseRes.y * (_Settings._RenderScale * 0.01f));

  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  if ( iNotifyRenderer && _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);

  if ( _PhotoRenderer )
  {
    _PhotoRenderSettings._WindowResolution = _Settings._WindowResolution;
    _PhotoRenderSettings._RenderResolution = _Settings._WindowResolution;
    if ( FpsPhotoModeState::Capturing == _PhotoModeState )
    {
      _PhotoCaptureWarmupDone = false;
      _PhotoRenderer -> Notify(DirtyState::RenderSettings);
    }
  }
}

// ----------------------------------------------------------------------------
// SetMouseCaptured
// ----------------------------------------------------------------------------
void FpsGameApp::SetMouseCaptured( bool iCaptured )
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
void FpsGameApp::HandleDroppedFiles( int iCount, const char ** iPaths )
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
void FpsGameApp::ApplyRendererDefaults()
{
  if ( FpsRendererMode::Software == _GameSettings._RendererMode )
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
    _Settings._Transparency = true;
    _Settings._TiledRendering = false;
  }
}

// ----------------------------------------------------------------------------
// SetRendererMode
// ----------------------------------------------------------------------------
void FpsGameApp::SetRendererMode( FpsRendererMode iMode )
{
  if ( iMode == _GameSettings._RendererMode )
    return;

  _GameSettings._RendererMode = iMode;
  ApplyRendererDefaults();
  CaptureMapRenderSettings(_Map, _GameSettings, _Settings);
  _ReloadRenderer = true;
  if ( _Editor.IsEnabled() )
    _Editor.MarkDirty();
}

// ----------------------------------------------------------------------------
// InitializeCpuTimings
// ----------------------------------------------------------------------------
void FpsGameApp::InitializeCpuTimings()
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
void FpsGameApp::ResetCpuTimings()
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
void FpsGameApp::BeginCpuTiming( int iTimingID )
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
void FpsGameApp::EndCpuTiming( int iTimingID )
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
void FpsGameApp::SetCpuTimingEnabled( int iTimingID, bool iEnabled )
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
void FpsGameApp::ApplyBenchmarkPose()
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
    _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings, _Editor.IsEnabled());
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
void FpsGameApp::StartBenchmark()
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
void FpsGameApp::UpdateBenchmark()
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
void FpsGameApp::FinishBenchmark()
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
// ----------------------------------------------------------------------------
// InitializeSoundBank
// ----------------------------------------------------------------------------
void FpsGameApp::InitializeSoundBank()
{
  // 1. UI Navigation
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 1200.0f;
    desc._EndFrequency = 1600.0f;
    desc._Duration = 0.025f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.010f;
    desc._SustainLevel = 0.2f;
    desc._ReleaseTime = 0.013f;
    desc._Volume = 0.5f;
    _Audio.RegisterProceduralSound("ui_nav", desc);
  }

  // 2. UI Confirm
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 660.0f;
    desc._EndFrequency = 660.0f;
    desc._ArpInterval2 = 5.0f;
    desc._ArpInterval3 = 12.0f;
    desc._ArpSpeed = 0.035f;
    desc._Duration = 0.12f;
    desc._AttackTime = 0.004f;
    desc._DecayTime = 0.030f;
    desc._SustainLevel = 0.4f;
    desc._ReleaseTime = 0.08f;
    desc._Volume = 0.7f;
    _Audio.RegisterProceduralSound("ui_confirm", desc);
  }

  // 3. UI Back
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 660.0f;
    desc._EndFrequency = 660.0f;
    desc._ArpInterval2 = -5.0f;
    desc._ArpSpeed = 0.040f;
    desc._Duration = 0.10f;
    desc._AttackTime = 0.004f;
    desc._DecayTime = 0.030f;
    desc._SustainLevel = 0.3f;
    desc._ReleaseTime = 0.06f;
    desc._Volume = 0.65f;
    _Audio.RegisterProceduralSound("ui_back", desc);
  }

  // 4. Footstep
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Noise;
    desc._StartFrequency = 180.0f;
    desc._EndFrequency = 60.0f;
    desc._Duration = 0.07f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.025f;
    desc._SustainLevel = 0.2f;
    desc._ReleaseTime = 0.043f;
    desc._NoiseMix = 0.85f;
    desc._Volume = 0.45f;
    _Audio.RegisterProceduralSound("fps_step", desc);
  }

  // 5. Jump
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Sine;
    desc._StartFrequency = 140.0f;
    desc._EndFrequency = 340.0f;
    desc._Duration = 0.12f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.040f;
    desc._SustainLevel = 0.3f;
    desc._ReleaseTime = 0.075f;
    desc._NoiseMix = 0.2f;
    desc._Volume = 0.65f;
    _Audio.RegisterProceduralSound("fps_jump", desc);
  }

  // 6. Land
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Square;
    desc._StartFrequency = 220.0f;
    desc._EndFrequency = 45.0f;
    desc._FrequencyExp = 2.0f;
    desc._Duration = 0.14f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.060f;
    desc._SustainLevel = 0.2f;
    desc._ReleaseTime = 0.078f;
    desc._NoiseMix = 0.45f;
    desc._Volume = 0.75f;
    _Audio.RegisterProceduralSound("fps_land", desc);
  }

  // 7. Fire
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Square;
    desc._StartFrequency = 880.0f;
    desc._EndFrequency = 110.0f;
    desc._FrequencyExp = 2.5f;
    desc._Duration = 0.16f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.060f;
    desc._SustainLevel = 0.2f;
    desc._ReleaseTime = 0.098f;
    desc._NoiseMix = 0.50f;
    desc._Volume = 0.85f;
    _Audio.RegisterProceduralSound("fps_fire", desc);
  }

  // File-based asset overrides (if present on disk)
  const char * soundNames[] = {
    "ui_nav", "ui_confirm", "ui_back",
    "fps_step", "fps_jump", "fps_land", "fps_fire"
  };
  for ( const char * name : soundNames )
  {
    const std::string path = std::string("Assets/Sounds/") + name + ".wav";
    if ( std::filesystem::exists(path) )
      _Audio.RegisterSoundFile(name, path);
  }

  // Per-weapon and projectile sound assets
  struct AssetSoundEntry
  {
    const char * _Id;
    const char * _AssetRelPath;
  };
  const AssetSoundEntry weaponSounds[] =
  {
    { "fps_fire",                 "overwatch_junkrats_grenade_launcher/sound/grenade_launcher-cannon-shot.mp3" },
    { "fps_fire_launcher",        "overwatch_junkrats_grenade_launcher/sound/grenade_launcher-cannon-shot.mp3" },
    { "fps_projectile_bounce",    "overwatch_junkrats_grenade_launcher/sound/grenade_launcher-bounce.mp3" },
    { "fps_projectile_explosion", "overwatch_junkrats_grenade_launcher/sound/grenade_launcher-explosion.mp3" },
    { "fps_fire_mark23",          "mark_23__animated_free/sound/mark_23-pistol-shoot-short-reverb.mp3" },
    { "fps_reload_mark23",        "mark_23__animated_free/sound/mark_23-gun-reload.mp3" },
    { "fps_fire_carbine",         "fps_animated_carbine/sound/carbine-submachine-gun.mp3" },
    { "fps_reload_carbine",       "fps_animated_carbine/sound/carbine-reload-sound.mp3" }
  };
  for ( const AssetSoundEntry & entry : weaponSounds )
  {
    const std::string assetPath = PathUtils::GetAssetPath(entry._AssetRelPath);
    if ( !assetPath.empty() && std::filesystem::exists(assetPath) )
      _Audio.RegisterSoundFile(entry._Id, assetPath);
  }
}

// InitializeUI
// ----------------------------------------------------------------------------
int FpsGameApp::InitializeUI()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  ImGui::StyleColorsDark();
  _Hud.Initialize();

  const char * glslVersion = "#version 410";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  return 0;
}

// ----------------------------------------------------------------------------
// MakeEditorContext
// ----------------------------------------------------------------------------
FpsGameEditorContext FpsGameApp::MakeEditorContext()
{
  return FpsGameEditorContext(_MainWindow.get(), _Scene.get(), _Renderer.get(), _Settings, _KeyInput, _GameSettings, _GameWorld, _SceneBinding, _Map, _MapPath, _MapLoaded, _ReloadScene, _ReloadBoids,
                              _FrameRate, _FrameTime, _DeltaTime, _NbRenderedFrames,
                              _DebugMode, _DeferredDebugView, _DeferredShowWires, _SoftwareDebugView, _SoftwareShowWires,
                              _DisplayedCpuTimings);
}

// ----------------------------------------------------------------------------
// MakeHudContext
// ----------------------------------------------------------------------------
FpsGameHudContext FpsGameApp::MakeHudContext()
{
  FpsGameHudContext context;
  context._AppState = _AppState;
  context._Paused = _Paused;
  context._EditorEnabled = _Editor.IsEnabled();
  context._DevPanelsVisible = _ShowDebugPanel;
  context._GameSettings = &_GameSettings;
  context._GameWorld = &_GameWorld;
  context._Scene = _Scene.get();
  context._Renderer = _Renderer.get();
  context._RenderSettings = &_Settings;
  context._Audio = &_Audio;
  context._MenuInput = &_MenuInput;
  context._Gamepad = &_GamepadInput;
  context._GamepadLookSensitivity = &_GamepadLookSensitivity;
  context._MapName = std::filesystem::path(_MapPath).filename().string();
  context._MapPath = _MapPath;
  context._FrameRate = _FrameRate;
  context._DeltaTime = _DeltaTime;
  context._NbRenderedFrames = _NbRenderedFrames;
  context._PhotoModeState = _PhotoModeState;
  context._PhotoSettings = &_PhotoSettings;
  context._PhotoCurrentPass = _PhotoRenderer ? static_cast<int>(_PhotoRenderer -> GetNbCompleteFrames()) : 0;
  context._PhotoTargetPasses = std::max(1, _PhotoSettings._TargetPasses);
  context._PhotoSavedPath = _LastPhotoOutputPath.string();
  return context;
}

// ----------------------------------------------------------------------------
// SyncPlayerView
// ----------------------------------------------------------------------------
void FpsGameApp::SyncPlayerView()
{
  if ( !_Scene )
    return;

  _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings, _Editor.IsEnabled());
  bool meshDataDirty = false;
  _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings, &meshDataDirty);
  if ( _Renderer )
  {
    _Renderer -> Notify(DirtyState::SceneCamera);
    if ( meshDataDirty )
      _Renderer -> Notify(DirtyState::SceneMeshData);
    _Renderer -> Notify(DirtyState::SceneInstances);
  }

  const Camera & cam = _Scene->GetCamera();
  _Audio.UpdateListener( cam.GetPos(), cam.GetForward(), cam.GetUp() );
}

#if 0
{
  if ( !_Scene )
    return;

  _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings, _Editor.IsEnabled());
  bool meshDataDirty = false;
  _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings, &meshDataDirty);
  if ( _Renderer )
  {
    _Renderer -> Notify(DirtyState::SceneCamera);
    if ( meshDataDirty )
      _Renderer -> Notify(DirtyState::SceneMeshData);
    _Renderer -> Notify(DirtyState::SceneInstances);
  }
}

// ----------------------------------------------------------------------------
#endif

// StartGame
// ----------------------------------------------------------------------------
void FpsGameApp::StartGame()
{
  if ( FpsPhotoModeState::Inactive != _PhotoModeState )
    ExitPhotoMode();

  _AppState = FpsAppState::InGame;
  _Paused = false;
  _Hud.SetSettingsOpen(false);
  _GameWorld.Reset(_GameSettings);
  SyncPlayerView();
  SetMouseCaptured(true);
}

// ----------------------------------------------------------------------------
// PauseGame
// ----------------------------------------------------------------------------
void FpsGameApp::PauseGame()
{
  _Paused = true;
  _MenuInput.Clear();
  SetMouseCaptured(false);
  _Audio.PlaySound2D("ui_back", 0.8f, 1.0f);
}

// ----------------------------------------------------------------------------
// ResumeGame
// ----------------------------------------------------------------------------
void FpsGameApp::ResumeGame()
{
  _Paused = false;
  _Hud.SetSettingsOpen(false);
  SetMouseCaptured(!_Editor.IsEnabled() && ( FpsPhotoModeState::Inactive == _PhotoModeState ));
  _Audio.PlaySound2D("ui_confirm", 0.8f, 1.0f);
}

// ----------------------------------------------------------------------------
// ReturnToTitle
// ----------------------------------------------------------------------------
void FpsGameApp::ReturnToTitle( FpsMenuPage iPage )
{
  if ( FpsPhotoModeState::Inactive != _PhotoModeState )
    ExitPhotoMode();

  if ( _Editor.IsEnabled() )
    SetEditorMode(false);

  _AppState = FpsAppState::TitleScreen;
  _Paused = false;
  _Hud.SetSettingsOpen(false);
  _Hud.SetMenuPage(iPage);
  SetMouseCaptured(false);

  if ( _Scene )
  {
    _SceneBinding.SetViewWeaponVisible(*_Scene, false);
    _SceneBinding.SyncTitleCamera(*_Scene, _GameWorld, _GameSettings, static_cast<float>(glfwGetTime()));
    if ( _Renderer )
    {
      _Renderer -> Notify(DirtyState::SceneCamera);
      _Renderer -> Notify(DirtyState::SceneInstances);
    }
  }
}

// ----------------------------------------------------------------------------
// RequestMap
// ----------------------------------------------------------------------------
void FpsGameApp::RequestMap( const std::string & iMapPath )
{
  const std::string resolved = std::filesystem::path(iMapPath).is_absolute() ? iMapPath : PathUtils::GetAssetPath(iMapPath);
  if ( _MapLoaded && ( resolved == _MapPath ) )
    return;

  _MapPath = resolved;
  _MapLoaded = false;
  _ReloadScene = true;
}

// ----------------------------------------------------------------------------
// SetDevPanelsVisible
// ----------------------------------------------------------------------------
void FpsGameApp::SetDevPanelsVisible( bool iVisible )
{
  _ShowMenuBar = iVisible;
  _ShowDebugPanel = iVisible;
  _ShowRenderSettingsPanel = iVisible;
  _ShowRenderStatsPanel = iVisible;
}

// ----------------------------------------------------------------------------
// UpdateGamepadMenuInput
// ----------------------------------------------------------------------------
void FpsGameApp::UpdateGamepadMenuInput()
{
  if ( !_GamepadInput.IsConnected() )
    return;

  const bool menuActive = ( FpsAppState::TitleScreen == _AppState ) || _Paused;
  if ( menuActive )
  {
    if ( _GamepadInput.IsMenuNavRepeated(GamepadDir::Up) )
      _MenuInput._NavUp = true;
    if ( _GamepadInput.IsMenuNavRepeated(GamepadDir::Down) )
      _MenuInput._NavDown = true;
    if ( _GamepadInput.IsMenuNavRepeated(GamepadDir::Left) )
      _MenuInput._NavLeft = true;
    if ( _GamepadInput.IsMenuNavRepeated(GamepadDir::Right) )
      _MenuInput._NavRight = true;

    if ( _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_A) )
      _MenuInput._Confirm = true;
    if ( _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_B) )
      _MenuInput._Back = true;
    if ( _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER) )
      _MenuInput._PrevTab = true;
    if ( _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER) )
      _MenuInput._NextTab = true;

    if ( _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_START) )
    {
      if ( FpsAppState::TitleScreen == _AppState )
        _MenuInput._Confirm = true;
      else
        _MenuInput._Back = true;
    }
  }
  else if ( ( FpsAppState::InGame == _AppState ) && !_Editor.IsEnabled() )
  {
    if ( _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_START) )
      _PauseRequested = true;
  }
}

// ----------------------------------------------------------------------------
// ApplyHudRequests
// ----------------------------------------------------------------------------
void FpsGameApp::ApplyHudRequests( FpsGameHudContext & ioContext )
{
  if ( ioContext._WeaponDirty && ( FpsAppState::InGame == _AppState ) )
    SyncPlayerView();
  if ( ioContext._CameraDirty && ( FpsAppState::InGame == _AppState ) )
    SyncPlayerView();
  if ( ioContext._RenderSettingsDirty && _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);
  if ( ioContext._RequestReloadScene )
    _ReloadScene = true;
  if ( ioContext._RequestToggleDevPanels )
    SetDevPanelsVisible(!_ShowDebugPanel);
  if ( ioContext._RequestRendererMode >= 0 )
    SetRendererMode(static_cast<FpsRendererMode>(ioContext._RequestRendererMode));

  if ( ioContext._RequestPhotoMode )
    EnterPhotoMode();
  if ( ioContext._RequestResetPhotoCamera )
    ResetPhotoCameraToPlayer();
  if ( ioContext._RequestCapturePhoto )
    StartPhotoCapture();
  if ( ioContext._RequestCancelPhotoCapture )
    CancelPhotoCapture();
  if ( ioContext._RequestRetakePhoto )
    ReturnToPhotoSetup();
  if ( ioContext._RequestExitPhotoPreview )
    ExitPhotoMode();

  if ( !ioContext._RequestMapPath.empty() )
    RequestMap(ioContext._RequestMapPath);

  if ( ioContext._RequestBrowseMap )
  {
    std::filesystem::path selectedPath;
    std::string error;
    const std::filesystem::path initialDirectory(PathUtils::GetAssetPath(""));
    if ( NativeFileDialogResult::Selected == OpenFileDialog("fpsmap", initialDirectory, selectedPath, error) )
    {
      std::error_code ec;
      selectedPath = std::filesystem::absolute(selectedPath, ec).lexically_normal();
      if ( !ec )
        RequestMap(selectedPath.string());
      else
        ioContext._RequestStartGame = false;
    }
    else
      ioContext._RequestStartGame = false;
  }

  if ( ioContext._RequestStartGame )
    StartGame();
  if ( ioContext._RequestResume )
    ResumeGame();
  if ( ioContext._RequestRespawn )
  {
    _GameWorld.Reset(_GameSettings);
    SyncPlayerView();
    ResumeGame();
  }
  if ( ioContext._RequestMapSelect )
    ReturnToTitle(FpsMenuPage::MapSelect);
  if ( ioContext._RequestTitle )
    ReturnToTitle(FpsMenuPage::MainMenu);

  if ( ioContext._RequestEditor && !_Editor.IsEnabled() )
  {
    if ( FpsAppState::TitleScreen == _AppState )
      StartGame();
    else
      ResumeGame();
    SetEditorMode(true);
  }

  if ( ioContext._RequestExit )
    glfwSetWindowShouldClose(_MainWindow.get(), GLFW_TRUE);
}

// ----------------------------------------------------------------------------
// SetEditorMode
// ----------------------------------------------------------------------------
bool FpsGameApp::SetEditorMode( bool iEnabled )
{
  if ( iEnabled && ( FpsPhotoModeState::Inactive != _PhotoModeState ) )
    ExitPhotoMode();

  FpsGameEditorContext editorContext = MakeEditorContext();
  if ( !_Editor.SetMode(editorContext, iEnabled) )
    return false;

  _HasLastMousePos = false;
  if ( _Editor.IsEnabled() )
    SetMouseCaptured(false);

  return true;
}

// ----------------------------------------------------------------------------
// EnterPhotoMode
// ----------------------------------------------------------------------------
void FpsGameApp::EnterPhotoMode()
{
  if ( ( FpsAppState::InGame != _AppState ) || _Editor.IsEnabled() )
    return;

  if ( FpsPhotoModeState::Inactive != _PhotoModeState )
    return;

  _Paused = false;
  _Hud.SetSettingsOpen(false);
  SetMouseCaptured(false);

  _SavedPlayerBeforePhoto = _GameWorld.GetPlayer();
  _SavedFreeLookBeforePhoto = _GameSettings._FreeLook;
  _SavedShowViewWeaponBeforePhoto = _GameSettings._ShowViewWeapon;
  _SavedCameraFOVBeforePhoto = _GameSettings._CameraFOV;
  _SavedToneMappingBeforePhoto = _Settings._ToneMapping;
  _SavedExposureBeforePhoto = _Settings._Exposure;
  _SavedGammaBeforePhoto = _Settings._Gamma;

  _GameWorld.GetPlayer()._Velocity = Vec3(0.f);
  _GameSettings._FreeLook = true;
  _GameSettings._ShowViewWeapon = false;

  _PhotoRenderer.reset();
  _PhotoModeState = FpsPhotoModeState::Setup;
  SyncPlayerView();
}

// ----------------------------------------------------------------------------
// ExitPhotoMode
// ----------------------------------------------------------------------------
void FpsGameApp::ExitPhotoMode()
{
  if ( FpsPhotoModeState::Inactive == _PhotoModeState )
    return;

  _PhotoRenderer.reset();
  _PhotoModeState = FpsPhotoModeState::Inactive;

  _GameWorld.GetPlayer() = _SavedPlayerBeforePhoto;
  _GameSettings._FreeLook = _SavedFreeLookBeforePhoto;
  _GameSettings._ShowViewWeapon = _SavedShowViewWeaponBeforePhoto;
  _GameSettings._CameraFOV = _SavedCameraFOVBeforePhoto;
  _Settings._ToneMapping = _SavedToneMappingBeforePhoto;
  _Settings._Exposure = _SavedExposureBeforePhoto;
  _Settings._Gamma = _SavedGammaBeforePhoto;

  SyncPlayerView();
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);

  if ( ( FpsAppState::InGame == _AppState ) && !_Paused && !_Editor.IsEnabled() )
    SetMouseCaptured(true);
}

// ----------------------------------------------------------------------------
// ResetPhotoCameraToPlayer
// ----------------------------------------------------------------------------
void FpsGameApp::ResetPhotoCameraToPlayer()
{
  if ( FpsPhotoModeState::Setup != _PhotoModeState )
    return;

  FpsPlayer & player = _GameWorld.GetPlayer();
  player._Position = _SavedPlayerBeforePhoto._Position;
  player._Velocity = Vec3(0.f);
  player._Yaw = _SavedPlayerBeforePhoto._Yaw;
  player._Pitch = _SavedPlayerBeforePhoto._Pitch;
  _GameSettings._CameraFOV = _SavedCameraFOVBeforePhoto;

  SyncPlayerView();
}

// ----------------------------------------------------------------------------
// StartPhotoCapture
// ----------------------------------------------------------------------------
void FpsGameApp::StartPhotoCapture()
{
  if ( !_Scene || ( FpsPhotoModeState::Setup != _PhotoModeState ) )
    return;

  _PhotoRenderSettings = _Settings;
  _PhotoRenderSettings._WindowResolution = _Settings._WindowResolution;
  _PhotoRenderSettings._RenderResolution = _Settings._WindowResolution;
  _PhotoRenderSettings._RenderScale = 100;
  _PhotoRenderSettings._LowResRatio = 1.f;
  _PhotoRenderSettings._TiledRendering = false;
  _PhotoRenderSettings._Accumulate = true;
  _PhotoRenderSettings._NbSamplesPerPixel = std::max(1, _PhotoSettings._SamplesPerPixel);
  _PhotoRenderSettings._Bounces = std::max(1, _PhotoSettings._Bounces);
  _PhotoRenderSettings._DenoisingMethod = _PhotoSettings._DenoisingMethod;
  _PhotoRenderSettings._Denoise = false;

  _PhotoRenderer.reset(new PathTracer(*_Scene, _PhotoRenderSettings));
  if ( !_PhotoRenderer || ( 0 != _PhotoRenderer -> Initialize() ) )
  {
    _PhotoRenderer.reset();
    return;
  }

  _PhotoCaptureWarmupDone = false;
  _PhotoModeState = FpsPhotoModeState::Capturing;
}

// ----------------------------------------------------------------------------
// CancelPhotoCapture
// ----------------------------------------------------------------------------
void FpsGameApp::CancelPhotoCapture()
{
  if ( FpsPhotoModeState::Capturing != _PhotoModeState )
    return;

  _PhotoRenderer.reset();
  _PhotoModeState = FpsPhotoModeState::Setup;
  SyncPlayerView();
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);
}

// ----------------------------------------------------------------------------
// ReturnToPhotoSetup
// ----------------------------------------------------------------------------
void FpsGameApp::ReturnToPhotoSetup()
{
  if ( FpsPhotoModeState::Preview != _PhotoModeState )
    return;

  _PhotoRenderer.reset();
  _PhotoModeState = FpsPhotoModeState::Setup;
  SyncPlayerView();
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);
}

// ----------------------------------------------------------------------------
// UpdatePhotoCapture
// ----------------------------------------------------------------------------
int FpsGameApp::UpdatePhotoCapture()
{
  if ( !_PhotoRenderer )
    return 1;

  const int targetPasses = std::max(1, _PhotoSettings._TargetPasses);
  const int completedBefore = static_cast<int>(_PhotoRenderer -> GetNbCompleteFrames());
  const bool willFinishThisFrame = _PhotoCaptureWarmupDone && ( ( completedBefore + 1 ) >= targetPasses );
  if ( willFinishThisFrame && _PhotoSettings._Denoise )
    _PhotoRenderSettings._Denoise = true;

  BeginCpuTiming(CpuRendererUpdate);
  _PhotoRenderer -> Update();
  EndCpuTiming(CpuRendererUpdate);

  BeginCpuTiming(CpuRenderToTexture);
  _PhotoRenderer -> RenderToTexture();
  EndCpuTiming(CpuRenderToTexture);

  BeginCpuTiming(CpuRenderToScreen);
  _PhotoRenderer -> RenderToScreen();
  EndCpuTiming(CpuRenderToScreen);

  _PhotoRenderer -> Done();
  _PhotoCaptureWarmupDone = true;

  if ( static_cast<int>(_PhotoRenderer -> GetNbCompleteFrames()) >= targetPasses )
  {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime = {};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    char timeBuf[32] = {};
    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", &localTime);

    std::string mapStem = std::filesystem::path(_MapPath).stem().string();
    if ( mapStem.empty() )
      mapStem = "arena";

    const std::string filename = "Photo_" + mapStem + "_" + std::string(timeBuf) + ".png";
    const std::filesystem::path outPath(PathUtils::GetScreenshotPath(filename));
    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);

    BeginCpuTiming(CpuRenderToFile);
    if ( 0 == _PhotoRenderer -> RenderToFile(outPath) )
    {
      _LastPhotoOutputPath = outPath;
      std::cout << "Photo saved to: " << _LastPhotoOutputPath.string() << std::endl;
    }
    else
    {
      _LastPhotoOutputPath.clear();
      std::cout << "Failed to save photo to: " << outPath.string() << std::endl;
    }
    EndCpuTiming(CpuRenderToFile);

    _PhotoModeState = FpsPhotoModeState::Preview;
  }
  else
    SetCpuTimingEnabled(CpuRenderToFile, false);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int FpsGameApp::InitializeScene()
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

  if ( FpsAppState::TitleScreen == _AppState )
  {
    _SceneBinding.SetViewWeaponVisible(*_Scene, false);
    _SceneBinding.SyncTitleCamera(*_Scene, _GameWorld, _GameSettings, static_cast<float>(glfwGetTime()));
  }
  return 0;
}

// ----------------------------------------------------------------------------
// RefreshPropCollisionColliders
// ----------------------------------------------------------------------------
int FpsGameApp::RefreshPropCollisionColliders()
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
int FpsGameApp::InitializeMapBoids( bool iResetSimulation )
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
int FpsGameApp::RefreshMapBoids()
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
int FpsGameApp::InitializeRenderer()
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
  if ( FpsRendererMode::Software == _GameSettings._RendererMode )
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
int FpsGameApp::ProcessInput()
{
  double curMouseX = 0., curMouseY = 0.;
  glfwGetCursorPos(_MainWindow.get(), &curMouseX, &curMouseY);

  _GamepadInput.Poll(static_cast<float>(std::min(_DeltaTime, 0.1)));
  UpdateGamepadMenuInput();

  // Dear ImGui gamepad navigation is only needed for the options panel and the editor
  ImGuiIO & uiIO = ImGui::GetIO();
  const bool optionsOpen = ( ( FpsAppState::TitleScreen == _AppState ) && ( FpsMenuPage::Options == _Hud.GetMenuPage() ) )
                        || ( _Paused && _Hud.IsSettingsOpen() );
  if ( optionsOpen || _Editor.IsEnabled() )
    uiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  else
    uiIO.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;

  if ( !uiIO.WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_F4) )
    SetDevPanelsVisible(!_ShowDebugPanel);

  if ( _PauseRequested )
  {
    _PauseRequested = false;
    if ( ( FpsAppState::InGame == _AppState ) && !_Paused && !_Editor.IsEnabled() )
      PauseGame();
  }

  if ( _Editor.IsEnabled() && !uiIO.WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) )
    SetEditorMode(false);

  // Title screen and pause menu: gameplay input is suspended
  if ( ( FpsAppState::TitleScreen == _AppState ) || _Paused )
  {
    if ( !uiIO.WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_F3) )
    {
      if ( FpsAppState::TitleScreen == _AppState )
        StartGame();
      else
        ResumeGame();
      SetEditorMode(true);
    }

    if ( ( FpsAppState::TitleScreen == _AppState ) && _Scene )
    {
      _SceneBinding.SyncTitleCamera(*_Scene, _GameWorld, _GameSettings, static_cast<float>(glfwGetTime()));
      if ( _Renderer )
        _Renderer -> Notify(DirtyState::SceneCamera);
    }

    _LastMouseX = curMouseX;
    _LastMouseY = curMouseY;
    _KeyInput.ClearLastEvents();
    _MouseInput.ClearLastEvents(curMouseX, curMouseY);

    glfwPollEvents();
    return 0;
  }

  if ( FpsPhotoModeState::Inactive != _PhotoModeState )
  {
    if ( FpsPhotoModeState::Setup == _PhotoModeState )
    {
      if ( !uiIO.WantCaptureKeyboard )
      {
        if ( _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) || _KeyInput.IsKeyReleased(GLFW_KEY_L) || _KeyInput.IsKeyReleased(GLFW_KEY_F12) )
        {
          ExitPhotoMode();
          _LastMouseX = curMouseX;
          _LastMouseY = curMouseY;
          _KeyInput.ClearLastEvents();
          _MouseInput.ClearLastEvents(curMouseX, curMouseY);
          glfwPollEvents();
          return 0;
        }
        if ( _KeyInput.IsKeyReleased(GLFW_KEY_ENTER) || _KeyInput.IsKeyReleased(GLFW_KEY_KP_ENTER) )
        {
          StartPhotoCapture();
          _LastMouseX = curMouseX;
          _LastMouseY = curMouseY;
          _KeyInput.ClearLastEvents();
          _MouseInput.ClearLastEvents(curMouseX, curMouseY);
          glfwPollEvents();
          return 0;
        }
        if ( _KeyInput.IsKeyReleased(GLFW_KEY_R) )
          ResetPhotoCameraToPlayer();
      }

      const bool mouseDragHeld = !uiIO.WantCaptureMouse
                              && ( ( GLFW_PRESS == glfwGetMouseButton(_MainWindow.get(), GLFW_MOUSE_BUTTON_RIGHT) )
                                || ( GLFW_PRESS == glfwGetMouseButton(_MainWindow.get(), GLFW_MOUSE_BUTTON_LEFT) ) );

      FpsPlayer & player = _GameWorld.GetPlayer();
      bool cameraMoved = false;

      if ( mouseDragHeld )
      {
        if ( _HasLastMousePos )
        {
          const float deltaX = static_cast<float>(curMouseX - _LastMouseX);
          const float deltaY = static_cast<float>(curMouseY - _LastMouseY);
          if ( ( std::abs(deltaX) > 0.f ) || ( std::abs(deltaY) > 0.f ) )
          {
            player._Yaw += deltaX * _GameSettings._MouseSensitivity;
            player._Pitch -= deltaY * _GameSettings._MouseSensitivity;
            player._Pitch = MathUtil::Clamp(player._Pitch, -89.f, 89.f);
            cameraMoved = true;
          }
        }
        _HasLastMousePos = true;
      }
      else
        _HasLastMousePos = false;

      if ( !uiIO.WantTextInput && !uiIO.WantCaptureKeyboard )
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
        if ( _KeyInput.IsKeyDown(GLFW_KEY_SPACE) || _KeyInput.IsKeyDown(GLFW_KEY_E) )
          move += up;
        if ( _KeyInput.IsKeyDown(GLFW_KEY_LEFT_CONTROL) || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_CONTROL) || _KeyInput.IsKeyDown(GLFW_KEY_Q) )
          move -= up;

        if ( _GamepadInput.IsConnected() )
        {
          const float deadzone = _GamepadInput.GetStickDeadzone();
          const float leftX = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_LEFT_X, deadzone);
          const float leftY = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_LEFT_Y, deadzone);
          const float rightX = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_RIGHT_X, deadzone);
          const float rightY = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y, deadzone);

          move += forward * ( -leftY ) + right * leftX;
          if ( ( std::abs(rightX) > 0.f ) || ( std::abs(rightY) > 0.f ) )
          {
            player._Yaw += rightX * _GamepadLookSensitivity * static_cast<float>(_DeltaTime);
            player._Pitch -= rightY * _GamepadLookSensitivity * static_cast<float>(_DeltaTime);
            player._Pitch = MathUtil::Clamp(player._Pitch, -89.f, 89.f);
            cameraMoved = true;
          }
        }

        if ( glm::length(move) > EPSILON )
        {
          const float speed = ( _KeyInput.IsKeyDown(GLFW_KEY_LEFT_SHIFT) || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_SHIFT) )
                            ? _GameSettings._SprintSpeed
                            : _GameSettings._MoveSpeed;
          const float deltaTime = std::min(static_cast<float>(_DeltaTime), 0.05f);
          player._Position += glm::normalize(move) * speed * deltaTime;
          cameraMoved = true;
        }
      }

      if ( cameraMoved )
        SyncPlayerView();
    }
    else if ( FpsPhotoModeState::Capturing == _PhotoModeState )
    {
      if ( !uiIO.WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) )
        CancelPhotoCapture();
    }
    else if ( FpsPhotoModeState::Preview == _PhotoModeState )
    {
      if ( !uiIO.WantCaptureKeyboard )
      {
        if ( _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) || _KeyInput.IsKeyReleased(GLFW_KEY_ENTER) || _KeyInput.IsKeyReleased(GLFW_KEY_KP_ENTER) )
          ExitPhotoMode();
        else if ( _KeyInput.IsKeyReleased(GLFW_KEY_BACKSPACE) )
          ReturnToPhotoSetup();
      }
    }

    _LastMouseX = curMouseX;
    _LastMouseY = curMouseY;
    _KeyInput.ClearLastEvents();
    _MouseInput.ClearLastEvents(curMouseX, curMouseY);
    glfwPollEvents();
    return 0;
  }

  if ( !ImGui::GetIO().WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_F2) )
    _GameSettings._FreeLook = !_GameSettings._FreeLook;

  if ( !ImGui::GetIO().WantCaptureKeyboard && _KeyInput.IsKeyReleased(GLFW_KEY_F3) )
    SetEditorMode(!_Editor.IsEnabled());

  if ( _Editor.IsEnabled() )
  {
    if ( !ImGui::GetIO().WantCaptureKeyboard )
    {
      FpsRendererMode requestedRendererMode = _GameSettings._RendererMode;
      if ( _KeyInput.IsKeyDown(GLFW_KEY_J) )
        requestedRendererMode = FpsRendererMode::Deferred;
      else if ( _KeyInput.IsKeyDown(GLFW_KEY_K) )
        requestedRendererMode = FpsRendererMode::Software;

      if ( requestedRendererMode != _GameSettings._RendererMode )
      {
        SetRendererMode(requestedRendererMode);
      }

      if ( _KeyInput.IsKeyReleased(GLFW_KEY_DELETE) )
      {
        FpsGameEditorContext editorContext = MakeEditorContext();
        _Editor.DeleteSelectedItem(editorContext);
      }
    }

    const bool rightMouseHeld = !ImGui::GetIO().WantCaptureMouse
                             && ( GLFW_PRESS == glfwGetMouseButton(_MainWindow.get(), GLFW_MOUSE_BUTTON_RIGHT) );
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
        _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings, _Editor.IsEnabled());
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
      if ( !_GameSettings._FreeLook )
      {
        input._FirePressed = true;
        input._FireHeld = true;
      }
    }

    double scrollX = 0., scrollY = 0.;
    if ( _MouseInput.IsScrolled(scrollX, scrollY) )
    {
      if ( scrollY > 0.05 )
        input._WeaponSwitchDelta = -1;
      else if ( scrollY < -0.05 )
        input._WeaponSwitchDelta = 1;
    }
  }

  if ( !ImGui::GetIO().WantCaptureKeyboard )
  {
    if ( _KeyInput.IsKeyReleased(GLFW_KEY_PAGE_UP) )
      _GameSettings._MoveSpeed = std::clamp(_GameSettings._MoveSpeed + 5.f, 1.f, 100.f);
    if ( _KeyInput.IsKeyReleased(GLFW_KEY_PAGE_DOWN) )
      _GameSettings._MoveSpeed = std::clamp(_GameSettings._MoveSpeed - 5.f, 1.f, 100.f);

    if ( _KeyInput.IsKeyReleased(GLFW_KEY_1) )
      input._RequestedWeaponSlot = 0;
    else if ( _KeyInput.IsKeyReleased(GLFW_KEY_2) )
      input._RequestedWeaponSlot = 1;
    else if ( _KeyInput.IsKeyReleased(GLFW_KEY_3) )
      input._RequestedWeaponSlot = 2;

    if ( _KeyInput.IsKeyReleased(GLFW_KEY_L) || _KeyInput.IsKeyReleased(GLFW_KEY_F12) )
    {
      EnterPhotoMode();
      _LastMouseX = curMouseX;
      _LastMouseY = curMouseY;
      _KeyInput.ClearLastEvents();
      _MouseInput.ClearLastEvents(curMouseX, curMouseY);
      glfwPollEvents();
      return 0;
    }

    FpsRendererMode requestedRendererMode = _GameSettings._RendererMode;
    if ( _KeyInput.IsKeyDown(GLFW_KEY_J) )
      requestedRendererMode = FpsRendererMode::Deferred;
    else if ( _KeyInput.IsKeyDown(GLFW_KEY_K) )
      requestedRendererMode = FpsRendererMode::Software;

    if ( requestedRendererMode != _GameSettings._RendererMode )
    {
      SetRendererMode(requestedRendererMode);
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

    if ( 0 != _GameWorld.GetActiveWeaponSlot() )
    {
      input._ReloadPressed = _KeyInput.IsKeyReleased(GLFW_KEY_R);
      input._ResetPressed = _KeyInput.IsKeyReleased(GLFW_KEY_BACKSPACE);
    }
    else
    {
      input._ResetPressed = _KeyInput.IsKeyReleased(GLFW_KEY_R) || _KeyInput.IsKeyReleased(GLFW_KEY_BACKSPACE);
    }
  }

  if ( _GamepadInput.IsConnected() && !ImGui::GetIO().WantTextInput )
  {
    const float deadzone = _GamepadInput.GetStickDeadzone();
    const float leftX = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_LEFT_X, deadzone);
    const float leftY = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_LEFT_Y, deadzone);
    const float rightX = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_RIGHT_X, deadzone);
    const float rightY = _GamepadInput.GetAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y, deadzone);

    input._MoveForward  = input._MoveForward  || ( leftY < -0.3f );
    input._MoveBackward = input._MoveBackward || ( leftY >  0.3f );
    input._MoveLeft     = input._MoveLeft     || ( leftX < -0.3f );
    input._MoveRight    = input._MoveRight    || ( leftX >  0.3f );
    input._Sprint       = input._Sprint || _GamepadInput.IsButtonDown(GLFW_GAMEPAD_BUTTON_LEFT_THUMB)
                                        || _GamepadInput.IsButtonDown(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER);
    if ( !_GameSettings._FreeLook )
      input._JumpPressed = input._JumpPressed || _GamepadInput.IsButtonDown(GLFW_GAMEPAD_BUTTON_A);
    input._ResetPressed = input._ResetPressed || _GamepadInput.IsButtonPressed(GLFW_GAMEPAD_BUTTON_Y);

    if ( !_GameSettings._FreeLook && _GamepadInput.IsRightTriggerDown() )
    {
      input._FirePressed = true;
      input._FireHeld = true;
    }

    const float lookScale = _GamepadLookSensitivity * static_cast<float>(_DeltaTime) / std::max(_GameSettings._MouseSensitivity, 0.001f);
    input._MouseDeltaX += rightX * lookScale;
    input._MouseDeltaY += rightY * lookScale;
  }

  if ( _MouseCaptured )
  {
    if ( _HasLastMousePos )
    {
      input._MouseDeltaX += static_cast<float>(curMouseX - _LastMouseX);
      input._MouseDeltaY += static_cast<float>(curMouseY - _LastMouseY);
    }
    _HasLastMousePos = true;
  }

  _LastMouseX = curMouseX;
  _LastMouseY = curMouseY;

  if ( 0 != _GameWorld.Update(static_cast<float>(_DeltaTime), input, _GameSettings) )
    return 1;

  _GameSettings._ActiveWeaponSlot = _GameWorld.GetActiveWeaponSlot();

  if ( 0 != _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings, _Editor.IsEnabled()) )
    return 1;

  if ( _Renderer )
    _Renderer -> Notify(DirtyState::SceneCamera);

  // 3D Audio player movement and combat cues
  const FpsPlayer & player = _GameWorld.GetPlayer();

  if ( _Scene )
  {
    const Camera & cam = _Scene->GetCamera();
    _Audio.UpdateListener( cam.GetPos(), cam.GetForward(), cam.GetUp(), player._Velocity );
  }

  if ( player._Grounded && !_PrevGrounded )
  {
    _Audio.PlaySound3D( "fps_land", player._Position, 0.75f, 1.0f );
  }
  else if ( !player._Grounded && _PrevGrounded && ( player._Velocity.y > 0.5f ) )
  {
    _Audio.PlaySound3D( "fps_jump", player._Position, 0.70f, 1.0f );
  }
  else if ( player._Grounded && ( std::sqrt( player._Velocity.x * player._Velocity.x + player._Velocity.z * player._Velocity.z ) > 0.8f ) )
  {
    _FootstepTimer += static_cast<float>( _DeltaTime );
    if ( _FootstepTimer >= 0.38f )
    {
      _FootstepTimer = 0.0f;
      float pitch = 0.95f + ( ( rand() % 10 ) * 0.015f );
      _Audio.PlaySound3D( "fps_step", player._Position, 0.40f, pitch );
    }
  }
  else
  {
    _FootstepTimer = 0.20f;
  }
  _PrevGrounded = player._Grounded;

  int firedSlot = -1;
  if ( _GameWorld.ConsumeWeaponFired(&firedSlot) )
    _Audio.PlaySound2D( GetWeaponFireSoundId(firedSlot), 0.85f, 1.0f );

  int reloadSlot = -1;
  if ( _GameWorld.ConsumeWeaponReloaded(&reloadSlot) )
  {
    if ( const char * reloadSound = GetWeaponReloadSoundId(reloadSlot) )
      _Audio.PlaySound2D( reloadSound, 0.85f, 1.0f );
  }

  std::vector<Vec3> bouncePositions;
  std::vector<Vec3> explosionPositions;
  _GameWorld.ConsumeProjectileSoundEvents(bouncePositions, explosionPositions);
  for ( const Vec3 & bouncePos : bouncePositions )
    _Audio.PlaySound3D( "fps_projectile_bounce", bouncePos, 0.80f, 1.0f );
  for ( const Vec3 & explosionPos : explosionPositions )
    _Audio.PlaySound3D( "fps_projectile_explosion", explosionPos, 0.95f, 1.0f );

  const bool projectilesDirty = _GameWorld.ConsumeProjectilesDirty();
  if ( projectilesDirty || ( _GameSettings._ShowViewWeapon && _SceneBinding.HasViewWeapon() ) )
  {
    bool meshDataDirty = false;
    if ( 0 != _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings, &meshDataDirty) )
      return 1;

    if ( _Renderer )
    {
      if ( meshDataDirty )
        _Renderer -> Notify(DirtyState::SceneMeshData);
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
int FpsGameApp::UpdateGame()
{
  BeginCpuTiming(CpuUpdateGame);

  if ( _ReloadScene )
  {
    if ( FpsPhotoModeState::Inactive != _PhotoModeState )
      ExitPhotoMode();

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

  if ( !_Paused && ( FpsPhotoModeState::Inactive == _PhotoModeState ) && ( 0 != UpdateBoids() ) )
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
int FpsGameApp::UpdateBoids()
{
  BeginCpuTiming(CpuUpdateBoids);

  if ( !_Scene || ( FpsPhotoModeState::Inactive != _PhotoModeState ) )
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
// DrawMainMenuBar
// ----------------------------------------------------------------------------
int FpsGameApp::DrawMainMenuBar()
{
  if ( !ImGui::BeginMainMenuBar() )
    return 0;

  if ( ImGui::BeginMenu(Loc::Tr(LocId::Fps_Bar_File)) )
  {
    FpsGameEditorContext editorContext = MakeEditorContext();
    if ( ImGui::MenuItem(_Editor.IsEnabled() ? Loc::Tr(LocId::Fps_Bar_ExitEditor) : Loc::Tr(LocId::Fps_Bar_EnterEditor), "F3") )
      SetEditorMode(!_Editor.IsEnabled());

    ImGui::Separator();
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_OpenMap)) )
      _Editor.LoadMapFromDialog(editorContext);

    if ( _Editor.IsEnabled() )
    {
      ImGui::Separator();
      if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_Save)) )
        _Editor.SaveMap(editorContext);
      if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_SaveAs)) )
        _Editor.SaveMapAsDialog(editorContext);
      if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_LoadProp)) )
        _Editor.LoadPropFromDialog(editorContext);
    }

    ImGui::Separator();
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_Exit)) )
      _ExitToTestSelection = true;
    ImGui::EndMenu();
  }

  if ( ImGui::BeginMenu(Loc::Tr(LocId::Fps_Bar_View)) )
  {
    ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_Debug), nullptr, &_ShowDebugPanel);
    ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_RenderSettings), nullptr, &_ShowRenderSettingsPanel);
    ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_RenderStats), nullptr, &_ShowRenderStatsPanel);

    if ( _Editor.IsEnabled() )
    {
      ImGui::Separator();
      _Editor.DrawViewMenuItems();
    }
    ImGui::EndMenu();
  }

  if ( ImGui::BeginMenu(Loc::Tr(LocId::Fps_Bar_Render)) )
  {
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Renderer_Deferred), nullptr, FpsRendererMode::Deferred == _GameSettings._RendererMode)
      && ( FpsRendererMode::Deferred != _GameSettings._RendererMode ) )
    {
      SetRendererMode(FpsRendererMode::Deferred);
    }
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Renderer_Software), nullptr, FpsRendererMode::Software == _GameSettings._RendererMode)
      && ( FpsRendererMode::Software != _GameSettings._RendererMode ) )
    {
      SetRendererMode(FpsRendererMode::Software);
    }

    ImGui::Separator();
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Key_PhotoMode), "L / F12", FpsPhotoModeState::Inactive != _PhotoModeState, ( FpsAppState::InGame == _AppState ) && !_Editor.IsEnabled()) )
    {
      if ( FpsPhotoModeState::Inactive == _PhotoModeState )
        EnterPhotoMode();
      else
        ExitPhotoMode();
    }

    ImGui::Separator();
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_CaptureImage)) )
    {
      _CaptureOutputPath = "./Test6_" + std::to_string(_NbRenderedFrames) + "frames.png";
      _RenderToFile = true;
    }
    ImGui::EndMenu();
  }

  if ( ImGui::BeginMenu(Loc::Tr(LocId::Option_Language)) )
  {
    Language currentLang = Loc::GetLanguage();
    if ( ImGui::MenuItem("English", nullptr, currentLang == Language::English) )
    {
      Loc::SetLanguage(Language::English);
    }
    if ( ImGui::MenuItem("Français", nullptr, currentLang == Language::French) )
    {
      Loc::SetLanguage(Language::French);
    }
    ImGui::EndMenu();
  }

  if ( ImGui::BeginMenu(Loc::Tr(LocId::Fps_Bar_Help)) )
  {
    if ( ImGui::MenuItem(Loc::Tr(LocId::Fps_Bar_About)) )
      _ShowHelpDialog = true;
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
  return 0;
}

// ----------------------------------------------------------------------------
// DrawHelpUI
// ----------------------------------------------------------------------------
int FpsGameApp::DrawHelpUI()
{
  if ( _ShowHelpDialog )
  {
    ImGui::OpenPopup("Test6 Help");
    _ShowHelpDialog = false;
  }

  if ( ImGui::BeginPopupModal("Test6 Help", nullptr, ImGuiWindowFlags_AlwaysAutoResize) )
  {
    ImGui::TextWrapped("Test6 is the FPS game and map-editing workbench.");
    ImGui::Separator();
    ImGui::Text("Gameplay");
    ImGui::BulletText("F1 opens Options; F2 toggles free look; F3 toggles editor mode; F4 toggles developer panels.");
    ImGui::BulletText("J and K select Deferred and Software renderers; L or F12 enters Photo Mode (Path Tracer).");
    ImGui::BulletText("Escape or P pauses the game and opens the pause menu.");
    ImGui::Text("Editor");
    ImGui::BulletText("Left click selects; right mouse moves the editor camera; Delete removes selection.");
    ImGui::BulletText("Drag .obj, .gltf, or .glb props into the editor to add them.");
    ImGui::BulletText("Alt toggles the menu bar. File > Open map loads .fpsmap files.");

    ImGui::Separator();
    if ( ImGui::Button("Close") )
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
int FpsGameApp::DrawUI()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  if ( _ShowMenuBar )
    DrawMainMenuBar();

  if ( _Editor.IsEnabled() )
    _Editor.DrawDockspace();
  else
    ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

  if ( _ShowDebugPanel )
    DrawDebugPanel();
  if ( _ShowRenderSettingsPanel )
    DrawRenderSettingsUI();
  if ( _ShowRenderStatsPanel )
    DrawRenderStatsUI();

  if ( _Editor.IsEnabled() )
  {
    FpsGameEditorContext editorContext = MakeEditorContext();
    _Editor.DrawPanels(editorContext);
    _Editor.DrawOverlays(editorContext);
    _Editor.DrawGizmo(editorContext);
  }

  FpsGameHudContext hudContext = MakeHudContext();
  _Hud.Draw(hudContext);

  DrawHelpUI();

  _MenuInput.Clear();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  ApplyHudRequests(hudContext);
  return _ExitToTestSelection ? 1 : 0;
}

// ----------------------------------------------------------------------------
// DrawDebugPanel
// ----------------------------------------------------------------------------
void FpsGameApp::DrawDebugPanel()
{
  if ( !ImGui::Begin("Test6 FPS", &_ShowDebugPanel) )
  {
    ImGui::End();
    return;
  }

  if ( FpsPhotoModeState::Inactive != _PhotoModeState )
    ImGui::Text("Photo mode active: gameplay paused");
  else
    ImGui::Text("Click viewport to capture mouse. Esc releases capture.");
  ImGui::Text("Renderer shortcuts: J Deferred, K Software, L/F12 Photo Mode");
  ImGui::Text("F4 toggles this panel. F2 toggles free look. F3 toggles editor.");
  ImGui::Text("Editor: %s", _Editor.IsEnabled() ? "on" : "off");

  ImGui::Checkbox("Free look", &_GameSettings._FreeLook);

  if ( ImGui::CollapsingHeader("Audio Settings") )
  {
    GameMenuUI::DrawAudioSettings( _Audio, _Hud.GetTheme(), _Hud.GetFonts() );
  }

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
    _SceneBinding.SyncCamera(*_Scene, _GameWorld, _GameSettings, _Editor.IsEnabled());
    bool meshDataDirty = false;
    _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings, &meshDataDirty);
    if ( _Renderer )
    {
      _Renderer -> Notify(DirtyState::SceneCamera);
      if ( meshDataDirty )
        _Renderer -> Notify(DirtyState::SceneMeshData);
      _Renderer -> Notify(DirtyState::SceneInstances);
    }
  }

  DrawBenchmarkUI();
  DrawGameSettingsUI();

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawBenchmarkUI
// ----------------------------------------------------------------------------
int FpsGameApp::DrawBenchmarkUI()
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
int FpsGameApp::DrawGameSettingsUI()
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
    bool weaponDirty = false;
    weaponDirty |= ImGui::Checkbox("Show weapon", &_GameSettings._ShowViewWeapon);

    const char * slotLabels[(int)FpsWeaponId::Count] = { "1: Grenade Launcher", "2: Mark 23 Pistol", "3: Carbine Rifle" };
    int selectedSlot = _GameWorld.GetActiveWeaponSlot();
    if ( ImGui::Combo("Weapon slot", &selectedSlot, slotLabels, (int)FpsWeaponId::Count) )
    {
      if ( _GameWorld.SelectWeaponSlot(selectedSlot, _GameSettings) )
      {
        _GameSettings._ActiveWeaponSlot = _GameWorld.GetActiveWeaponSlot();
        weaponDirty = true;
      }
    }

    const int activeSlot = _GameWorld.GetActiveWeaponSlot();
    const FpsWeaponSlotConfig & slotCfg = _GameSettings._WeaponSlots[activeSlot];
    const char * fireModeName = ( FpsWeaponFireMode::Hitscan == slotCfg._FireMode )
                              ? ( slotCfg._Automatic ? "Hitscan (Auto)" : "Hitscan (Semi)" )
                              : "Projectile";
    const char * animActionName = "Idle";
    switch ( _GameWorld.GetWeaponAnimAction() )
    {
    case FpsWeaponAnimAction::Draw:   animActionName = "Draw"; break;
    case FpsWeaponAnimAction::Shoot:  animActionName = "Shoot"; break;
    case FpsWeaponAnimAction::Reload: animActionName = "Reload"; break;
    default: break;
    }

    ImGui::Text("Mode: %s | Ammo: %d / %d",
                fireModeName,
                _GameWorld.GetWeaponAmmo(activeSlot, _GameSettings),
                _GameWorld.GetWeaponMaxAmmo(activeSlot, _GameSettings));
    ImGui::Text("Animation: %s (t = %.3fs)", animActionName, _GameWorld.GetWeaponAnimTimer());

    if ( ( 0 != activeSlot ) && ImGui::SmallButton("Trigger Reload") )
    {
      if ( _GameWorld.TriggerReload(_GameSettings) )
      {
        int reloadSlot = -1;
        if ( _GameWorld.ConsumeWeaponReloaded(&reloadSlot) )
        {
          if ( const char * reloadSound = GetWeaponReloadSoundId(reloadSlot) )
            _Audio.PlaySound2D( reloadSound, 0.85f, 1.0f );
        }
        weaponDirty = true;
      }
    }

    FpsWeaponViewSettings & slotView = _GameSettings._WeaponSlots[activeSlot]._View;
    if ( 0 == activeSlot )
    {
      slotView._Offset = _GameSettings._ViewWeaponOffset;
      slotView._Rotation = _GameSettings._ViewWeaponRotation;
      slotView._Scale = _GameSettings._ViewWeaponScale;
    }

    ImGui::Text("Camera-local transform");
    ImGui::PushItemWidth(260.f);
    weaponDirty |= ImGui::DragFloat3("Translation XYZ", &slotView._Offset.x, 0.01f, -3.f, 3.f, "%.3f");
    weaponDirty |= ImGui::DragFloat3("Rotation XYZ deg", &slotView._Rotation.x, 0.5f, -360.f, 360.f, "%.2f");
    weaponDirty |= ImGui::DragFloat("Scale", &slotView._Scale, 0.001f, 0.001f, 8.f, "%.4f");
    weaponDirty |= ImGui::DragFloat3("Model Pivot XYZ", &slotView._ModelPivot.x, 0.05f, -500.f, 500.f, "%.3f");
    weaponDirty |= ImGui::Checkbox("Flip Model X", &slotView._FlipModelX);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if ( ImGui::SmallButton("Reset weapon pose") )
    {
      const FpsGameSettings defaults;
      slotView = defaults._WeaponSlots[activeSlot]._View;
      weaponDirty = true;
    }

    if ( ImGui::SmallButton("Copy weapon pose") )
    {
      char buffer[256];
      std::snprintf(buffer, sizeof(buffer),
                    "Offset = Vec3(%.3ff, %.3ff, %.3ff), Rotation = Vec3(%.2ff, %.2ff, %.2ff), Scale = %.4ff, Pivot = Vec3(%.3ff, %.3ff, %.3ff), FlipX = %s",
                    slotView._Offset.x, slotView._Offset.y, slotView._Offset.z,
                    slotView._Rotation.x, slotView._Rotation.y, slotView._Rotation.z,
                    slotView._Scale,
                    slotView._ModelPivot.x, slotView._ModelPivot.y, slotView._ModelPivot.z,
                    slotView._FlipModelX ? "true" : "false");
      ImGui::SetClipboardText(buffer);
    }

    ImGui::Text("Offset %.3f %.3f %.3f", slotView._Offset.x, slotView._Offset.y, slotView._Offset.z);
    ImGui::Text("Rotation %.2f %.2f %.2f, Scale %.4f",
                slotView._Rotation.x, slotView._Rotation.y, slotView._Rotation.z, slotView._Scale);

    if ( weaponDirty )
    {
      if ( 0 == activeSlot )
      {
        _GameSettings._ViewWeaponOffset = slotView._Offset;
        _GameSettings._ViewWeaponRotation = slotView._Rotation;
        _GameSettings._ViewWeaponScale = slotView._Scale;
      }

      bool meshDataDirty = false;
      _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings, &meshDataDirty);
      if ( _Renderer )
      {
        if ( meshDataDirty )
          _Renderer -> Notify(DirtyState::SceneMeshData);
        _Renderer -> Notify(DirtyState::SceneInstances);
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawRenderSettingsUI
// ----------------------------------------------------------------------------
int FpsGameApp::DrawRenderSettingsUI()
{
  if ( !ImGui::Begin("Test6 Render Settings", &_ShowRenderSettingsPanel) )
  {
    ImGui::End();
    return 0;
  }

  FpsGameEditorContext editorContext = MakeEditorContext();
  _Editor.DrawRenderSettingsUI(editorContext);

  ImGui::End();
  return 0;
}

// ----------------------------------------------------------------------------
// DrawRenderStatsUI
// ----------------------------------------------------------------------------
int FpsGameApp::DrawRenderStatsUI()
{
  if ( !ImGui::Begin("Test6 Rendering Stats", &_ShowRenderStatsPanel) )
  {
    ImGui::End();
    return 0;
  }

  FpsGameEditorContext editorContext = MakeEditorContext();
  _Editor.DrawPerformanceUI(editorContext);

  ImGui::End();
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int FpsGameApp::UpdateCPUTime()
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
int FpsGameApp::Run()
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
    glfwSetFramebufferSizeCallback(_MainWindow.get(), FpsGameApp::FramebufferSizeCallback);
    glfwSetMouseButtonCallback(_MainWindow.get(), FpsGameApp::MouseButtonCallback);
    glfwSetScrollCallback(_MainWindow.get(), FpsGameApp::MouseScrollCallback);
    glfwSetKeyCallback(_MainWindow.get(), FpsGameApp::KeyCallback);
    glfwSetDropCallback(_MainWindow.get(), FpsGameApp::DropCallback);

    glfwMakeContextCurrent(_MainWindow.get());
    glfwSwapInterval(0);

    glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    SyncFramebufferResolution();

    if ( 0 != InitializeUI() )
    {
      ret = 1;
      break;
    }

    _Preferences.LoadFromFile(UserPreferences::GetDefaultFilePath("fps_preferences.json"));
    Loc::SetLanguage(_Preferences._Language);
    _Preferences.ApplyTo(_Settings, &_Audio);
    if ( _Settings._VSync )
      glfwSwapInterval(1);
    else
      glfwSwapInterval(0);

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

    _Audio.Initialize();
    InitializeSoundBank();

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

      if ( ( FpsPhotoModeState::Capturing == _PhotoModeState ) && _PhotoRenderer )
      {
        UpdatePhotoCapture();
      }
      else if ( ( FpsPhotoModeState::Preview == _PhotoModeState ) && _PhotoRenderer )
      {
        SetCpuTimingEnabled(CpuRendererUpdate, false);
        SetCpuTimingEnabled(CpuRenderToTexture, false);
        SetCpuTimingEnabled(CpuRenderToFile, false);

        BeginCpuTiming(CpuRenderToScreen);
        _PhotoRenderer -> RenderToScreen();
        EndCpuTiming(CpuRenderToScreen);
      }
      else
      {
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
      }

      BeginCpuTiming(CpuDrawUI);
      if ( 0 != DrawUI() )
      {
        EndCpuTiming(CpuDrawUI);
        break;
      }
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
