#pragma warning(disable : 4100) // unreferenced formal parameter

#include "Test6.h"

#include "DeferredRenderer.h"
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
#include "glm/gtc/type_ptr.hpp"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <thread>

namespace RTRT
{

const char * Test6::GetTestHeader() { return "Basic FPS"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static int g_Test6DebugMode = 0;
static unsigned int g_Test6NbThreadsMax = std::thread::hardware_concurrency();

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
// CopyPathToBuffer
// ----------------------------------------------------------------------------
static void CopyPathToBuffer( char * oBuffer, size_t iBufferSize, const std::string & iPath )
{
  if ( !oBuffer || !iBufferSize )
    return;

  std::snprintf(oBuffer, iBufferSize, "%s", iPath.c_str());
}

// ----------------------------------------------------------------------------
// EditorMaterialSlotFromName
// ----------------------------------------------------------------------------
static bool EditorMaterialSlotFromName( const std::string & iName, FpsMaterialSlot & oMaterial )
{
  if ( "floor" == iName )
    oMaterial = FpsMaterialSlot::Floor;
  else if ( "wall" == iName )
    oMaterial = FpsMaterialSlot::Wall;
  else if ( "pillar" == iName )
    oMaterial = FpsMaterialSlot::Pillar;
  else if ( "crate" == iName )
    oMaterial = FpsMaterialSlot::Crate;
  else if ( "accent" == iName )
    oMaterial = FpsMaterialSlot::Accent;
  else
    return false;

  return true;
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
// CTOR
// ----------------------------------------------------------------------------
Test6::Test6( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: BaseTest(iMainWindow, iScreenWidth, iScreenHeight)
{
  _Settings._WindowResolution.x = iScreenWidth;
  _Settings._WindowResolution.y = iScreenHeight;
  _Settings._RenderResolution.x = iScreenWidth;
  _Settings._RenderResolution.y = iScreenHeight;
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
    _Settings._NbThreads = std::max(1u, g_Test6NbThreadsMax);
    _Settings._ShadingType = ShadingType::PBR;
    _Settings._Sampling = SamplingMode::Bilinear;
    _Settings._TiledRendering = true;
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
// SetEditorPathBuffers
// ----------------------------------------------------------------------------
void Test6::SetEditorPathBuffers()
{
  CopyPathToBuffer(_Editor._SavePath, sizeof(_Editor._SavePath), _MapPath);
  CopyPathToBuffer(_Editor._LoadPath, sizeof(_Editor._LoadPath), _MapPath);
}

// ----------------------------------------------------------------------------
// SyncMapFromRuntimeSettings
// ----------------------------------------------------------------------------
void Test6::SyncMapFromRuntimeSettings()
{
  _Map._MaxProjectiles = _GameSettings._MaxProjectiles;
  _Map._MaxProjectileAmmo = _GameSettings._MaxProjectileAmmo;
  _Map._ProjectileAmmoRefillTime = _GameSettings._ProjectileAmmoRefillTime;

  _Map._Weapon._Visible = _Editor._Enabled ? _Editor._PreviousShowViewWeapon : _GameSettings._ShowViewWeapon;
  _Map._Weapon._Offset = _GameSettings._ViewWeaponOffset;
  _Map._Weapon._Rotation = _GameSettings._ViewWeaponRotation;
  _Map._Weapon._Scale = _GameSettings._ViewWeaponScale;
}

// ----------------------------------------------------------------------------
// SetEditorMode
// ----------------------------------------------------------------------------
void Test6::SetEditorMode( bool iEnabled )
{
  if ( _Editor._Enabled == iEnabled )
    return;

  _Editor._Enabled = iEnabled;
  _HasLastMousePos = false;

  if ( _Editor._Enabled )
  {
    _Editor._PreviousShowViewWeapon = _GameSettings._ShowViewWeapon;
    _Editor._PreviousFreeLook = _GameSettings._FreeLook;
    _GameSettings._ShowViewWeapon = false;
    _GameSettings._FreeLook = true;
    _GameWorld.ClearProjectiles();
    SetMouseCaptured(false);
  }
  else
  {
    _GameSettings._ShowViewWeapon = _Editor._PreviousShowViewWeapon;
    _GameSettings._FreeLook = _Editor._PreviousFreeLook;
  }

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
// SyncEditorObject
// ----------------------------------------------------------------------------
void Test6::SyncEditorObject( int iObjectIndex )
{
  if ( ( iObjectIndex < 0 ) || ( iObjectIndex >= static_cast<int>(_Map._Objects.size()) ) )
    return;

  std::vector<FpsSceneObject> & objects = _GameWorld.GetObjects();
  if ( iObjectIndex >= static_cast<int>(objects.size()) )
    return;

  objects[iObjectIndex] = _Map._Objects[iObjectIndex];

  if ( _Scene )
    _SceneBinding.SyncTransforms(*_Scene, _GameWorld, _GameSettings);
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::SceneInstances);
}

// ----------------------------------------------------------------------------
// SyncEditorLight
// ----------------------------------------------------------------------------
void Test6::SyncEditorLight( int iLightIndex )
{
  if ( !_Scene )
    return;
  if ( ( iLightIndex < 0 ) || ( iLightIndex >= static_cast<int>(_Map._Lights.size()) ) )
    return;

  Light * light = _Scene -> GetLight(iLightIndex);
  if ( !light )
    return;

  *light = _Map._Lights[iLightIndex];
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::SceneLights);
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

  ImGui::StyleColorsDark();
  io.Fonts -> AddFontDefault();

  const char * glslVersion = "#version 410";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test6::InitializeScene()
{
  std::unique_ptr<Scene> newScene(new Scene);
  if ( !newScene )
    return 1;

  const bool preserveEditorPose = _Editor._Enabled;
  FpsPlayer editorPlayer;
  if ( preserveEditorPose )
    editorPlayer = _GameWorld.GetPlayer();

  if ( _MapPath.empty() )
    _MapPath = PathUtils::GetAssetPath("FPSMaps/default.fpsmap");

  if ( !_MapLoaded )
  {
    _Map = FpsGameMap();
    _MapLoaded = FpsGameMapLoader::Load(_MapPath, _Map);
  }

  if ( _MapLoaded )
  {
    FpsGameMapLoader::SeedDefaultMaterials(_Map);
    ApplyMapSettings(_Map, _GameSettings);
    if ( _Editor._Enabled )
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
  SetEditorPathBuffers();
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

  g_Test6DebugMode = 0;
  _Renderer -> SetDebugMode(g_Test6DebugMode);
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
    SetEditorMode(!_Editor._Enabled);

  if ( _Editor._Enabled )
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
        _ReloadRenderer = true;
      }
    }

    const bool rightMouseHeld = !ImGui::GetIO().WantCaptureMouse
                             && ( GLFW_PRESS == glfwGetMouseButton(_MainWindow.get(), GLFW_MOUSE_BUTTON_RIGHT) )
                             && ( FpsRendererMode::PhotoPathTracer != _GameSettings._RendererMode );
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
  if ( _ReloadScene )
  {
    _ReloadScene = false;
    _Renderer.reset();
    if ( 0 != InitializeScene() )
      return 1;
    _ReloadRenderer = true;
  }

  if ( _ReloadRenderer )
  {
    _ReloadRenderer = false;
    if ( 0 != InitializeRenderer() )
      return 1;
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
    DrawDebugPanel();

  if ( _Editor._Enabled )
  {
    DrawEditorPanel();
    DrawEditorGizmo();
    DrawCrosshair();
  }
  else
    DrawHUD();

  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  return 0;
}

// ----------------------------------------------------------------------------
// DrawDebugPanel
// ----------------------------------------------------------------------------
void Test6::DrawDebugPanel()
{
  ImGui::Begin("Test6 FPS");

  static const char * Renderers[] = { "Deferred", "Software", "Photo Path Tracer" };
  int rendererMode = (int)_GameSettings._RendererMode;
  if ( ImGui::Combo("Renderer", &rendererMode, Renderers, 3) )
  {
    if ( rendererMode != (int)_GameSettings._RendererMode )
    {
      _GameSettings._RendererMode = (FpsRendererMode)rendererMode;
      _ReloadRenderer = true;
    }
  }

  if ( FpsRendererMode::PhotoPathTracer == _GameSettings._RendererMode )
    ImGui::Text("Photo mode: gameplay paused");
  else
    ImGui::Text("Click viewport to capture mouse. Esc releases capture.");
  ImGui::Text("Renderer shortcuts: J Deferred, K Software, L Photo");
  ImGui::Text("F1 toggles this panel. F2 toggles free look. F3 toggles editor.");
  ImGui::Text("Editor: %s", _Editor._Enabled ? "on" : "off");

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

  DrawSettingsUI();

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorPanel
// ----------------------------------------------------------------------------
void Test6::DrawEditorPanel()
{
  ImGui::Begin("Test6 Map Editor");

  ImGui::Text("Mode: editor");
  ImGui::Text("Map: %s%s", _MapPath.c_str(), _Editor._Dirty ? " *" : "");
  ImGui::Checkbox("Collider helpers", &_Editor._ShowColliderHelpers);
  ImGui::SameLine();
  ImGui::Checkbox("Light helpers", &_Editor._ShowLightHelpers);

  ImGui::PushItemWidth(360.f);
  ImGui::InputText("Save path", _Editor._SavePath, sizeof(_Editor._SavePath));
  if ( ImGui::Button("Save") )
  {
    SyncMapFromRuntimeSettings();
    if ( FpsGameMapLoader::Save(_Editor._SavePath, _Map) )
    {
      _MapPath = _Editor._SavePath;
      SetEditorPathBuffers();
      _Editor._Dirty = false;
    }
  }

  ImGui::InputText("Load path", _Editor._LoadPath, sizeof(_Editor._LoadPath));
  if ( ImGui::Button("Load") )
  {
    FpsGameMap loadedMap;
    if ( FpsGameMapLoader::Load(_Editor._LoadPath, loadedMap) )
    {
      _Map = loadedMap;
      _MapPath = _Editor._LoadPath;
      _MapLoaded = true;
      _Editor._Selection = FpsEditorSelection();
      _Editor._Dirty = false;
      SetEditorPathBuffers();
      _ReloadScene = true;
    }
  }
  ImGui::PopItemWidth();

  if ( ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen) )
  {
    if ( ImGui::Button("Add box") )
    {
      FpsSceneObject object;
      object._Name = "Box " + std::to_string(static_cast<int>(_Map._Objects.size()));
      object._Center = _GameWorld.GetPlayer().EyePosition(_GameSettings) + Vec3(0.f, 0.f, 3.f);
      object._HalfExtents = Vec3(0.5f);
      object._MaterialName = "wall";
      object._Material = FpsMaterialSlot::Wall;
      object._Collidable = true;
      object._Visible = true;
      _Map._Objects.push_back(object);
      _MapLoaded = true;
      _Editor._Selection._Kind = FpsEditableKind::Box;
      _Editor._Selection._Index = static_cast<int>(_Map._Objects.size()) - 1;
      MarkEditorDirty();
      _ReloadScene = true;
    }
    ImGui::SameLine();
    if ( ImGui::Button("Add collider") )
    {
      FpsSceneObject object;
      object._Name = "Collider " + std::to_string(static_cast<int>(_Map._Objects.size()));
      object._Center = _GameWorld.GetPlayer().EyePosition(_GameSettings) + Vec3(0.f, 0.f, 3.f);
      object._HalfExtents = Vec3(0.5f);
      object._MaterialName = "wall";
      object._Material = FpsMaterialSlot::Wall;
      object._Collidable = true;
      object._Visible = false;
      _Map._Objects.push_back(object);
      _MapLoaded = true;
      _Editor._Selection._Kind = FpsEditableKind::Collider;
      _Editor._Selection._Index = static_cast<int>(_Map._Objects.size()) - 1;
      MarkEditorDirty();
      _ReloadScene = true;
    }

    if ( ImGui::BeginListBox("Instances", ImVec2(-FLT_MIN, 180.f)) )
    {
      for ( int i = 0; i < static_cast<int>(_Map._Objects.size()); ++i )
      {
        const FpsSceneObject & object = _Map._Objects[i];
        const bool selected = ( ( FpsEditableKind::Box == _Editor._Selection._Kind )
                             || ( FpsEditableKind::Collider == _Editor._Selection._Kind ) )
                           && ( _Editor._Selection._Index == i );
        const std::string label = std::string(object._Visible ? "Box: " : "Collider: ") + object._Name + "##object" + std::to_string(i);
        if ( ImGui::Selectable(label.c_str(), selected) )
        {
          _Editor._Selection._Kind = object._Visible ? FpsEditableKind::Box : FpsEditableKind::Collider;
          _Editor._Selection._Index = i;
        }
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }

    if ( ( FpsEditableKind::Box == _Editor._Selection._Kind )
      || ( FpsEditableKind::Collider == _Editor._Selection._Kind ) )
    {
      const int index = _Editor._Selection._Index;
      if ( ( index >= 0 ) && ( index < static_cast<int>(_Map._Objects.size()) ) )
      {
        FpsSceneObject & object = _Map._Objects[index];
        bool objectDirty = false;
        char nameBuffer[128];
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object._Name.c_str());
        if ( ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)) )
        {
          object._Name = nameBuffer;
          objectDirty = true;
        }

        objectDirty |= ImGui::DragFloat3("Center", &object._Center.x, 0.05f, -100.f, 100.f, "%.3f");
        objectDirty |= ImGui::DragFloat3("Half extents", &object._HalfExtents.x, 0.05f, 0.01f, 100.f, "%.3f");
        objectDirty |= ImGui::Checkbox("Collidable", &object._Collidable);

        if ( object._Visible )
        {
          FpsGameMapLoader::SeedDefaultMaterials(_Map);
          int currentMaterial = 0;
          for ( int i = 0; i < static_cast<int>(_Map._Materials.size()); ++i )
          {
            if ( _Map._Materials[i]._Name == object._MaterialName )
              currentMaterial = i;
          }

          const char * currentName = _Map._Materials.empty() ? "" : _Map._Materials[currentMaterial]._Name.c_str();
          if ( ImGui::BeginCombo("Material", currentName) )
          {
            for ( int i = 0; i < static_cast<int>(_Map._Materials.size()); ++i )
            {
              const bool selected = ( i == currentMaterial );
              if ( ImGui::Selectable(_Map._Materials[i]._Name.c_str(), selected) )
              {
                object._MaterialName = _Map._Materials[i]._Name;
                FpsMaterialSlot materialSlot;
                if ( EditorMaterialSlotFromName(object._MaterialName, materialSlot) )
                  object._Material = materialSlot;
                MarkEditorDirty();
                _ReloadScene = true;
              }
              if ( selected )
                ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
          }
        }

        if ( objectDirty )
        {
          object._HalfExtents = MathUtil::Max(object._HalfExtents, Vec3(0.01f));
          SyncEditorObject(index);
          MarkEditorDirty();
        }
      }
    }
  }

  if ( ImGui::CollapsingHeader("Materials") )
  {
    FpsGameMapLoader::SeedDefaultMaterials(_Map);
    if ( !_Map._Materials.empty() )
      _Editor._SelectedMaterial = MathUtil::Clamp(_Editor._SelectedMaterial, 0, static_cast<int>(_Map._Materials.size()) - 1);

    ImGui::InputText("New material", _Editor._NewMaterialName, sizeof(_Editor._NewMaterialName));
    ImGui::SameLine();
    if ( ImGui::Button("Add material") && std::strlen(_Editor._NewMaterialName) > 0 )
    {
      bool exists = false;
      for ( int i = 0; i < static_cast<int>(_Map._Materials.size()); ++i )
      {
        if ( _Map._Materials[i]._Name == _Editor._NewMaterialName )
        {
          _Editor._SelectedMaterial = i;
          exists = true;
          break;
        }
      }

      if ( !exists )
      {
        FpsMapMaterial material;
        material._Name = _Editor._NewMaterialName;
        material._Material._Albedo = Vec3(0.8f);
        material._Material._Roughness = 0.5f;
        _Map._Materials.push_back(material);
        _Editor._SelectedMaterial = static_cast<int>(_Map._Materials.size()) - 1;
        MarkEditorDirty();
        _ReloadScene = true;
      }
    }

    if ( !_Map._Materials.empty() )
    {
      if ( ImGui::BeginListBox("Materials", ImVec2(-FLT_MIN, 130.f)) )
      {
        for ( int i = 0; i < static_cast<int>(_Map._Materials.size()); ++i )
        {
          const bool selected = ( i == _Editor._SelectedMaterial );
          if ( ImGui::Selectable(_Map._Materials[i]._Name.c_str(), selected) )
            _Editor._SelectedMaterial = i;
          if ( selected )
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
      }

      FpsMapMaterial & mapMaterial = _Map._Materials[_Editor._SelectedMaterial];
      Material & material = mapMaterial._Material;
      bool materialDirty = false;
      materialDirty |= ImGui::ColorEdit3("Albedo", &material._Albedo.x);
      materialDirty |= ImGui::DragFloat("Roughness", &material._Roughness, 0.01f, 0.f, 1.f, "%.3f");
      materialDirty |= ImGui::DragFloat("Metallic", &material._Metallic, 0.01f, 0.f, 1.f, "%.3f");
      materialDirty |= ImGui::DragFloat("Reflectance", &material._Reflectance, 0.01f, 0.f, 1.f, "%.3f");
      materialDirty |= ImGui::ColorEdit3("Emission", &material._Emission.x);
      materialDirty |= ImGui::DragFloat("Opacity", &material._Opacity, 0.01f, 0.f, 1.f, "%.3f");
      int alphaMode = static_cast<int>(material._AlphaMode);
      static const char * alphaModes[] = { "Opaque", "Blend", "Mask" };
      if ( ImGui::Combo("Alpha mode", &alphaMode, alphaModes, 3) )
      {
        material._AlphaMode = static_cast<float>(alphaMode);
        materialDirty = true;
      }

      if ( materialDirty )
      {
        material._Roughness = MathUtil::Clamp(material._Roughness, 0.f, 1.f);
        material._Metallic = MathUtil::Clamp(material._Metallic, 0.f, 1.f);
        material._Reflectance = MathUtil::Clamp(material._Reflectance, 0.f, 1.f);
        material._Opacity = MathUtil::Clamp(material._Opacity, 0.f, 1.f);
        if ( _Scene )
        {
          const int materialID = _Scene -> FindMaterialID("__FpsMap_" + mapMaterial._Name);
          if ( ( materialID >= 0 ) && ( materialID < static_cast<int>(_Scene -> GetMaterials().size()) ) )
          {
            const float id = _Scene -> GetMaterials()[materialID]._ID;
            _Scene -> GetMaterials()[materialID] = material;
            _Scene -> GetMaterials()[materialID]._ID = id;
          }
        }
        if ( _Renderer )
          _Renderer -> Notify(DirtyState::SceneMaterials);
        MarkEditorDirty();
      }
    }
  }

  if ( ImGui::CollapsingHeader("Lights") )
  {
    if ( ImGui::Button("Add sphere light") )
    {
      Light light;
      light._Type = (float)LightType::SphereLight;
      light._Pos = _GameWorld.GetPlayer().EyePosition(_GameSettings) + Vec3(0.f, 0.f, 2.f);
      light._Emission = Vec3(1.f, 0.85f, 0.65f);
      light._Intensity = 20.f;
      light._Radius = 0.25f;
      light._Area = 4.0f * static_cast<float>(M_PI) * light._Radius * light._Radius;
      _Map._Lights.push_back(light);
      _Editor._Selection._Kind = FpsEditableKind::Light;
      _Editor._Selection._Index = static_cast<int>(_Map._Lights.size()) - 1;
      MarkEditorDirty();
      _ReloadScene = true;
    }
    ImGui::SameLine();
    if ( ImGui::Button("Add distant light") )
    {
      Light light;
      light._Type = (float)LightType::DistantLight;
      light._Pos = Vec3(-0.4f, 0.8f, -0.3f);
      light._Emission = Vec3(1.f);
      light._Intensity = 2.f;
      light._CastShadow = true;
      _Map._Lights.push_back(light);
      _Editor._Selection._Kind = FpsEditableKind::Light;
      _Editor._Selection._Index = static_cast<int>(_Map._Lights.size()) - 1;
      MarkEditorDirty();
      _ReloadScene = true;
    }

    if ( ImGui::BeginListBox("Lights", ImVec2(-FLT_MIN, 150.f)) )
    {
      for ( int i = 0; i < static_cast<int>(_Map._Lights.size()); ++i )
      {
        const LightType type = (LightType)(int)_Map._Lights[i]._Type;
        const char * typeName = ( LightType::DistantLight == type ) ? "Distant" : ( LightType::RectLight == type ? "Rect" : "Sphere" );
        const bool selected = ( FpsEditableKind::Light == _Editor._Selection._Kind ) && ( _Editor._Selection._Index == i );
        const std::string label = std::string(typeName) + " light##light" + std::to_string(i);
        if ( ImGui::Selectable(label.c_str(), selected) )
        {
          _Editor._Selection._Kind = FpsEditableKind::Light;
          _Editor._Selection._Index = i;
        }
        if ( selected )
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndListBox();
    }

    if ( FpsEditableKind::Light == _Editor._Selection._Kind )
    {
      const int index = _Editor._Selection._Index;
      if ( ( index >= 0 ) && ( index < static_cast<int>(_Map._Lights.size()) ) )
      {
        Light & light = _Map._Lights[index];
        bool lightDirty = false;
        LightType type = (LightType)(int)light._Type;
        int lightType = ( LightType::DistantLight == type ) ? 2 : ( LightType::RectLight == type ? 1 : 0 );
        static const char * lightTypes[] = { "Sphere", "Rect", "Distant" };
        if ( ImGui::Combo("Type", &lightType, lightTypes, 3) )
        {
          if ( 0 == lightType )
            light._Type = (float)LightType::SphereLight;
          else if ( 1 == lightType )
            light._Type = (float)LightType::RectLight;
          else
            light._Type = (float)LightType::DistantLight;
          lightDirty = true;
        }

        type = (LightType)(int)light._Type;
        if ( LightType::DistantLight == type )
          lightDirty |= ImGui::DragFloat3("Direction", &light._Pos.x, 0.01f, -1.f, 1.f, "%.3f");
        else
          lightDirty |= ImGui::DragFloat3("Position", &light._Pos.x, 0.05f, -100.f, 100.f, "%.3f");

        lightDirty |= ImGui::ColorEdit3("Emission", &light._Emission.x);
        lightDirty |= ImGui::DragFloat("Intensity", &light._Intensity, 0.1f, 0.f, 500.f, "%.2f");
        if ( LightType::SphereLight == type )
          lightDirty |= ImGui::DragFloat("Radius", &light._Radius, 0.01f, 0.01f, 20.f, "%.3f");
        if ( LightType::RectLight == type )
        {
          lightDirty |= ImGui::DragFloat3("Dir U", &light._DirU.x, 0.05f, -20.f, 20.f, "%.3f");
          lightDirty |= ImGui::DragFloat3("Dir V", &light._DirV.x, 0.05f, -20.f, 20.f, "%.3f");
          lightDirty |= ImGui::DragFloat("Area", &light._Area, 0.01f, 0.01f, 400.f, "%.3f");
        }
        lightDirty |= ImGui::Checkbox("Cast shadow", &light._CastShadow);
        lightDirty |= ImGui::DragFloat("Shadow radius", &light._ShadowRadius, 0.05f, 0.f, 200.f, "%.2f");

        if ( lightDirty )
        {
          if ( LightType::SphereLight == (LightType)(int)light._Type )
            light._Area = 4.0f * static_cast<float>(M_PI) * light._Radius * light._Radius;
          SyncEditorLight(index);
          MarkEditorDirty();
        }
      }
    }
  }

  if ( ImGui::CollapsingHeader("Player spawn") )
  {
    bool spawnDirty = false;
    spawnDirty |= ImGui::DragFloat3("Spawn position", &_Map._Player._Position.x, 0.05f, -100.f, 100.f, "%.3f");
    spawnDirty |= ImGui::DragFloat("Spawn yaw", &_Map._Player._Yaw, 0.5f, -360.f, 360.f, "%.2f");
    spawnDirty |= ImGui::DragFloat("Spawn pitch", &_Map._Player._Pitch, 0.5f, -89.f, 89.f, "%.2f");
    if ( ImGui::Button("Use current camera") )
    {
      const FpsPlayer & player = _GameWorld.GetPlayer();
      _Map._Player._Position = player._Position;
      _Map._Player._Yaw = player._Yaw;
      _Map._Player._Pitch = player._Pitch;
      spawnDirty = true;
    }
    if ( spawnDirty )
      MarkEditorDirty();
  }

  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawEditorGizmo
// ----------------------------------------------------------------------------
int Test6::DrawEditorGizmo()
{
  if ( !_Editor._Enabled || !_Scene || !_Renderer )
    return 0;

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);

  Camera & camera = _Scene -> GetCamera();
  camera.ComputeLookAtMatrix(view);

  const float width  = static_cast<float>(std::max(1, _Settings._WindowResolution.x));
  const float height = static_cast<float>(std::max(1, _Settings._WindowResolution.y));
  camera.ComputePerspectiveProjMatrix(width / height, proj);

  ImGuiIO & io = ImGui::GetIO();
  ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
  ImGuizmo::SetRect(0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y);
  ImGuizmo::SetOrthographic(false);

  if ( ( FpsEditableKind::Box == _Editor._Selection._Kind )
    || ( FpsEditableKind::Collider == _Editor._Selection._Kind ) )
  {
    const int index = _Editor._Selection._Index;
    if ( ( index < 0 ) || ( index >= static_cast<int>(_Map._Objects.size()) ) )
      return 0;

    FpsSceneObject & object = _Map._Objects[index];
    Mat4x4 transform = glm::translate(object._Center);
    if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                              glm::value_ptr(transform)) )
    {
      object._Center = Vec3(transform[3]);
      SyncEditorObject(index);
      MarkEditorDirty();
    }
  }
  else if ( FpsEditableKind::Light == _Editor._Selection._Kind )
  {
    const int index = _Editor._Selection._Index;
    if ( ( index < 0 ) || ( index >= static_cast<int>(_Map._Lights.size()) ) )
      return 0;

    Light & light = _Map._Lights[index];
    if ( LightType::DistantLight == (LightType)(int)light._Type )
      return 0;

    Mat4x4 transform(1.f);
    transform[3] = Vec4(light._Pos, 1.f);
    if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                              glm::value_ptr(transform)) )
    {
      light._Pos = Vec3(transform[3]);
      SyncEditorLight(index);
      MarkEditorDirty();
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawHUD
// ----------------------------------------------------------------------------
void Test6::DrawHUD()
{
  const ImGuiIO & io = ImGui::GetIO();
  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  const FpsPlayer & player = _GameWorld.GetPlayer();

  const float margin = 24.f;
  const float barWidth = 180.f;
  const float barHeight = 16.f;
  const float lineHeight = 26.f;
  const ImVec2 origin(margin, io.DisplaySize.y - margin - lineHeight * 3.f);
  const ImU32 bgColor = IM_COL32(16, 16, 18, 210);
  const ImU32 textColor = IM_COL32(245, 245, 245, 255);

  struct HudBar
  {
    const char * _Label;
    int          _Value;
    int          _MaxValue;
    ImU32        _Color;
  };

  const HudBar bars[] =
  {
    { "HEALTH", player._Health, _GameSettings._MaxHealth, IM_COL32(220, 42, 42, 235) },
    { "ARMOR", player._Armor, _GameSettings._MaxArmor, IM_COL32(64, 142, 255, 235) },
    { "PROJECTILES", _GameWorld.GetProjectileAmmo(), _GameSettings._MaxProjectileAmmo, IM_COL32(235, 190, 48, 235) }
  };

  for ( int i = 0; i < 3; ++i )
  {
    const HudBar & bar = bars[i];
    const float y = origin.y + lineHeight * i;
    const int maxValue = std::max(1, bar._MaxValue);
    const int value = MathUtil::Clamp(bar._Value, 0, maxValue);
    const float ratio = static_cast<float>(value) / static_cast<float>(maxValue);
    const ImVec2 barMin(origin.x, y + 4.f);
    const ImVec2 barMax(origin.x + barWidth, y + 4.f + barHeight);
    const ImVec2 fillMax(origin.x + barWidth * ratio, barMax.y);
    const std::string label = std::string(bar._Label) + " " + std::to_string(value) + " / " + std::to_string(maxValue);

    drawList -> AddRectFilled(barMin, barMax, bgColor, 2.f);
    drawList -> AddRectFilled(barMin, fillMax, bar._Color, 2.f);
    drawList -> AddRect(barMin, barMax, IM_COL32(255, 255, 255, 90), 2.f);
    drawList -> AddText(ImVec2(origin.x + 8.f, y + 5.f), textColor, label.c_str());
  }

  DrawCrosshair();
}

// ----------------------------------------------------------------------------
// DrawCrosshair
// ----------------------------------------------------------------------------
void Test6::DrawCrosshair()
{
  const ImGuiIO & io = ImGui::GetIO();
  const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  const float gap = 3.f;
  const float length = 10.f;
  const float thickness = 2.f;
  const ImU32 color = IM_COL32(255, 24, 24, 255);

  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  drawList -> AddLine(ImVec2(center.x - length, center.y), ImVec2(center.x - gap, center.y), color, thickness);
  drawList -> AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + length, center.y), color, thickness);
  drawList -> AddLine(ImVec2(center.x, center.y - length), ImVec2(center.x, center.y - gap), color, thickness);
  drawList -> AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + length), color, thickness);
}

// ----------------------------------------------------------------------------
// DrawSettingsUI
// ----------------------------------------------------------------------------
int Test6::DrawSettingsUI()
{
  if ( ImGui::CollapsingHeader("Game tuning") )
  {
    ImGui::SliderFloat("Move speed", &_GameSettings._MoveSpeed, 1.f, 12.f);
    ImGui::SliderFloat("Sprint speed", &_GameSettings._SprintSpeed, 1.f, 16.f);
    ImGui::SliderFloat("Mouse sensitivity", &_GameSettings._MouseSensitivity, 0.01f, 0.25f);
    ImGui::SliderFloat("Player radius", &_GameSettings._PlayerRadius, 0.15f, 0.8f);
    ImGui::SliderFloat("Player height", &_GameSettings._PlayerHeight, 1.f, 2.4f);
    ImGui::SliderFloat("Gravity", &_GameSettings._Gravity, 1.f, 35.f);
    ImGui::SliderFloat("Jump speed", &_GameSettings._JumpSpeed, 1.f, 12.f);
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

  if ( ImGui::CollapsingHeader("Render settings") && _Renderer )
  {
    int renderScale = _Settings._RenderScale;
    if ( ImGui::SliderInt("Render scale", &renderScale, 25, 150) )
    {
      _Settings._RenderScale = renderScale;
      SyncFramebufferResolution(true);
    }

    if ( ImGui::Checkbox("Show lights", &_Settings._ShowLights) )
      _Renderer -> Notify(DirtyState::SceneLights);

    if ( ImGui::Checkbox("Tone mapping", &_Settings._ToneMapping) )
      _Renderer -> Notify(DirtyState::RenderSettings);

    if ( _Settings._ToneMapping )
    {
      if ( ImGui::SliderFloat("Gamma", &_Settings._Gamma, 0.5f, 3.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("Exposure", &_Settings._Exposure, 0.1f, 5.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
    }

    if ( FpsRendererMode::Deferred == _GameSettings._RendererMode )
    {
      if ( ImGui::Checkbox("Shadow mapping", &_Settings._ShadowMapping) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      int shadowMapResolution = _Settings._ShadowMapResolution;
      if ( ImGui::SliderInt( "Shadow map resolution", &shadowMapResolution, 256, 4096 ) )
      {
        shadowMapResolution = std::max(256, ( shadowMapResolution / 64 ) * 64);
        _Settings._ShadowMapResolution = shadowMapResolution;
        _Renderer -> Notify(DirtyState::RenderSettings);
      }
      if ( ImGui::SliderFloat( "Shadow bias", &_Settings._ShadowBias, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox("SSAO", &_Settings._SSAO) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::Checkbox("SSAO blur", &_Settings._SSAOBlur) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSAO radius", &_Settings._SSAORadius, 0.05f, 5.f, "%.3f", ImGuiSliderFlags_Logarithmic) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSAO bias", &_Settings._SSAOBias, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSAO intensity", &_Settings._SSAOIntensity, 0.f, 3.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      int ssaoKernelSize = _Settings._SSAOKernelSize;
      if ( ImGui::SliderInt("SSAO kernel size", &ssaoKernelSize, 4, 32) )
      {
        _Settings._SSAOKernelSize = std::max(4, std::min(32, ssaoKernelSize));
        _Renderer -> Notify(DirtyState::RenderSettings);
      }
      if ( ImGui::SliderInt("Max shadow casting lights", &_Settings._MaxShadowCastingLights, 1, 8) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox("SSR", &_Settings._SSR) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSR intensity", &_Settings._SSRIntensity, 0.f, 2.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSR max roughness", &_Settings._SSRMaxRoughness, 0.05f, 1.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      int ssrMaxSteps = _Settings._SSRMaxSteps;
      if ( ImGui::SliderInt("SSR max steps", &ssrMaxSteps, 4, 128) )
      {
        _Settings._SSRMaxSteps = std::max(4, std::min(128, ssrMaxSteps));
        _Renderer -> Notify(DirtyState::RenderSettings);
      }
      if ( ImGui::SliderFloat("SSR step size", &_Settings._SSRStepSize, 0.01f, 1.f, "%.3f", ImGuiSliderFlags_Logarithmic) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSR max distance", &_Settings._SSRMaxDistance, 1.f, 100.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSR thickness", &_Settings._SSRThickness, 0.01f, 2.f, "%.3f", ImGuiSliderFlags_Logarithmic) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("SSR edge fade", &_Settings._SSRFade, 0.01f, 0.5f) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox("PBR direct lighting", &_Settings._PBRDirectLighting) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("Direct light intensity", &_Settings._DirectLightIntensity, 0.f, 8.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderFloat("IBL max roughness", &_Settings._SpecularIBLMaxRoughness, 0.05f, 1.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      static const char * DEBUG_VIEWS[] = { "Color", "Depth", "Normals", "Shadows", "SSAO", "Specular IBL", "Material Params", "SSR", "Direct diffuse", "Direct specular" };
      static int bufferChoice = 0;
      static bool showWires = false;
      if ( ImGui::Combo("Debug view", &bufferChoice, DEBUG_VIEWS, 10) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::Checkbox("Show wires", &showWires) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      g_Test6DebugMode = 0;
      _Settings._ShowShadowMap = ( 3 == bufferChoice );
      if ( 1 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::DepthBuffer;
      else if ( 2 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::Normals;
      else if ( 3 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::Shadows;
      else if ( 4 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::SSAO;
      else if ( 5 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::SpecularIBL;
      else if ( 6 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::MaterialParams;
      else if ( 7 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::SSR;
      else if ( 8 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::DirectDiffuse;
      else if ( 9 == bufferChoice )
        g_Test6DebugMode |= (int)DeferredDebugModes::DirectSpecular;

      if ( showWires )
        g_Test6DebugMode |= (int)DeferredDebugModes::Wires;
      _Renderer -> SetDebugMode(g_Test6DebugMode);
    }
    else if ( FpsRendererMode::Software == _GameSettings._RendererMode )
    {
      int numThreads = (int)_Settings._NbThreads;
      if ( ImGui::SliderInt("Nb threads", &numThreads, 1, (int)std::max(1u, g_Test6NbThreadsMax)) )
      {
        _Settings._NbThreads = std::max(1, numThreads);
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

      static const char * DEBUG_VIEWS[] = { "Color", "Depth", "Normals" };
      static int bufferChoice = 0;
      static bool showWires = false;
      if ( ImGui::Combo("Debug view", &bufferChoice, DEBUG_VIEWS, 3) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::Checkbox("Show wires", &showWires) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      g_Test6DebugMode = 0;
      if ( 1 == bufferChoice )
        g_Test6DebugMode |= (int)RasterDebugModes::DepthBuffer;
      else if ( 2 == bufferChoice )
        g_Test6DebugMode |= (int)RasterDebugModes::Normals;
      if ( showWires )
        g_Test6DebugMode |= (int)RasterDebugModes::Wires;
      _Renderer -> SetDebugMode(g_Test6DebugMode);
    }
    else if ( FpsRendererMode::PhotoPathTracer == _GameSettings._RendererMode )
    {
      if ( ImGui::SliderInt("Bounces", &_Settings._Bounces, 1, 8) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::SliderInt("SPP", &_Settings._NbSamplesPerPixel, 1, 8) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      if ( ImGui::Checkbox("Denoise", &_Settings._Denoise) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      static const char * DEBUG_VIEWS[] = { "Off", "Tiles", "Albedo", "Metalness", "Roughness", "Normals", "UV", "BLAS" };
      if ( ImGui::Combo("Debug view", &g_Test6DebugMode, DEBUG_VIEWS, 8) )
      {
        _Renderer -> SetDebugMode(g_Test6DebugMode);
        _Renderer -> Notify(DirtyState::RenderSettings);
      }
    }
  }

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

    glfwMakeContextCurrent(_MainWindow.get());
    glfwSwapInterval(0);

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

    glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    SyncFramebufferResolution();
    glDisable(GL_DEPTH_TEST);

    while ( !glfwWindowShouldClose(_MainWindow.get()) )
    {
      UpdateCPUTime();

      if ( 0 != UpdateGame() )
        break;

      if ( 0 != ProcessInput() )
        break;

      _Renderer -> Update();
      _Renderer -> RenderToTexture();
      _Renderer -> RenderToScreen();

      if ( _RenderToFile )
      {
        _Renderer -> RenderToFile(_CaptureOutputPath);
        _RenderToFile = false;
      }

      _Renderer -> Done();

      DrawUI();

      glfwSwapBuffers(_MainWindow.get());
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

  return ret;
}

}
