#include "TetrisGameApp.h"

#include "DeferredRenderer.h"
#include "PathTracer.h"
#include "SoftwareRasterizer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "stb_image_write.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "PathUtils.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace Tetris
{
using namespace RTRT;

// ----------------------------------------------------------------------------
// ResolveDevicesForPreset
// ----------------------------------------------------------------------------
void TetrisGameApp::ResolveDevicesForPreset( TetrisMultiplayerPreset iPreset,
                                             int iNumPlayers,
                                             bool iGp1Connected,
                                             bool iGp2Connected,
                                             TetrisControlDevice & oP1Device,
                                             TetrisControlDevice & oP2Device )
{
  if ( iNumPlayers <= 1 )
  {
    oP1Device = TetrisControlDevice::KeyboardFull;
    oP2Device = TetrisControlDevice::None;
    return;
  }

  switch ( iPreset )
  {
    case TetrisMultiplayerPreset::Gamepad1_Gamepad2:
      oP1Device = TetrisControlDevice::Gamepad1;
      oP2Device = TetrisControlDevice::Gamepad2;
      break;

    case TetrisMultiplayerPreset::Gamepad1_Keyboard:
      oP1Device = TetrisControlDevice::Gamepad1;
      oP2Device = TetrisControlDevice::KeyboardFull;
      break;

    case TetrisMultiplayerPreset::Keyboard_Gamepad1:
      oP1Device = TetrisControlDevice::KeyboardFull;
      oP2Device = TetrisControlDevice::Gamepad1;
      break;

    case TetrisMultiplayerPreset::Keyboard_Keyboard:
      oP1Device = TetrisControlDevice::KeyboardLeft;
      oP2Device = TetrisControlDevice::KeyboardRight;
      break;

    case TetrisMultiplayerPreset::Auto:
    default:
      if ( iGp1Connected && iGp2Connected )
      {
        oP1Device = TetrisControlDevice::Gamepad1;
        oP2Device = TetrisControlDevice::Gamepad2;
      }
      else if ( iGp1Connected )
      {
        oP1Device = TetrisControlDevice::Gamepad1;
        oP2Device = TetrisControlDevice::KeyboardFull;
      }
      else
      {
        oP1Device = TetrisControlDevice::KeyboardLeft;
        oP2Device = TetrisControlDevice::KeyboardRight;
      }
      break;
  }
}

// ----------------------------------------------------------------------------
// RouteKeyPressToInput
// ----------------------------------------------------------------------------
void TetrisGameApp::RouteKeyPressToInput( int iKey,
                                          TetrisControlDevice iDevice,
                                          TetrisInputState & ioInput )
{
  if ( TetrisControlDevice::KeyboardFull == iDevice )
  {
    if ( ( GLFW_KEY_LEFT == iKey ) || ( GLFW_KEY_A == iKey ) )
      ioInput._MoveLeftPressed = true;
    if ( ( GLFW_KEY_RIGHT == iKey ) || ( GLFW_KEY_D == iKey ) )
      ioInput._MoveRightPressed = true;
    if ( GLFW_KEY_SPACE == iKey )
      ioInput._HardDropPressed = true;
    if ( ( GLFW_KEY_UP == iKey ) || ( GLFW_KEY_X == iKey ) || ( GLFW_KEY_H == iKey ) )
      ioInput._RotateCWPressed = true;
    if ( ( GLFW_KEY_Z == iKey ) || ( GLFW_KEY_LEFT_CONTROL == iKey ) || ( GLFW_KEY_G == iKey ) )
      ioInput._RotateCCWPressed = true;
    if ( ( GLFW_KEY_W == iKey ) || ( GLFW_KEY_Y == iKey ) )
      ioInput._Rotate180Pressed = true;
    if ( ( GLFW_KEY_C == iKey ) || ( GLFW_KEY_LEFT_SHIFT == iKey ) )
      ioInput._HoldPressed = true;
  }
  else if ( TetrisControlDevice::KeyboardLeft == iDevice )
  {
    if ( GLFW_KEY_A == iKey )
      ioInput._MoveLeftPressed = true;
    if ( GLFW_KEY_D == iKey )
      ioInput._MoveRightPressed = true;
    if ( GLFW_KEY_SPACE == iKey )
      ioInput._HardDropPressed = true;
    if ( ( GLFW_KEY_H == iKey ) || ( GLFW_KEY_X == iKey ) || ( GLFW_KEY_E == iKey ) )
      ioInput._RotateCWPressed = true;
    if ( ( GLFW_KEY_G == iKey ) || ( GLFW_KEY_Z == iKey ) || ( GLFW_KEY_Q == iKey ) )
      ioInput._RotateCCWPressed = true;
    if ( ( GLFW_KEY_W == iKey ) || ( GLFW_KEY_Y == iKey ) )
      ioInput._Rotate180Pressed = true;
    if ( ( GLFW_KEY_LEFT_SHIFT == iKey ) || ( GLFW_KEY_C == iKey ) || ( GLFW_KEY_LEFT_CONTROL == iKey ) )
      ioInput._HoldPressed = true;
  }
  else if ( TetrisControlDevice::KeyboardRight == iDevice )
  {
    if ( ( GLFW_KEY_LEFT == iKey ) || ( GLFW_KEY_KP_4 == iKey ) )
      ioInput._MoveLeftPressed = true;
    if ( ( GLFW_KEY_RIGHT == iKey ) || ( GLFW_KEY_KP_6 == iKey ) )
      ioInput._MoveRightPressed = true;
    if ( ( GLFW_KEY_RIGHT_SHIFT == iKey ) || ( GLFW_KEY_ENTER == iKey ) || ( GLFW_KEY_KP_ENTER == iKey ) || ( GLFW_KEY_KP_8 == iKey ) )
      ioInput._HardDropPressed = true;
    if ( ( GLFW_KEY_UP == iKey ) || ( GLFW_KEY_KP_2 == iKey ) || ( GLFW_KEY_SLASH == iKey ) )
      ioInput._RotateCWPressed = true;
    if ( ( GLFW_KEY_KP_1 == iKey ) || ( GLFW_KEY_PERIOD == iKey ) || ( GLFW_KEY_RIGHT_ALT == iKey ) )
      ioInput._RotateCCWPressed = true;
    if ( ( GLFW_KEY_KP_3 == iKey ) || ( GLFW_KEY_COMMA == iKey ) )
      ioInput._Rotate180Pressed = true;
    if ( ( GLFW_KEY_RIGHT_CONTROL == iKey ) || ( GLFW_KEY_KP_0 == iKey ) || ( GLFW_KEY_SEMICOLON == iKey ) )
      ioInput._HoldPressed = true;
  }
}

// ----------------------------------------------------------------------------
// PollHeldKeysForDevice
// ----------------------------------------------------------------------------
void TetrisGameApp::PollHeldKeysForDevice( const KeyInput & iKeyInput,
                                           TetrisControlDevice iDevice,
                                           bool & oLeftHeld,
                                           bool & oRightHeld,
                                           bool & oSoftDropHeld )
{
  oLeftHeld     = false;
  oRightHeld    = false;
  oSoftDropHeld = false;

  if ( TetrisControlDevice::KeyboardFull == iDevice )
  {
    oLeftHeld     = iKeyInput.IsKeyDown(GLFW_KEY_LEFT)  || iKeyInput.IsKeyDown(GLFW_KEY_A);
    oRightHeld    = iKeyInput.IsKeyDown(GLFW_KEY_RIGHT) || iKeyInput.IsKeyDown(GLFW_KEY_D);
    oSoftDropHeld = iKeyInput.IsKeyDown(GLFW_KEY_DOWN)  || iKeyInput.IsKeyDown(GLFW_KEY_S);
  }
  else if ( TetrisControlDevice::KeyboardLeft == iDevice )
  {
    oLeftHeld     = iKeyInput.IsKeyDown(GLFW_KEY_A);
    oRightHeld    = iKeyInput.IsKeyDown(GLFW_KEY_D);
    oSoftDropHeld = iKeyInput.IsKeyDown(GLFW_KEY_S);
  }
  else if ( TetrisControlDevice::KeyboardRight == iDevice )
  {
    oLeftHeld     = iKeyInput.IsKeyDown(GLFW_KEY_LEFT)  || iKeyInput.IsKeyDown(GLFW_KEY_KP_4);
    oRightHeld    = iKeyInput.IsKeyDown(GLFW_KEY_RIGHT) || iKeyInput.IsKeyDown(GLFW_KEY_KP_6);
    oSoftDropHeld = iKeyInput.IsKeyDown(GLFW_KEY_DOWN)  || iKeyInput.IsKeyDown(GLFW_KEY_KP_5);
  }
}

// ----------------------------------------------------------------------------
// IsInActiveGameplay
// ----------------------------------------------------------------------------
bool TetrisGameApp::IsInActiveGameplay() const
{
  if ( ( TetrisAppState::InGame != _AppState ) || _Hud.IsSettingsOpen() )
    return false;

  if ( 2 == _MpState._NumPlayers )
  {
    const bool anyEnded  = _GameLogic[0].IsGameOver() || _GameLogic[0].IsVictory() ||
                           _GameLogic[1].IsGameOver() || _GameLogic[1].IsVictory();
    const bool anyPaused = _GameLogic[0].IsPaused() || _GameLogic[1].IsPaused();
    return !anyEnded && !anyPaused;
  }

  return !_GameLogic[0].IsPaused() && !_GameLogic[0].IsGameOver() && !_GameLogic[0].IsVictory();
}

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void TetrisGameApp::KeyCallback( GLFWwindow * iWindow, int iKey, int, int iAction, int iMods )
{
  auto * const app = static_cast<TetrisGameApp*>(glfwGetWindowUserPointer(iWindow));
  if ( !app )
    return;

  if ( ( GLFW_PRESS == iAction ) || ( GLFW_RELEASE == iAction ) )
    app->_KeyInput.AddEvent(iKey, iAction, iMods);

  if ( ( GLFW_PRESS == iAction ) || ( GLFW_REPEAT == iAction ) )
  {
    if ( ( GLFW_KEY_UP == iKey ) || ( GLFW_KEY_W == iKey ) )
      app->_MenuInput._NavUp = true;
    if ( ( GLFW_KEY_DOWN == iKey ) || ( GLFW_KEY_S == iKey ) )
      app->_MenuInput._NavDown = true;
    if ( ( GLFW_KEY_LEFT == iKey ) || ( GLFW_KEY_A == iKey ) )
      app->_MenuInput._NavLeft = true;
    if ( ( GLFW_KEY_RIGHT == iKey ) || ( GLFW_KEY_D == iKey ) )
      app->_MenuInput._NavRight = true;
  }

  if ( GLFW_PRESS == iAction )
  {
    const bool inActiveGameplay = app->IsInActiveGameplay();

    if ( ( !inActiveGameplay && ( ( GLFW_KEY_ENTER == iKey ) || ( GLFW_KEY_KP_ENTER == iKey ) || ( GLFW_KEY_SPACE == iKey ) ) ) )
    {
      app->_MenuInput._Confirm = true;
    }

    if ( ( GLFW_KEY_ESCAPE == iKey ) || ( GLFW_KEY_BACKSPACE == iKey ) )
    {
      app->_MenuInput._Back = true;
    }

    if ( inActiveGameplay )
    {
      app->ResolvePlayerDevices();
      if ( 1 == app->_MpState._NumPlayers )
      {
        RouteKeyPressToInput(iKey, TetrisControlDevice::KeyboardFull, app->_InputState[0]);
      }
      else
      {
        RouteKeyPressToInput(iKey, app->_MpState._PlayerDevices[0], app->_InputState[0]);
        RouteKeyPressToInput(iKey, app->_MpState._PlayerDevices[1], app->_InputState[1]);
      }

      if ( GLFW_KEY_R == iKey )
        app->_InputState[0]._ResetPressed = true;
    }

    // Pause toggle during InGame
    const bool anyEnded = ( 2 == app->_MpState._NumPlayers )
                          ? ( app->_GameLogic[0].IsGameOver() || app->_GameLogic[0].IsVictory() ||
                              app->_GameLogic[1].IsGameOver() || app->_GameLogic[1].IsVictory() )
                          : ( app->_GameLogic[0].IsGameOver() || app->_GameLogic[0].IsVictory() );

    if ( ( TetrisAppState::InGame == app->_AppState ) && !anyEnded )
    {
      if ( GLFW_KEY_P == iKey )
      {
        app->_InputState[0]._PausePressed = true;
      }
      else if ( GLFW_KEY_ESCAPE == iKey )
      {
        if ( app->_Hud.IsSettingsOpen() )
          app->_Hud.ToggleSettings();
        else
          app->_InputState[0]._PausePressed = true;
      }
    }

    // Toggle Settings UI
    if ( GLFW_KEY_F1 == iKey )
    {
      if ( TetrisAppState::TitleScreen == app->_AppState )
      {
        if ( TetrisMenuPage::Options == app->_Hud.GetMenuPage() )
          app->_Hud.SetMenuPage(TetrisMenuPage::MainMenu);
        else
          app->_Hud.SetMenuPage(TetrisMenuPage::Options);
      }
      else
      {
        app->_Hud.ToggleSettings();
        if ( app->_Hud.IsSettingsOpen() && !app->_GameLogic[0].IsPaused() )
        {
          app->_GameLogic[0].SetPaused(true);
          app->_GameLogic[1].SetPaused(true);
        }
      }
    }

    // Hotkey backend switching
    if ( GLFW_KEY_F2 == iKey )
      app->SetRendererMode(TetrisRendererMode::Deferred);
    if ( GLFW_KEY_F3 == iKey )
      app->SetRendererMode(TetrisRendererMode::PathTracer);
    if ( GLFW_KEY_F4 == iKey )
      app->SetRendererMode(TetrisRendererMode::Software);

    // Cycle camera presets
    if ( ( GLFW_KEY_V == iKey ) && ( TetrisAppState::InGame == app->_AppState ) )
    {
      const int nextCam = ( (int)app->_SceneManager.GetCameraPreset() + 1 ) % 3;
      app->_SceneManager.SetCameraPreset(static_cast<TetrisCameraPreset>(nextCam));
    }
  }
}

// ----------------------------------------------------------------------------
// MouseButtonCallback
// ----------------------------------------------------------------------------
void TetrisGameApp::MouseButtonCallback( GLFWwindow * iWindow, int iButton, int iAction, int )
{
  if ( ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
    return;

  auto * const app = static_cast<TetrisGameApp*>(glfwGetWindowUserPointer(iWindow));
  if ( !app )
    return;

  if ( GLFW_MOUSE_BUTTON_LEFT == iButton )
  {
    if ( GLFW_PRESS == iAction )
    {
      app->_IsMouseDragging = true;
      glfwGetCursorPos(iWindow, &app->_LastMouseX, &app->_LastMouseY);
    }
    else if ( GLFW_RELEASE == iAction )
    {
      app->_IsMouseDragging = false;
    }
  }
}

// ----------------------------------------------------------------------------
// MouseScrollCallback
// ----------------------------------------------------------------------------
void TetrisGameApp::MouseScrollCallback( GLFWwindow * iWindow, double, double iOffsetY )
{
  if ( ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
    return;

  auto * const app = static_cast<TetrisGameApp*>(glfwGetWindowUserPointer(iWindow));
  if ( !app )
    return;

  if ( TetrisCameraPreset::FreeOrbit == app->_SceneManager.GetCameraPreset() )
    app->_SceneManager.OrbitZoom(static_cast<float>(iOffsetY) * 1.5f);
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void TetrisGameApp::FramebufferSizeCallback( GLFWwindow * iWindow, int iWidth, int iHeight )
{
  auto * const app = static_cast<TetrisGameApp*>(glfwGetWindowUserPointer(iWindow));
  if ( !app || ( iWidth <= 0 ) || ( iHeight <= 0 ) )
    return;

  app->SyncFramebufferResolution(true);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
TetrisGameApp::TetrisGameApp( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow), _ScreenWidth(iScreenWidth), _ScreenHeight(iScreenHeight)
{
  _GamepadInputs[0].SetDeviceSlot(0);
  _GamepadInputs[1].SetDeviceSlot(1);

  _Settings._WindowResolution.x = _ScreenWidth;
  _Settings._WindowResolution.y = _ScreenHeight;
  _Settings._RenderResolution.x = _Settings._WindowResolution.x;
  _Settings._RenderResolution.y = _Settings._WindowResolution.y;
  _Settings._BackgroundColor = Vec3(0.015f, 0.018f, 0.024f);
  _Settings._EnableSkybox = true;
  _Settings._EnableBackGround = true;
  _Settings._RenderScale = 100;
  _Settings._SSAO = true;
  _Settings._SSR = true;
  _Settings._ShadowMapping = true;
  _Settings._ShadowBias = 0.002f;
  _Settings._MaxShadowCastingLights = 3;

  // PathTracer initial defaults (preserved across renderer hot-swaps)
  _Settings._Accumulate = true;
  _Settings._Denoise = true;
#if defined(__APPLE__)
  _Settings._DenoisingMethod = 2; // Edge-Aware GPU shader denoiser (OIDN is slow on macOS CPU)
#elif defined(RTRT_HAS_OIDN)
  _Settings._DenoisingMethod = 3; // OIDN (AI)
#else
  _Settings._DenoisingMethod = 2; // Edge-Aware GPU shader denoiser
#endif
  _Settings._NbSamplesPerPixel = 1;
  _Settings._Bounces = 2;
  _Settings._LowResRatio = 1.f;
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisGameApp::~TetrisGameApp()
{
}

// ----------------------------------------------------------------------------
// ResolvePlayerDevices
// ----------------------------------------------------------------------------
void TetrisGameApp::ResolvePlayerDevices()
{
  ResolveDevicesForPreset(
    _MpState._Preset,
    _MpState._NumPlayers,
    _GamepadInputs[0].IsConnected(),
    _GamepadInputs[1].IsConnected(),
    _MpState._PlayerDevices[0],
    _MpState._PlayerDevices[1]
  );
}

// ----------------------------------------------------------------------------
// StartMatch
// ----------------------------------------------------------------------------
void TetrisGameApp::StartMatch( int iNumPlayers, TetrisGameMode iMode, int iStartLevel, bool iResetSeries )
{
  _MpState._NumPlayers = std::clamp(iNumPlayers, 1, 2);
  if ( iResetSeries )
  {
    _MpState._RoundWins[0]    = 0;
    _MpState._RoundWins[1]    = 0;
    _MpState._LastRoundWinner = -1;
  }

  _RoundResolved = false;

  std::random_device rd;
  unsigned int sharedSeed = rd();
  if ( 0 == sharedSeed )
    sharedSeed = 1337u;

  _GameLogic[0].StartNewGame(iMode, iStartLevel, sharedSeed);
  _GameLogic[1].StartNewGame(iMode, iStartLevel, sharedSeed);

  _AudioTrackers[0].Reset(_GameLogic[0]);
  _AudioTrackers[1].Reset(_GameLogic[1]);

  if ( _Scene )
  {
    _SceneManager.SelectNextRandomBackground(*_Scene, _Settings, _Renderer.get());
    _SceneManager.SyncScene(*_Scene, _GameLogic, _MpState._NumPlayers, 0.f);
    _SceneManager.SyncCamera(*_Scene, _GameLogic, _MpState._NumPlayers);
  }

  _PrevLevel   = _GameLogic[0].GetLevel();
  _PrevElapsed = _GameLogic[0].GetElapsedTime();
}

// ----------------------------------------------------------------------------
// CheckPendingMatchRequests
// ----------------------------------------------------------------------------
void TetrisGameApp::CheckPendingMatchRequests()
{
  if ( _MpState._RequestStart1P )
  {
    _MpState._RequestStart1P = false;
    StartMatch(1, _MpState._Requested1PMode, _MpState._StartLevel, true);
  }
  else if ( _MpState._RequestStart2P )
  {
    _MpState._RequestStart2P = false;
    StartMatch(2, _MpState._MpMode, _MpState._StartLevel, true);
  }
  else if ( _MpState._RequestNextRound )
  {
    _MpState._RequestNextRound = false;
    StartMatch(_MpState._NumPlayers, _GameLogic[0].GetGameMode(), _GameLogic[0].GetStartLevel(), false);
  }
  else if ( _MpState._RequestRematch )
  {
    _MpState._RequestRematch = false;
    StartMatch(_MpState._NumPlayers, _GameLogic[0].GetGameMode(), _GameLogic[0].GetStartLevel(), true);
  }
}

// ----------------------------------------------------------------------------
// ApplyRendererDefaults
// ----------------------------------------------------------------------------
void TetrisGameApp::ApplyRendererDefaults()
{
  if ( TetrisRendererMode::PathTracer == _RendererMode )
  {
    _Settings._LowResRatio = 1.f;
    _Settings._TiledRendering = false;
  }
  else if ( TetrisRendererMode::Software == _RendererMode )
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
void TetrisGameApp::SetRendererMode( TetrisRendererMode iMode )
{
  if ( iMode == _RendererMode )
    return;

  _RendererMode = iMode;
  ApplyRendererDefaults();
  _ReloadRenderer = true;
}

// ----------------------------------------------------------------------------
// SyncFramebufferResolution
// ----------------------------------------------------------------------------
void TetrisGameApp::SyncFramebufferResolution( bool iNotifyRenderer )
{
  if ( !_MainWindow )
    return;

  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(_MainWindow.get(), &fbWidth, &fbHeight);
  if ( ( fbWidth <= 0 ) || ( fbHeight <= 0 ) )
    return;

  _Settings._WindowResolution.x = fbWidth;
  _Settings._WindowResolution.y = fbHeight;
  _Settings._RenderResolution.x = static_cast<int>(_Settings._WindowResolution.x * ( _Settings._RenderScale * 0.01f ));
  _Settings._RenderResolution.y = static_cast<int>(_Settings._WindowResolution.y * ( _Settings._RenderScale * 0.01f ));

  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  if ( iNotifyRenderer && _Renderer )
    _Renderer->Notify(DirtyState::RenderSettings);
}

// ----------------------------------------------------------------------------
// InitializeSoundBank
// ----------------------------------------------------------------------------
void TetrisGameApp::InitializeSoundBank()
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

  // 4. Piece Move
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Sine;
    desc._StartFrequency = 440.0f;
    desc._EndFrequency = 520.0f;
    desc._Duration = 0.025f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.010f;
    desc._SustainLevel = 0.1f;
    desc._ReleaseTime = 0.013f;
    desc._Harmonic2Weight = 0.25f;
    desc._Volume = 0.55f;
    _Audio.RegisterProceduralSound("piece_move", desc);
  }

  // 5. Piece Rotate
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 480.0f;
    desc._EndFrequency = 880.0f;
    desc._Duration = 0.045f;
    desc._AttackTime = 0.003f;
    desc._DecayTime = 0.020f;
    desc._SustainLevel = 0.3f;
    desc._ReleaseTime = 0.022f;
    desc._Harmonic2Weight = 0.2f;
    desc._Volume = 0.65f;
    _Audio.RegisterProceduralSound("piece_rotate", desc);
  }

  // 6. Piece Soft Drop
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Sine;
    desc._StartFrequency = 240.0f;
    desc._EndFrequency = 220.0f;
    desc._Duration = 0.022f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.010f;
    desc._SustainLevel = 0.1f;
    desc._ReleaseTime = 0.010f;
    desc._Volume = 0.40f;
    _Audio.RegisterProceduralSound("piece_softdrop", desc);
  }

  // 7. Piece Hard Drop
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Square;
    desc._StartFrequency = 320.0f;
    desc._EndFrequency = 50.0f;
    desc._FrequencyExp = 2.0f;
    desc._Duration = 0.12f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.050f;
    desc._SustainLevel = 0.2f;
    desc._ReleaseTime = 0.068f;
    desc._NoiseMix = 0.35f;
    desc._Volume = 0.85f;
    _Audio.RegisterProceduralSound("piece_harddrop", desc);
  }

  // 8. Piece Lock
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 280.0f;
    desc._EndFrequency = 140.0f;
    desc._Duration = 0.06f;
    desc._AttackTime = 0.002f;
    desc._DecayTime = 0.030f;
    desc._SustainLevel = 0.2f;
    desc._ReleaseTime = 0.028f;
    desc._NoiseMix = 0.15f;
    desc._Volume = 0.65f;
    _Audio.RegisterProceduralSound("piece_lock", desc);
  }

  // 9. Piece Hold
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Sine;
    desc._StartFrequency = 380.0f;
    desc._EndFrequency = 620.0f;
    desc._Duration = 0.07f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.030f;
    desc._SustainLevel = 0.4f;
    desc._ReleaseTime = 0.035f;
    desc._Harmonic2Weight = 0.4f;
    desc._Volume = 0.70f;
    _Audio.RegisterProceduralSound("piece_hold", desc);
  }

  // 10. Clear Line
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 523.25f; // C5
    desc._EndFrequency = 523.25f;
    desc._ArpInterval2 = 4.0f; // E5
    desc._ArpInterval3 = 7.0f; // G5
    desc._ArpSpeed = 0.045f;
    desc._Duration = 0.22f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.060f;
    desc._SustainLevel = 0.4f;
    desc._ReleaseTime = 0.12f;
    desc._Harmonic2Weight = 0.3f;
    desc._Volume = 0.80f;
    _Audio.RegisterProceduralSound("clear_line", desc);
  }

  // 11. Clear Tetris
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 523.25f; // C5
    desc._EndFrequency = 523.25f;
    desc._ArpInterval2 = 4.0f;  // E5
    desc._ArpInterval3 = 7.0f;  // G5
    desc._ArpInterval4 = 12.0f; // C6
    desc._ArpSpeed = 0.065f;
    desc._Duration = 0.45f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.100f;
    desc._SustainLevel = 0.5f;
    desc._ReleaseTime = 0.20f;
    desc._Harmonic2Weight = 0.4f;
    desc._Volume = 0.90f;
    _Audio.RegisterProceduralSound("clear_tetris", desc);
  }

  // 12. Clear T-Spin
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Square;
    desc._StartFrequency = 440.0f;
    desc._EndFrequency = 880.0f;
    desc._DutyCycle = 0.3f;
    desc._Duration = 0.32f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.080f;
    desc._SustainLevel = 0.4f;
    desc._ReleaseTime = 0.15f;
    desc._Harmonic2Weight = 0.5f;
    desc._Volume = 0.85f;
    _Audio.RegisterProceduralSound("clear_tspin", desc);
  }

  // 13. Clear Combo
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Sine;
    desc._StartFrequency = 600.0f;
    desc._EndFrequency = 900.0f;
    desc._Duration = 0.15f;
    desc._AttackTime = 0.004f;
    desc._DecayTime = 0.040f;
    desc._SustainLevel = 0.3f;
    desc._ReleaseTime = 0.08f;
    desc._Harmonic2Weight = 0.5f;
    desc._Harmonic3Weight = 0.25f;
    desc._Volume = 0.75f;
    _Audio.RegisterProceduralSound("clear_combo", desc);
  }

  // 14. Perfect Clear
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 440.0f;
    desc._EndFrequency = 880.0f;
    desc._ArpInterval2 = 7.0f;
    desc._ArpInterval3 = 12.0f;
    desc._ArpInterval4 = 19.0f;
    desc._ArpSpeed = 0.07f;
    desc._Duration = 0.55f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.10f;
    desc._SustainLevel = 0.5f;
    desc._ReleaseTime = 0.25f;
    desc._Volume = 0.95f;
    _Audio.RegisterProceduralSound("clear_perfect", desc);
  }

  // 15. Level Up
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Square;
    desc._StartFrequency = 350.0f;
    desc._EndFrequency = 700.0f;
    desc._DutyCycle = 0.25f;
    desc._ArpInterval2 = 4.0f;
    desc._ArpInterval3 = 7.0f;
    desc._ArpInterval4 = 12.0f;
    desc._ArpSpeed = 0.055f;
    desc._Duration = 0.38f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.08f;
    desc._SustainLevel = 0.4f;
    desc._ReleaseTime = 0.15f;
    desc._Volume = 0.85f;
    _Audio.RegisterProceduralSound("level_up", desc);
  }

  // 16. Game Over
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Sawtooth;
    desc._StartFrequency = 330.0f;
    desc._EndFrequency = 110.0f;
    desc._ArpInterval2 = -3.0f;
    desc._ArpInterval3 = -7.0f;
    desc._ArpSpeed = 0.11f;
    desc._Duration = 0.65f;
    desc._AttackTime = 0.008f;
    desc._DecayTime = 0.15f;
    desc._SustainLevel = 0.3f;
    desc._ReleaseTime = 0.25f;
    desc._NoiseMix = 0.15f;
    desc._Volume = 0.85f;
    _Audio.RegisterProceduralSound("game_over", desc);
  }

  // 17. Victory
  {
    ProceduralSoundDesc desc;
    desc._Waveform = WaveformType::Triangle;
    desc._StartFrequency = 523.25f;
    desc._EndFrequency = 1046.50f;
    desc._ArpInterval2 = 4.0f;
    desc._ArpInterval3 = 7.0f;
    desc._ArpInterval4 = 12.0f;
    desc._ArpSpeed = 0.07f;
    desc._Duration = 0.65f;
    desc._AttackTime = 0.005f;
    desc._DecayTime = 0.12f;
    desc._SustainLevel = 0.5f;
    desc._ReleaseTime = 0.30f;
    desc._Volume = 0.90f;
    _Audio.RegisterProceduralSound("victory", desc);
  }

  // File-based asset overrides (if present on disk)
  const char * soundNames[] = {
    "ui_nav", "ui_confirm", "ui_back",
    "piece_move", "piece_rotate", "piece_softdrop", "piece_harddrop", "piece_lock", "piece_hold",
    "clear_line", "clear_tetris", "clear_tspin", "clear_combo", "clear_perfect",
    "level_up", "game_over", "victory"
  };
  for ( const char * name : soundNames )
  {
    std::string path = PathUtils::GetAssetPath( std::string("Sounds/Tetris/") + name + ".wav" );
    if ( std::filesystem::exists( path ) )
    {
      _Audio.RegisterSoundFile( name, path );
    }
  }
}

// ----------------------------------------------------------------------------
// InitializeUI
// ----------------------------------------------------------------------------
int TetrisGameApp::InitializeUI()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  _Hud.Initialize();
  _Hud.SetGamepadInputs(&_GamepadInputs[0], &_GamepadInputs[1]);
  _Hud.SetPlayerLogics(_GameLogic);
  _Hud.SetMultiplayerState(&_MpState);
  _Hud.SetAudioEngine(&_Audio);

  const char * glslVersion = "#version 410";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int TetrisGameApp::InitializeScene()
{
  std::unique_ptr<Scene> scene(new Scene);
  if ( 0 != _SceneManager.Initialize(*scene) )
  {
    std::cerr << "TetrisGameApp: Failed to initialize scene!" << std::endl;
    return 1;
  }

  _Scene = std::move(scene);
  if ( TetrisAppState::TitleScreen == _AppState )
  {
    _SceneManager.ApplyTitleBackground(*_Scene, _Settings, nullptr);
    _SceneManager.SyncTitleShowcase(*_Scene, 0.f);
    _SceneManager.SyncTitleCamera(*_Scene, 0.f);
  }
  else
  {
    _SceneManager.SelectNextRandomBackground(*_Scene, _Settings, nullptr);
    _SceneManager.SyncScene(*_Scene, _GameLogic, _MpState._NumPlayers, 0.f);
    _SceneManager.SyncCamera(*_Scene, _GameLogic, _MpState._NumPlayers);
  }
  _PrevAppState = _AppState;
  _PrevLevel    = _GameLogic[0].GetLevel();
  _PrevElapsed  = _GameLogic[0].GetElapsedTime();
  return 0;
}

// ----------------------------------------------------------------------------
// InitializeRenderer
// ----------------------------------------------------------------------------
int TetrisGameApp::InitializeRenderer()
{
  if ( !_Scene )
    return 1;

  ApplyRendererDefaults();

  Renderer * newRenderer = nullptr;
  if ( TetrisRendererMode::PathTracer == _RendererMode )
    newRenderer = new PathTracer(*_Scene, _Settings);
  else if ( TetrisRendererMode::Software == _RendererMode )
    newRenderer = new SoftwareRasterizer(*_Scene, _Settings);
  else
    newRenderer = new DeferredRenderer(*_Scene, _Settings);

  if ( !newRenderer )
    return 1;

  _Renderer.reset(newRenderer);
  _Renderer->Initialize();

  return 0;
}

// ----------------------------------------------------------------------------
// PollGamepadForPlayer
// ----------------------------------------------------------------------------
void TetrisGameApp::PollGamepadForPlayer( GamepadInput & ioGamepad,
                                          TetrisInputState & ioInput,
                                          bool iInActiveGameplay )
{
  if ( !ioGamepad.IsConnected() )
    return;

  const float stickDeadzone = ioGamepad.GetStickDeadzone();
  const float leftStickX    = ioGamepad.GetAxis(GLFW_GAMEPAD_AXIS_LEFT_X, stickDeadzone);
  const float leftStickY    = ioGamepad.GetAxis(GLFW_GAMEPAD_AXIS_LEFT_Y, stickDeadzone);

  const bool gpLeftHeld     = ioGamepad.IsButtonDown(GLFW_GAMEPAD_BUTTON_DPAD_LEFT)  || ( leftStickX < -0.45f );
  const bool gpRightHeld    = ioGamepad.IsButtonDown(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT) || ( leftStickX >  0.45f );
  const bool gpSoftDropHeld = ioGamepad.IsButtonDown(GLFW_GAMEPAD_BUTTON_DPAD_DOWN)  || ( leftStickY >  0.45f );

  ioInput._LeftHeld     = ioInput._LeftHeld     || gpLeftHeld;
  ioInput._RightHeld    = ioInput._RightHeld    || gpRightHeld;
  ioInput._SoftDropHeld = ioInput._SoftDropHeld || gpSoftDropHeld;

  if ( !iInActiveGameplay )
    return;

  if ( ioGamepad.IsDirPressed(GamepadDir::Left, true) )
    ioInput._MoveLeftPressed = true;
  if ( ioGamepad.IsDirPressed(GamepadDir::Right, true) )
    ioInput._MoveRightPressed = true;

  const bool dpadUpPressed  = ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_DPAD_UP);
  const bool stickUpPressed = ioGamepad.GetAllowStickHardDrop() && ioGamepad.IsStrictStickUpPressed();
  if ( dpadUpPressed || stickUpPressed )
    ioInput._HardDropPressed = true;

  if ( ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_B) )
    ioInput._RotateCWPressed = true;
  if ( ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_A) )
    ioInput._RotateCCWPressed = true;
  if ( ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_X) || ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_Y) )
    ioInput._Rotate180Pressed = true;

  if ( ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER) ||
       ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER) ||
       ioGamepad.IsLeftTriggerPressed() ||
       ioGamepad.IsRightTriggerPressed() )
  {
    ioInput._HoldPressed = true;
  }

  if ( ioGamepad.IsButtonPressed(GLFW_GAMEPAD_BUTTON_BACK) )
    _InputState[0]._ResetPressed = true;
}

// ----------------------------------------------------------------------------
// ProcessInput
// ----------------------------------------------------------------------------
int TetrisGameApp::ProcessInput()
{
  const float dt = static_cast<float>(_DeltaTime);
  _GamepadInputs[0].Poll(dt);
  _GamepadInputs[1].Poll(dt);

  ResolvePlayerDevices();

  const bool inOptionsMenu = ( ( TetrisAppState::TitleScreen == _AppState ) && ( TetrisMenuPage::Options == _Hud.GetMenuPage() ) ) ||
                             _Hud.IsSettingsOpen();

  // Enable Dear ImGui built-in gamepad widget navigation only when inside the Options panel
  ImGuiIO & io = ImGui::GetIO();
  if ( inOptionsMenu )
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  else
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;

  const bool inActiveGameplay = IsInActiveGameplay();

  // Menu navigation from either connected Gamepad when not in active gameplay
  for ( int g = 0; g < 2; ++g )
  {
    GamepadInput & gp = _GamepadInputs[g];
    if ( !gp.IsConnected() )
      continue;

    if ( !inActiveGameplay )
    {
      if ( gp.IsMenuNavRepeated(GamepadDir::Up) )
        _MenuInput._NavUp = true;
      if ( gp.IsMenuNavRepeated(GamepadDir::Down) )
        _MenuInput._NavDown = true;
      if ( gp.IsMenuNavRepeated(GamepadDir::Left) )
        _MenuInput._NavLeft = true;
      if ( gp.IsMenuNavRepeated(GamepadDir::Right) )
        _MenuInput._NavRight = true;

      if ( gp.IsButtonPressed(GLFW_GAMEPAD_BUTTON_A) )
        _MenuInput._Confirm = true;

      if ( gp.IsButtonPressed(GLFW_GAMEPAD_BUTTON_B) )
      {
        const bool isPaused = ( TetrisAppState::InGame == _AppState ) &&
                              ( _GameLogic[0].IsPaused() || _GameLogic[1].IsPaused() ) &&
                              !_Hud.IsSettingsOpen();
        if ( isPaused )
          _InputState[0]._PausePressed = true;
        else
          _MenuInput._Back = true;
      }

      if ( gp.IsButtonPressed(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER) )
        _MenuInput._PrevTab = true;
      if ( gp.IsButtonPressed(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER) )
        _MenuInput._NextTab = true;
    }

    // Start (Menu) button: Pause / Resume in-game, or Confirm on Title Screen
    if ( gp.IsButtonPressed(GLFW_GAMEPAD_BUTTON_START) )
    {
      if ( TetrisAppState::InGame == _AppState )
      {
        const bool anyEnded = ( 2 == _MpState._NumPlayers )
                              ? ( _GameLogic[0].IsGameOver() || _GameLogic[0].IsVictory() ||
                                  _GameLogic[1].IsGameOver() || _GameLogic[1].IsVictory() )
                              : ( _GameLogic[0].IsGameOver() || _GameLogic[0].IsVictory() );
        if ( _Hud.IsSettingsOpen() )
          _Hud.ToggleSettings();
        else if ( !anyEnded )
          _InputState[0]._PausePressed = true;
        else
          _MenuInput._Confirm = true;
      }
      else
      {
        _MenuInput._Confirm = true;
      }
    }

    // R3 (Right Thumbstick Click): Cycle 3D Camera Preset in-game
    if ( ( TetrisAppState::InGame == _AppState ) && gp.IsButtonPressed(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB) )
    {
      const int nextCam = ( static_cast<int>(_SceneManager.GetCameraPreset()) + 1 ) % 3;
      _SceneManager.SetCameraPreset(static_cast<TetrisCameraPreset>(nextCam));
    }

    // Right Stick: Orbit 3D camera when in FreeOrbit preset
    if ( ( TetrisAppState::InGame == _AppState ) && ( TetrisCameraPreset::FreeOrbit == _SceneManager.GetCameraPreset() ) )
    {
      const float stickDeadzone = gp.GetStickDeadzone();
      const float rx = gp.GetAxis(GLFW_GAMEPAD_AXIS_RIGHT_X, stickDeadzone);
      const float ry = gp.GetAxis(GLFW_GAMEPAD_AXIS_RIGHT_Y, stickDeadzone);
      if ( ( std::fabs(rx) > 0.001f ) || ( std::fabs(ry) > 0.001f ) )
      {
        const float orbitSpeed = 95.0f * dt;
        _SceneManager.OrbitYawPitch(rx * orbitSpeed, -ry * orbitSpeed);
      }
    }
  }

  // Gameplay input routing for 1P or 2P
  if ( 1 == _MpState._NumPlayers )
  {
    PollHeldKeysForDevice(_KeyInput, TetrisControlDevice::KeyboardFull,
                          _InputState[0]._LeftHeld, _InputState[0]._RightHeld, _InputState[0]._SoftDropHeld);
    PollGamepadForPlayer(_GamepadInputs[0], _InputState[0], inActiveGameplay);
  }
  else
  {
    for ( int p = 0; p < 2; ++p )
    {
      const TetrisControlDevice dev = _MpState._PlayerDevices[p];
      PollHeldKeysForDevice(_KeyInput, dev,
                            _InputState[p]._LeftHeld, _InputState[p]._RightHeld, _InputState[p]._SoftDropHeld);

      if ( TetrisControlDevice::Gamepad1 == dev )
        PollGamepadForPlayer(_GamepadInputs[0], _InputState[p], inActiveGameplay);
      else if ( TetrisControlDevice::Gamepad2 == dev )
        PollGamepadForPlayer(_GamepadInputs[1], _InputState[p], inActiveGameplay);
    }
  }

  // FreeOrbit mouse drag
  if ( _IsMouseDragging && ( TetrisCameraPreset::FreeOrbit == _SceneManager.GetCameraPreset() ) )
  {
    double mouseX = 0., mouseY = 0.;
    glfwGetCursorPos(_MainWindow.get(), &mouseX, &mouseY);
    const float dx = static_cast<float>(mouseX - _LastMouseX) * 0.4f;
    const float dy = static_cast<float>(mouseY - _LastMouseY) * 0.4f;
    _LastMouseX = mouseX;
    _LastMouseY = mouseY;

    _SceneManager.OrbitYawPitch(dx, -dy);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// EvaluatePlayerAudio
// ----------------------------------------------------------------------------
void TetrisGameApp::EvaluatePlayerAudio( int iPlayerIdx, const Vec3 & iBoardPos, bool iUseSpatial3D )
{
  const TetrisGameLogic & logic     = _GameLogic[iPlayerIdx];
  const TetrisInputState & input    = _InputState[iPlayerIdx];
  TetrisPlayerAudioTracker & tracker = _AudioTrackers[iPlayerIdx];

  auto playSfx = [&]( const std::string & iSoundId, float iVol, float iPitch )
  {
    if ( iUseSpatial3D )
      _Audio.PlaySound3D(iSoundId, iBoardPos, iVol, iPitch, 20.0f, 80.0f);
    else
      _Audio.PlaySound2D(iSoundId, iVol, iPitch);
  };

  const bool isGameOver            = logic.IsGameOver();
  const bool isVictory             = logic.IsVictory();
  const unsigned int curLockId     = logic.GetLockEventId();
  const unsigned int curLineClearId = logic.GetLineClearEventId();
  const int curLevel               = logic.GetLevel();
  const TetrisPieceType curPiece   = logic.GetActivePiece();
  const Vec2i & curOrigin          = logic.GetActiveOrigin();
  const TetrisRotation curRot      = logic.GetActiveRotation();
  const TetrisPieceType curHold    = logic.GetHoldPiece();

  if ( isGameOver && !tracker._PrevGameOver )
  {
    playSfx("game_over", 0.9f, 1.0f);
  }
  else if ( isVictory && !tracker._PrevVictory )
  {
    playSfx("victory", 0.95f, 1.0f);
  }
  else if ( curLineClearId != tracker._PrevLineClearId )
  {
    if ( logic.IsLastClearPerfect() )
    {
      playSfx("clear_perfect", 1.0f, 1.0f);
    }
    else if ( logic.GetLastClearTSpin() != TetrisTSpinType::None )
    {
      playSfx("clear_tspin", 0.95f, 1.0f);
    }
    else
    {
      const int cleared = static_cast<int>(logic.GetLastClearedRows().size());
      if ( cleared >= 4 )
      {
        playSfx("clear_tetris", 1.0f, 1.0f);
      }
      else
      {
        const float pitch = 1.0f + ( cleared - 1 ) * 0.12f;
        playSfx("clear_line", 0.85f, pitch);
      }
    }

    if ( logic.GetComboCount() >= 1 )
    {
      const float comboPitch = std::min(2.2f, 1.0f + logic.GetComboCount() * 0.08f);
      playSfx("clear_combo", 0.8f, comboPitch);
    }
  }
  else if ( curLockId != tracker._PrevLockEventId )
  {
    if ( logic.WasLastLockFromHardDrop() )
      playSfx("piece_harddrop", 0.85f, 1.0f);
    else
      playSfx("piece_lock", 0.75f, 1.0f);
  }
  else if ( curHold != tracker._PrevHoldPiece )
  {
    playSfx("piece_hold", 0.75f, 1.0f);
  }
  else if ( ( curPiece != TetrisPieceType::None ) && ( curPiece == tracker._PrevActivePiece ) )
  {
    if ( curRot != tracker._PrevRotation )
    {
      playSfx("piece_rotate", 0.70f, 1.0f);
    }
    else if ( curOrigin.x != tracker._PrevPieceOriginX )
    {
      playSfx("piece_move", 0.60f, 1.0f);
    }
    else if ( input._SoftDropHeld && ( curOrigin.y < tracker._PrevPieceOriginY ) )
    {
      playSfx("piece_softdrop", 0.45f, 1.0f);
    }
  }

  if ( ( curLevel > tracker._PrevLevel ) && !isGameOver && !isVictory )
  {
    playSfx("level_up", 0.9f, 1.0f);
  }

  tracker._PrevGameOver     = isGameOver;
  tracker._PrevVictory      = isVictory;
  tracker._PrevLockEventId  = curLockId;
  tracker._PrevLineClearId  = curLineClearId;
  tracker._PrevActivePiece  = curPiece;
  tracker._PrevPieceOriginX = curOrigin.x;
  tracker._PrevPieceOriginY = curOrigin.y;
  tracker._PrevRotation     = curRot;
  tracker._PrevHoldPiece    = curHold;
  tracker._PrevLevel        = curLevel;
}

// ----------------------------------------------------------------------------
// UpdateGame
// ----------------------------------------------------------------------------
int TetrisGameApp::UpdateGame( float iDeltaTime )
{
  CheckPendingMatchRequests();

  if ( _Scene && ( _AppState != _PrevAppState ) )
  {
    if ( TetrisAppState::TitleScreen == _AppState )
    {
      _SceneManager.ApplyTitleBackground(*_Scene, _Settings, _Renderer.get());
    }
    else
    {
      _SceneManager.SelectNextRandomBackground(*_Scene, _Settings, _Renderer.get());
      _PrevLevel   = _GameLogic[0].GetLevel();
      _PrevElapsed = _GameLogic[0].GetElapsedTime();
    }
    _PrevAppState = _AppState;
  }

  if ( TetrisAppState::TitleScreen == _AppState )
  {
    if ( _Scene )
    {
      const float curTime = static_cast<float>(glfwGetTime());
      _SceneManager.SyncTitleShowcase(*_Scene, curTime);
      _SceneManager.SyncTitleCamera(*_Scene, curTime);
    }
  }
  else if ( 1 == _MpState._NumPlayers )
  {
    const bool resetRequested = _InputState[0]._ResetPressed;
    if ( !_Hud.IsSettingsOpen() )
      _GameLogic[0].Update(iDeltaTime, _InputState[0], _HandlingConfig);

    EvaluatePlayerAudio(0, Vec3(0.0f, 10.0f, 0.0f), false);

    if ( _Scene )
    {
      const double curElapsed = _GameLogic[0].GetElapsedTime();
      const int curLevel      = _GameLogic[0].GetLevel();

      if ( resetRequested || ( ( _PrevElapsed > 0.25 ) && ( curElapsed < 0.05 ) ) )
      {
        _SceneManager.SelectNextRandomBackground(*_Scene, _Settings, _Renderer.get());
        _PrevLevel = curLevel;
      }
      else if ( _SceneManager.GetRotateBackgroundOnLevelUp() && ( curLevel > _PrevLevel ) )
      {
        _SceneManager.SelectNextRandomBackground(*_Scene, _Settings, _Renderer.get());
        _PrevLevel = curLevel;
      }
      else
      {
        _PrevLevel = curLevel;
      }
      _PrevElapsed = curElapsed;

      if ( !_Hud.IsSettingsOpen() && !_GameLogic[0].IsPaused() && !_GameLogic[0].IsGameOver() )
      {
        const float curTime = static_cast<float>(glfwGetTime());
        _SceneManager.UpdateBackgroundMotion(_Settings, curTime);
      }

      _SceneManager.SyncScene(*_Scene, _GameLogic, 1, iDeltaTime);
      _SceneManager.SyncCamera(*_Scene, _GameLogic, 1);
    }
  }
  else
  {
    // 2-Player Local Multiplayer Update
    if ( _InputState[0]._ResetPressed )
    {
      StartMatch(2, _GameLogic[0].GetGameMode(), _GameLogic[0].GetStartLevel(), false);
    }
    else if ( _InputState[0]._PausePressed )
    {
      const bool nextPaused = !_GameLogic[0].IsPaused();
      _GameLogic[0].SetPaused(nextPaused);
      _GameLogic[1].SetPaused(nextPaused);
      _InputState[0]._PausePressed = false;
      _InputState[1]._PausePressed = false;
    }

    const unsigned int prevClearId0 = _GameLogic[0].GetLineClearEventId();
    const unsigned int prevClearId1 = _GameLogic[1].GetLineClearEventId();

    if ( !_Hud.IsSettingsOpen() && !_RoundResolved )
    {
      _GameLogic[0].Update(iDeltaTime, _InputState[0], _HandlingConfig);
      _GameLogic[1].Update(iDeltaTime, _InputState[1], _HandlingConfig);

      // Route VS Battle Garbage Attacks between P1 and P2
      if ( TetrisGameMode::Versus == _GameLogic[0].GetGameMode() )
      {
        if ( ( _GameLogic[0].GetLineClearEventId() != prevClearId0 ) && ( _GameLogic[0].GetLastAttackLinesSent() > 0 ) )
        {
          _GameLogic[1].AddPendingGarbage(_GameLogic[0].GetLastAttackLinesSent());
        }
        if ( ( _GameLogic[1].GetLineClearEventId() != prevClearId1 ) && ( _GameLogic[1].GetLastAttackLinesSent() > 0 ) )
        {
          _GameLogic[0].AddPendingGarbage(_GameLogic[1].GetLastAttackLinesSent());
        }
      }

      // Check 2P Round Completion & Winner Resolution
      const bool p0Dead = _GameLogic[0].IsGameOver();
      const bool p1Dead = _GameLogic[1].IsGameOver();
      const bool p0Won  = _GameLogic[0].IsVictory();
      const bool p1Won  = _GameLogic[1].IsVictory();

      if ( p0Dead || p1Dead || p0Won || p1Won )
      {
        _RoundResolved = true;
        int winner = -1;

        if ( p0Dead && !p1Dead )
        {
          _GameLogic[1].ForceVictory();
          winner = 1;
        }
        else if ( p1Dead && !p0Dead )
        {
          _GameLogic[0].ForceVictory();
          winner = 0;
        }
        else if ( p0Won && !p1Won )
        {
          _GameLogic[1].ForceGameOver("Opponent Completed Objective First");
          winner = 0;
        }
        else if ( p1Won && !p0Won )
        {
          _GameLogic[0].ForceGameOver("Opponent Completed Objective First");
          winner = 1;
        }
        else
        {
          // Simultaneous finish (e.g. Ultra 2:00 timer expiration or simultaneous top-out)
          if ( _GameLogic[0].GetScore() > _GameLogic[1].GetScore() )
          {
            _GameLogic[0].ForceVictory();
            _GameLogic[1].ForceGameOver("Outscored by Player 1");
            winner = 0;
          }
          else if ( _GameLogic[1].GetScore() > _GameLogic[0].GetScore() )
          {
            _GameLogic[1].ForceVictory();
            _GameLogic[0].ForceGameOver("Outscored by Player 2");
            winner = 1;
          }
          else
          {
            winner = -1;
          }
        }

        _MpState._LastRoundWinner = winner;
        if ( ( 0 == winner ) || ( 1 == winner ) )
          _MpState._RoundWins[winner]++;
      }
    }

    // 3D Spatialized Audio for P1 (Left Board) and P2 (Right Board)
    _Audio.UpdateListener(Vec3(0.0f, 10.0f, 18.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f));
    EvaluatePlayerAudio(0, TetrisSceneManager::GetBoardOrigin(0, 2) + Vec3(0.0f, 10.0f, 0.0f), true);
    EvaluatePlayerAudio(1, TetrisSceneManager::GetBoardOrigin(1, 2) + Vec3(0.0f, 10.0f, 0.0f), true);

    if ( _Scene )
    {
      const int maxLevel = std::max(_GameLogic[0].GetLevel(), _GameLogic[1].GetLevel());
      if ( _SceneManager.GetRotateBackgroundOnLevelUp() && ( maxLevel > _PrevLevel ) )
      {
        _SceneManager.SelectNextRandomBackground(*_Scene, _Settings, _Renderer.get());
      }
      _PrevLevel = maxLevel;

      if ( !_Hud.IsSettingsOpen() && !_GameLogic[0].IsPaused() && !_RoundResolved )
      {
        const float curTime = static_cast<float>(glfwGetTime());
        _SceneManager.UpdateBackgroundMotion(_Settings, curTime);
      }

      _SceneManager.SyncScene(*_Scene, _GameLogic, 2, iDeltaTime);
      _SceneManager.SyncCamera(*_Scene, _GameLogic, 2);
    }
  }

  // Clear edge-triggered inputs for next frame
  for ( int p = 0; p < 2; ++p )
  {
    _InputState[p]._MoveLeftPressed  = false;
    _InputState[p]._MoveRightPressed = false;
    _InputState[p]._HardDropPressed  = false;
    _InputState[p]._RotateCWPressed  = false;
    _InputState[p]._RotateCCWPressed = false;
    _InputState[p]._Rotate180Pressed = false;
    _InputState[p]._HoldPressed      = false;
    _InputState[p]._PausePressed     = false;
    _InputState[p]._ResetPressed     = false;
  }
  _KeyInput.ClearLastEvents();

  if ( _Renderer )
  {
    _Renderer->Notify(DirtyState::SceneCamera);
    _Renderer->Notify(DirtyState::SceneInstances);
    _Renderer->Notify(DirtyState::SceneLights);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
int TetrisGameApp::DrawUI()
{
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  bool requestExit = false;
  _Hud.Draw(_AppState, _GameLogic[0], _SceneManager, *_Scene, _Renderer.get(), _Settings, _RendererMode,
            _HandlingConfig, _MenuInput, _FrameRate, _DeltaTime, _NbRenderedFrames, _ReloadRenderer, requestExit);

  CheckPendingMatchRequests();
  _MenuInput.Clear();

  if ( requestExit && _MainWindow )
    glfwSetWindowShouldClose(_MainWindow.get(), GLFW_TRUE);

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  return 0;
}

// ----------------------------------------------------------------------------
// Run
// ----------------------------------------------------------------------------
int TetrisGameApp::Run( const std::string & iCapturePath, int iMaxFrames )
{
  if ( !_MainWindow )
    return 1;

  glfwSetWindowUserPointer(_MainWindow.get(), this);
  glfwSetKeyCallback(_MainWindow.get(), KeyCallback);
  glfwSetMouseButtonCallback(_MainWindow.get(), MouseButtonCallback);
  glfwSetScrollCallback(_MainWindow.get(), MouseScrollCallback);
  glfwSetFramebufferSizeCallback(_MainWindow.get(), FramebufferSizeCallback);

  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cerr << "TetrisGameApp: Failed to initialize GLEW!" << std::endl;
    return 1;
  }

  if ( 0 != InitializeUI() )
    return 1;
  if ( 0 != InitializeScene() )
    return 1;
  if ( 0 != InitializeRenderer() )
    return 1;

  _Audio.Initialize();
  InitializeSoundBank();

  SyncFramebufferResolution(true);
  glDisable(GL_DEPTH_TEST);

  _LastTime = glfwGetTime();

  while ( !glfwWindowShouldClose(_MainWindow.get()) )
  {
    const double currentTime = glfwGetTime();
    _DeltaTime = std::min(0.1, currentTime - _LastTime);
    _LastTime = currentTime;
    _FrameRate = ( _DeltaTime > 0.0001 ) ? ( 1.0 / _DeltaTime ) : 60.0;
    _FrameTime = _DeltaTime * 1000.0;

    glfwPollEvents();

    if ( _ReloadRenderer )
    {
      InitializeRenderer();
      _ReloadRenderer = false;
    }

    ProcessInput();
    UpdateGame(static_cast<float>(_DeltaTime));

    if ( _Renderer )
    {
      _Renderer->Update();
      _Renderer->RenderToTexture();
      _Renderer->RenderToScreen();
      _Renderer->Done();
    }

    DrawUI();

    if ( !iCapturePath.empty() && ( _NbRenderedFrames >= 10 ) )
    {
      const int w = _Settings._WindowResolution.x;
      const int h = _Settings._WindowResolution.y;
      if ( ( w > 0 ) && ( h > 0 ) )
      {
        std::vector<unsigned char> frameData(static_cast<size_t>(w) * static_cast<size_t>(h) * 4u, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, frameData.data());
        stbi_flip_vertically_on_write(true);
        stbi_write_png(iCapturePath.c_str(), w, h, 4, frameData.data(), w * 4);
      }
      glfwSwapBuffers(_MainWindow.get());
      break;
    }

    glfwSwapBuffers(_MainWindow.get());
    _NbRenderedFrames++;

    if ( ( iMaxFrames > 0 ) && ( static_cast<int>(_NbRenderedFrames) >= iMaxFrames ) )
      break;
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return 0;
}

}
