#include "TetrisGameApp.h"

#include "DeferredRenderer.h"
#include "PathTracer.h"
#include "SoftwareRasterizer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <iostream>
#include <thread>

namespace RTRT
{

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

  if ( GLFW_PRESS == iAction )
  {
    // Directional tap triggers
    if ( ( GLFW_KEY_LEFT == iKey ) || ( GLFW_KEY_A == iKey ) )
      app->_InputState._MoveLeftPressed = true;
    if ( ( GLFW_KEY_RIGHT == iKey ) || ( GLFW_KEY_D == iKey ) )
      app->_InputState._MoveRightPressed = true;

    // Hard drop
    if ( GLFW_KEY_SPACE == iKey )
      app->_InputState._HardDropPressed = true;

    // Rotations
    if ( ( GLFW_KEY_UP == iKey ) || ( GLFW_KEY_X == iKey ) )
      app->_InputState._RotateCWPressed = true;
    if ( ( GLFW_KEY_Z == iKey ) || ( GLFW_KEY_LEFT_CONTROL == iKey ) )
      app->_InputState._RotateCCWPressed = true;
    if ( GLFW_KEY_W == iKey )
      app->_InputState._Rotate180Pressed = true;

    // Hold
    if ( ( GLFW_KEY_C == iKey ) || ( GLFW_KEY_LEFT_SHIFT == iKey ) )
      app->_InputState._HoldPressed = true;

    // Pause & Reset
    if ( ( GLFW_KEY_P == iKey ) || ( GLFW_KEY_ESCAPE == iKey ) )
      app->_InputState._PausePressed = true;
    if ( GLFW_KEY_R == iKey )
      app->_InputState._ResetPressed = true;

    // Toggle Settings UI
    if ( GLFW_KEY_F1 == iKey )
      app->_Hud.ToggleSettings();

    // Hotkey backend switching
    if ( GLFW_KEY_F2 == iKey )
      app->SetRendererMode(TetrisRendererMode::Deferred);
    if ( GLFW_KEY_F3 == iKey )
      app->SetRendererMode(TetrisRendererMode::PhotoPathTracer);
    if ( GLFW_KEY_F4 == iKey )
      app->SetRendererMode(TetrisRendererMode::Software);

    // Cycle camera presets
    if ( GLFW_KEY_V == iKey )
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
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisGameApp::~TetrisGameApp()
{
}

// ----------------------------------------------------------------------------
// ApplyRendererDefaults
// ----------------------------------------------------------------------------
void TetrisGameApp::ApplyRendererDefaults()
{
  if ( TetrisRendererMode::PhotoPathTracer == _RendererMode )
  {
    _Settings._Accumulate = true;
    _Settings._Denoise = true;
#if defined(RTRT_HAS_OIDN)
    _Settings._DenoisingMethod = 3;
#else
    _Settings._DenoisingMethod = 2;
#endif
    _Settings._NbSamplesPerPixel = 1;
    _Settings._Bounces = 2;
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
    _Settings._Transparency = false;
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
// InitializeUI
// ----------------------------------------------------------------------------
int TetrisGameApp::InitializeUI()
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  io.Fonts->AddFontDefault();

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
  _SceneManager.SyncScene(*_Scene, _GameLogic, 0.f);
  _SceneManager.SyncCamera(*_Scene, _GameLogic);
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
  if ( TetrisRendererMode::PhotoPathTracer == _RendererMode )
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
// ProcessInput
// ----------------------------------------------------------------------------
int TetrisGameApp::ProcessInput()
{
  // Track held inputs
  _InputState._LeftHeld = _KeyInput.IsKeyDown(GLFW_KEY_LEFT) || _KeyInput.IsKeyDown(GLFW_KEY_A);
  _InputState._RightHeld = _KeyInput.IsKeyDown(GLFW_KEY_RIGHT) || _KeyInput.IsKeyDown(GLFW_KEY_D);
  _InputState._SoftDropHeld = _KeyInput.IsKeyDown(GLFW_KEY_DOWN) || _KeyInput.IsKeyDown(GLFW_KEY_S);

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
// UpdateGame
// ----------------------------------------------------------------------------
int TetrisGameApp::UpdateGame( float iDeltaTime )
{
  _GameLogic.Update(iDeltaTime, _InputState, _HandlingConfig);

  // Clear edge-triggered inputs for next frame
  _InputState._MoveLeftPressed = false;
  _InputState._MoveRightPressed = false;
  _InputState._HardDropPressed = false;
  _InputState._RotateCWPressed = false;
  _InputState._RotateCCWPressed = false;
  _InputState._Rotate180Pressed = false;
  _InputState._HoldPressed = false;
  _InputState._PausePressed = false;
  _InputState._ResetPressed = false;
  _KeyInput.ClearLastEvents();

  if ( _Scene )
  {
    _SceneManager.SyncScene(*_Scene, _GameLogic, iDeltaTime);
    _SceneManager.SyncCamera(*_Scene, _GameLogic);
  }

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

  _Hud.Draw(_GameLogic, _SceneManager, *_Scene, _Renderer.get(), _Settings, _RendererMode,
            _HandlingConfig, _FrameRate, _DeltaTime, _NbRenderedFrames, _ReloadRenderer);

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

    glfwSwapBuffers(_MainWindow.get());
    _NbRenderedFrames++;

    if ( !iCapturePath.empty() && ( _NbRenderedFrames >= 10 ) )
    {
      if ( _Renderer )
        _Renderer->RenderToFile(iCapturePath);
      break;
    }

    if ( ( iMaxFrames > 0 ) && ( _NbRenderedFrames >= iMaxFrames ) )
      break;
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return 0;
}

}
