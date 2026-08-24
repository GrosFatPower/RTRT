#pragma warning(disable : 4100) // unreferenced formal parameter

#include "Test5.h"
#include "RendererFactory.h"

#include "DroppedFileUtils.h"
#include "Loader.h"
#include "PathTracer.h"
#include "SoftwareRasterizer.h"
#include "DeferredRenderer.h"
#include "Util.h"
#include "PathUtils.h"
#include "Mesh.h"
#include "NativeFileDialog.h"
#include "RenderStatsUI.h"

#include <string>
#include <iostream>
#include <thread>
#include <limits>
#include <cmath>

#include "imgui.h"
#include "ImGuizmo.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "glm/gtc/type_ptr.hpp"

namespace RTRT
{

const char * Test5::GetTestHeader() { return "Renderers"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static int g_DebugMode    = 0;
static int g_FStopMode    = 0;
static unsigned int g_NbThreadsMax = std::thread::hardware_concurrency();

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void Test5::KeyCallback(GLFWwindow* iWindow, const int iKey, const int iScancode, const int iAction, const int iMods)
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  if ( (iAction == GLFW_PRESS ) ||  (iAction == GLFW_RELEASE ) )
    this_ -> _KeyInput.AddEvent(iKey, iAction, iMods);
}

// ----------------------------------------------------------------------------
// MouseButtonCallback
// ----------------------------------------------------------------------------
void Test5::MouseButtonCallback(GLFWwindow* iWindow, const int iButton, const int iAction, const int iMods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(iWindow));
    if ( !this_ )
      return;

    double mouseX = 0., mouseY = 0.;
    glfwGetCursorPos(iWindow, &mouseX, &mouseY);

    if ( (iAction == GLFW_PRESS ) ||  (iAction == GLFW_RELEASE ) )
      this_ -> _MouseInput.AddButtonEvent(iButton, iAction, mouseX, mouseY);
  }
}

// ----------------------------------------------------------------------------
// MouseScrollCallback
// ----------------------------------------------------------------------------
void Test5::MouseScrollCallback(GLFWwindow* iWindow, const double iOffsetX, const double iOffsetY)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(iWindow));
    if ( !this_ )
      return;

    this_ -> _MouseInput.AddScrollEvent(iOffsetX, iOffsetY);
  }
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void Test5::FramebufferSizeCallback(GLFWwindow* iWindow, const int iWidth, const int iHeight)
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << iWidth << " HEIGHT = " << iHeight << std::endl;

  if ( !iWidth || !iHeight)
    return;

  this_ -> SyncFramebufferResolution( true );
}

// ----------------------------------------------------------------------------
// DropCallback
// ----------------------------------------------------------------------------
void Test5::DropCallback( GLFWwindow * iWindow, int iCount, const char ** iPaths )
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(iWindow));
  if ( !this_ )
    return;

  this_ -> HandleDroppedFiles(iCount, iPaths);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Test5::Test5( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: BaseTest(iMainWindow, iScreenWidth, iScreenHeight)
{
  _Settings._WindowResolution.x = iScreenWidth;
  _Settings._WindowResolution.y = iScreenHeight;
  _Settings._RenderResolution.x = iScreenWidth;
  _Settings._RenderResolution.y = iScreenHeight;
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test5::~Test5()
{
  for ( auto fileName : _SceneNames )
    delete[] fileName;
  for ( auto fileName : _BackgroundNames )
    delete[] fileName;

  GLUtil::DeleteTEX(_AlbedoTEX);
  GLUtil::DeleteTEX(_MetalRoughTEX);
  GLUtil::DeleteTEX(_NormalMapTEX);
  GLUtil::DeleteTEX(_EmissionMapTEX);
}

// ----------------------------------------------------------------------------
// SyncFramebufferResolution
// ----------------------------------------------------------------------------
void Test5::SyncFramebufferResolution( bool iNotifyRenderer )
{
  if ( !_MainWindow )
    return;

  int frameBufferWidth = 0;
  int frameBufferHeight = 0;
  glfwGetFramebufferSize( _MainWindow.get(), &frameBufferWidth, &frameBufferHeight );

  if ( frameBufferWidth <= 0 || frameBufferHeight <= 0 )
    return;

  _Settings._WindowResolution.x = frameBufferWidth;
  _Settings._WindowResolution.y = frameBufferHeight;
  _Settings._RenderResolution.x = int( _Settings._WindowResolution.x * ( _Settings._RenderScale * 0.01f ) );
  _Settings._RenderResolution.y = int( _Settings._WindowResolution.y * ( _Settings._RenderScale * 0.01f ) );

  glViewport( 0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y );

  if ( iNotifyRenderer && _Renderer )
    _Renderer -> Notify(DirtyState::RenderSettings);
}

// ----------------------------------------------------------------------------
// HandleDroppedFiles
// ----------------------------------------------------------------------------
void Test5::HandleDroppedFiles( int iCount, const char ** iPaths )
{
  if ( !iPaths || ( iCount <= 0 ) )
    return;

  for ( int i = 0; i < iCount; ++i )
  {
    if ( !iPaths[i] || !iPaths[i][0] )
      continue;

    const std::filesystem::path filepath(DroppedFileUtils::NormalizeDroppedPath(iPaths[i]));
    if ( !DroppedFileUtils::IsDroppedScenePath(filepath) )
    {
      std::cout << "Test5 : unsupported dropped file " << DroppedFileUtils::DisplayName(filepath) << std::endl;
      continue;
    }

    std::error_code ec;
    if ( !std::filesystem::exists(filepath, ec) || ec )
    {
      std::cout << "Test5 : dropped scene does not exist " << filepath.generic_string() << std::endl;
      continue;
    }

    _CurSceneId = AddExternalSceneFile(filepath);
    _LastExternalSceneDirectory = filepath.parent_path();
    _SceneLoadError.clear();
    _ReloadScene = true;
    std::cout << "Test5 : dropped scene " << filepath.generic_string() << std::endl;
    return;
  }
}

// ----------------------------------------------------------------------------
// AddExternalSceneFile
// ----------------------------------------------------------------------------
int Test5::AddExternalSceneFile( const std::filesystem::path & iFilepath )
{
  std::error_code ec;
  std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(iFilepath, ec);
  if ( ec )
  {
    ec.clear();
    normalizedPath = std::filesystem::absolute(iFilepath, ec).lexically_normal();
  }
  const std::string scenePath = normalizedPath.string();

  for ( int i = 0; i < static_cast<int>(_SceneFiles.size()); ++i )
  {
    if ( _SceneFiles[i] == scenePath )
      return i;
  }

  std::string sceneName = DroppedFileUtils::DisplayName(normalizedPath);
  const std::string baseName = sceneName;
  int suffix = 2;
  bool duplicateName = true;
  while ( duplicateName )
  {
    duplicateName = false;
    for ( const char * existingName : _SceneNames )
    {
      if ( sceneName == existingName )
      {
        sceneName = baseName + " (" + std::to_string(suffix++) + ")";
        duplicateName = true;
        break;
      }
    }
  }

  char * const sceneNameBuffer = new char[sceneName.size() + 1u];
  snprintf(sceneNameBuffer, sceneName.size() + 1u, "%s", sceneName.c_str());
  _SceneFiles.push_back(scenePath);
  _SceneNames.push_back(sceneNameBuffer);
  return static_cast<int>(_SceneFiles.size()) - 1;
}

// ----------------------------------------------------------------------------
// LoadSceneFromDialog
// ----------------------------------------------------------------------------
void Test5::LoadSceneFromDialog()
{
  std::filesystem::path initialDirectory = _LastExternalSceneDirectory.empty() ? std::filesystem::path(PathUtils::GetAssetPath("")) : _LastExternalSceneDirectory;
  std::filesystem::path selectedPath;
  std::string error;
  const NativeFileDialogResult result = OpenFileDialog("scene,obj,gltf,glb", initialDirectory, selectedPath, error);
  if ( NativeFileDialogResult::Cancelled == result )
    return;
  if ( NativeFileDialogResult::Error == result )
  {
    _SceneLoadError = "File dialog error: " + error;
    return;
  }

  std::error_code ec;
  selectedPath = std::filesystem::absolute(selectedPath, ec).lexically_normal();
  if ( ec || !std::filesystem::is_regular_file(selectedPath, ec) || ec )
  {
    _SceneLoadError = "Selected scene file does not exist.";
    return;
  }

  _CurSceneId = AddExternalSceneFile(selectedPath);
  _LastExternalSceneDirectory = selectedPath.parent_path();
  _SceneLoadError.clear();
  _ReloadScene = true;
}

// ----------------------------------------------------------------------------
// InitializeSceneFiles
// ----------------------------------------------------------------------------
int Test5::InitializeSceneFiles()
{
  std::vector<std::string> sceneNames;
  Util::RetrieveSceneFiles(PathUtils::GetAssetPath(""), _SceneFiles, &sceneNames);

  for ( int i = 0; i < sceneNames.size(); ++i )
  {
    char * filename = new char[256];
    snprintf(filename, 256, "%s", sceneNames[i].c_str());
    _SceneNames.push_back(filename);

    if ( "TexturedBox.scene" == sceneNames[i] )
      _CurSceneId = i;
  }

  if ( _CurSceneId < 0 )
  {
    if ( sceneNames.size() )
      _CurSceneId = 0;
    else
      return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeBackgroundFiles
// ----------------------------------------------------------------------------
int Test5::InitializeBackgroundFiles()
{
  std::vector<std::string> backgroundNames;
  Util::RetrieveBackgroundFiles(PathUtils::GetEnvMapPath(""), _BackgroundFiles, &backgroundNames);

  for ( int i = 0; i < backgroundNames.size(); ++i )
  {
    char * filename = new char[256];
    snprintf(filename, 256, "%s", backgroundNames[i].c_str());
    _BackgroundNames.push_back(filename);

    if ( "alps_field_2k.hdr" == backgroundNames[i] )
      _CurBackgroundId = i;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeUI
// ----------------------------------------------------------------------------
int Test5::InitializeUI()
{
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Load Fonts
  io.Fonts -> AddFontDefault();

  // Setup Platform/Renderer backends
  const char* glsl_version = "#version 410";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
int Test5::DrawUI()
{
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

  {
    ImGui::Begin("Test 5 : Viewer");

    // Renderer selection
    {
      static const char * Renderers[] = {"PathTracer", "SoftwareRasterizer", "OpenGLRasterizer"};
      //_RendererType
      int selectedRenderer = (int)_RendererType;
      if ( ImGui::Combo( "Renderer", &selectedRenderer, Renderers, 3 ) )
      {
        _RendererType = (RendererType) selectedRenderer;
        _ReloadRenderer = true;
      }
    }

    // Scene selection
    {
      int selectedSceneId = _CurSceneId;
      if ( ImGui::Combo("Scene selection", &selectedSceneId, &_SceneNames[0], static_cast<int>(_SceneNames.size())) )
      {
        if ( selectedSceneId != _CurSceneId )
        {
          _CurSceneId = selectedSceneId;
          _SceneLoadError.clear();
          _ReloadScene = true;
        }
      }

      if ( ImGui::Button("Load scene...") )
        LoadSceneFromDialog();

      if ( !_SceneLoadError.empty() )
        ImGui::TextColored(ImVec4(1.f, .35f, .35f, 1.f), "%s", _SceneLoadError.c_str());
    }

    ImGui::Checkbox("Show rendering stats", &_ShowRenderStatsPanel);

    if ( ImGui::Button( "Capture image" ) )
    {
      const std::string sceneName = std::string(_SceneNames[_CurSceneId]);
      _CaptureOutputPath = "./" + sceneName + "_" + std::to_string( _NbRenderedFrames ) + "frames.png";
      _RenderToFile = true;
    }

    DrawSettingsUI();
    DrawCameraUI();
    DrawMeshInstanceUI();
    DrawBoidsUI();
    DrawBackgroundUI();
    DrawMaterialsUI();
    DrawLightsUI();

    ImGui::End();
  }

  DrawRenderStatsUI();

  if ( _SelectedLightID >= 0 )
    DrawLightGizmo();
  else
  {
    DrawSelectedMeshInstanceBBox();
    DrawMeshInstanceGizmo();
  }

  // Rendering
  ImGui::Render();

  //const ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  //glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  //glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
  //glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  return 0;
}

// ----------------------------------------------------------------------------
// NotifyMeshInstanceEdited
// ----------------------------------------------------------------------------
void Test5::NotifyMeshInstanceEdited()
{
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::SceneInstances);
}

// ----------------------------------------------------------------------------
// NotifyLightEdited
// ----------------------------------------------------------------------------
void Test5::NotifyLightEdited()
{
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::SceneLights);
}

// ----------------------------------------------------------------------------
// NotifyBoidsInstancesEdited
// ----------------------------------------------------------------------------
void Test5::NotifyBoidsInstancesEdited()
{
  if ( _Renderer )
    _Renderer -> Notify(DirtyState::SceneInstances);
}

// ----------------------------------------------------------------------------
// ComputeBoidsBoundsFromScene
// ----------------------------------------------------------------------------
void Test5::ComputeBoidsBoundsFromScene()
{
  if ( !_Scene )
    return;

  Vec3 low( MAX_FLOAT );
  Vec3 high( -MAX_FLOAT );
  bool hasBounds = false;

  const std::vector<MeshInstance> & meshInstances = _Scene -> GetMeshInstances();
  const std::vector<Mesh*>        & meshes        = _Scene -> GetMeshes();

  for ( int i = 0; i < static_cast<int>(meshInstances.size()); ++i )
  {
    if ( _BoidsBinding.ContainsInstanceID(i) )
      continue;

    const MeshInstance & inst = meshInstances[i];
    if ( !inst._Visible )
      continue;

    if ( ( inst._MeshID < 0 ) || ( inst._MeshID >= static_cast<int>(meshes.size()) ) )
      continue;

    Mesh * mesh = meshes[inst._MeshID];
    if ( !mesh )
      continue;

    Vec3 corners[8];
    mesh -> GetBoundingBox().Corners(corners);
    for ( int c = 0; c < 8; ++c )
    {
      const Vec3 worldCorner = MathUtil::TransformPoint(corners[c], inst._Transform);
      MathUtil::Minimize(low, worldCorner);
      MathUtil::Maximize(high, worldCorner);
      hasBounds = true;
    }
  }

  if ( !hasBounds )
  {
    _BoidsSettings._BoundsCenter = Vec3(0.f);
    _BoidsSettings._BoundsRadius = 4.f;
    _BoidsSettings._BoundsHeight = 3.f;
    return;
  }

  const Vec3 center = 0.5f * ( low + high );
  const Vec3 extent = high - low;
  const float horizontalExtent = std::max(extent.x, extent.z);

  _BoidsSettings._BoundsCenter = center;
  _BoidsSettings._BoundsRadius = std::max(4.f, horizontalExtent * 0.75f + 1.f);
  _BoidsSettings._BoundsHeight = std::max(3.f, extent.y * 1.5f + 1.f);
}

// ----------------------------------------------------------------------------
// DrawRenderStatsUI
// ----------------------------------------------------------------------------
int Test5::DrawRenderStatsUI()
{
  if ( !_ShowRenderStatsPanel )
    return 0;

  if ( !ImGui::Begin("Test5 Rendering Stats", &_ShowRenderStatsPanel) )
  {
    ImGui::End();
    return 0;
  }

  RenderStatsUI::DrawFrameRateGraph(_RenderStatsState, _FrameRate, _DeltaTime, _NbRenderedFrames);
  RenderStatsUI::DrawRenderOverview(_Settings, _FrameTime, _NbRenderedFrames);
  RenderStatsUI::DrawRenderPassTimings(_Renderer.get());
  RenderStatsUI::DrawPathTracerStats(_Renderer.get());
  RenderStatsUI::DrawSceneStats(_Scene.get());

  ImGui::End();

  return 0;
}

// ----------------------------------------------------------------------------
// DrawSettingsUI
// ----------------------------------------------------------------------------
int Test5::DrawSettingsUI()
{
  if (ImGui::CollapsingHeader("Settings"))
  {
    static const char * YESorNO[] = { "No", "Yes" };

    static bool vSync = false;
    if ( ImGui::Checkbox( "VSync", &vSync ) )
    {
      if ( vSync )
        glfwSwapInterval( 1 );
      else
        glfwSwapInterval( 0 );
    }

    if ( RendererType::SoftwareRasterizer == _RendererType )
    {
      int numThreads = _Settings._NbThreads;
      if ( ImGui::SliderInt("Nb Threads", &numThreads, 1, g_NbThreadsMax) && ( numThreads > 0 ) )
      {
        _Settings._NbThreads = numThreads;
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

      if (ImGui::Checkbox("Tiled rendering", &_Settings._TiledRendering))
      {
        _Renderer->Notify(DirtyState::RenderSettings);
      }

      SoftwareRasterizer * softwareRasterizer = _Renderer -> AsSoftwareRasterizer();
      if ( softwareRasterizer )
      {
        if (_Settings._TiledRendering)
        {
          static const char * TILE_SIZE[] = { "16", "32", "64", "128", "256", "512" };

          unsigned int tileSize = softwareRasterizer -> GetTileSize();
          tileSize = tileSize >> 4;
          int curIndex = 0;
          while ( !(tileSize & 1) && ( curIndex < 5 ) )
          {
            tileSize = tileSize >> 1;
            curIndex++;
          }

          if ( ImGui::Combo( "Tile Size", &curIndex, TILE_SIZE, 6 ) )
          {
            softwareRasterizer -> SetTileSize(atoi(TILE_SIZE[curIndex]));
            _Renderer -> Notify(DirtyState::RenderSettings);
          }

          if ( SIMDUtils::HasSIMDSupport() )
          {
            bool enableSIMD = softwareRasterizer -> GetEnableSIMD();
            if (ImGui::Checkbox("Enable SIMD", &enableSIMD))
            {
              softwareRasterizer->SetEnableSIMD(enableSIMD);
              _Renderer->Notify(DirtyState::RenderSettings);
            }
          }
        }
      }
    }

    int scale = _Settings._RenderScale;
    if ( ImGui::SliderInt("Render scale", &scale, 5, 200) )
    {
      _Settings._RenderScale = scale;
      _Renderer -> Notify(DirtyState::RenderSettings);
    }

    if ( RendererType::PathTracer == _RendererType )
    {
      if ( ImGui::SliderFloat( "Interactive res Ratio", &_Settings._LowResRatio, 0.05f, 1.f ) )
      {
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

      if ( ImGui::SliderInt( "SPP", &_Settings._NbSamplesPerPixel, 1, 10 ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::SliderInt( "Bounces", &_Settings._Bounces, 1, 10 ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox( "Russian Roulette", &_Settings._RussianRoulette) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox( "Accumulate", &_Settings._Accumulate ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox( "Tiled rendering", &_Settings._TiledRendering ) )
      {
        if ( _Settings._TiledRendering && ( ( _Settings._TileResolution.x <= 0 ) || ( _Settings._TileResolution.y <= 0 ) ) )
          _Settings._TileResolution.x = _Settings._TileResolution.y = 256;
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

      if ( _Settings._TiledRendering )
      {
        int tileSize = _Settings._TileResolution.x;
        if ( ImGui::SliderInt("Tile size", &tileSize, 64, 1024) )
        {
          _Settings._TileResolution = Vec2i(tileSize);
          _Renderer -> Notify(DirtyState::RenderSettings);
        }
      }

      if ( ImGui::Checkbox( "Denoise", &_Settings._Denoise ) )
      {}

      if ( _Settings._Denoise )
      {
        static const char * DENOISING_METHODS[] = { "Bilateral", "Wavelet", "Edge-aware" };
        int denoisingMethod = (int)_Settings._DenoisingMethod;
        if ( ImGui::Combo( "Denoising method", &denoisingMethod, DENOISING_METHODS, 3 ) )
        {
          _Settings._DenoisingMethod = denoisingMethod;
        }

        if ( 0 == _Settings._DenoisingMethod )
        {
          if ( ImGui::SliderFloat( "Sigma spatial", &_Settings._DenoiserSigmaSpatial, 0.1f, 10.f ) )
          {}

          if ( ImGui::SliderFloat( "Sigma range", &_Settings._DenoiserSigmaRange, 0.01f, 1.f ) )
          {}
        }
        else if ( 1 == _Settings._DenoisingMethod )
        {
          if ( ImGui::SliderInt( "Wavelet scale", (int*)&_Settings._DenoisingWaveletScale, 1, 5 ) )
          {}

          if ( ImGui::SliderFloat( "Threshold", &_Settings._DenoiserThreshold, .01f, 1.f ) )
          {}
        }
        else if ( 2 == _Settings._DenoisingMethod )
        {
          if ( ImGui::SliderFloat( "Color phi", &_Settings._DenoiserColorPhi, 0.01f, 1.f ) )
          {}

          if ( ImGui::SliderFloat( "Normal phi", &_Settings._DenoiserNormalPhi, 0.01f, 1.f ) )
          {}

          if ( ImGui::SliderFloat( "Position phi", &_Settings._DenoiserPositionPhi, 0.01f, 1.f ) )
          {}
        }
      }
    }
    else if ( RendererType::SoftwareRasterizer == _RendererType )
    {
      static const char * NEARESTorBILNEAR[] = { "Nearest", "Bilinear", "Trilinear" };
      static const char * PHONGorFLATorPBR[]      = { "Flat", "Phong", "PBR" };

      int sampling = (int)_Settings._Sampling;
      if ( ImGui::Combo("Texture sampling", &sampling, NEARESTorBILNEAR, 3) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      _Settings._Sampling = (SamplingMode)sampling;

      int shadingType = (int)_Settings._ShadingType;
      if ( ImGui::Combo("Shading", &shadingType, PHONGorFLATorPBR, 3) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      _Settings._ShadingType = (ShadingType)shadingType;

      int useWBuffer = !!_Settings._WBuffer;
      if ( ImGui::Combo("W-Buffer", &useWBuffer, YESorNO, 2) )
        _Renderer -> Notify(DirtyState::RenderSettings);
      _Settings._WBuffer = !!useWBuffer;

      SoftwareRasterizer * softwareRasterizer = _Renderer -> AsSoftwareRasterizer();
      if ( softwareRasterizer )
      {
        bool generateMips = _Settings._GenerateMipMaps;
        if ( ImGui::Checkbox( "Generate mip maps", &generateMips ) )
          softwareRasterizer -> SetGenerateMipMaps(generateMips);
      }

      if ( ImGui::Checkbox( "Transparency", &_Settings._Transparency ) )
        _Renderer -> Notify(DirtyState::RenderSettings);
    }
    else if ( RendererType::OpenGLRasterizer == _RendererType )
    {
      DeferredRenderer * deferredRenderer = _Renderer -> AsDeferredRenderer();
      if ( deferredRenderer )
      {
        bool generateMips = _Settings._GenerateMipMaps;
        if ( ImGui::Checkbox( "Generate mip maps", &generateMips ) )
          deferredRenderer -> SetGenerateMipMaps(generateMips);

        int anisoLevel = _Settings._AnisotropicLevel;
        if ( ImGui::SliderInt( "Anisotropic level", &anisoLevel, 1, 16 ) )
          deferredRenderer -> SetAnisotropicLevel(anisoLevel);

        if ( ImGui::Checkbox( "Shadow mapping", &_Settings._ShadowMapping ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        int shadowMapResolution = _Settings._ShadowMapResolution;
        if ( ImGui::SliderInt( "Shadow map resolution", &shadowMapResolution, 256, 4096 ) )
        {
          shadowMapResolution = std::max(256, ( shadowMapResolution / 64 ) * 64);
          _Settings._ShadowMapResolution = shadowMapResolution;
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( ImGui::SliderInt( "Max shadow casting lights", &_Settings._MaxShadowCastingLights, 1, 8 ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Shadow bias", &_Settings._ShadowBias, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        bool autoShadowFar = _Settings._ShadowFar <= 0.f;
        if ( ImGui::Checkbox( "Auto shadow far plane", &autoShadowFar ) )
        {
          _Settings._ShadowFar = autoShadowFar ? 0.f : std::max(deferredRenderer -> GetEffectiveShadowFar(), 1.f);
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( autoShadowFar )
        {
          ImGui::Text( "Shadow far plane %.2f", deferredRenderer -> GetEffectiveShadowFar() );
        }
        else if ( ImGui::SliderFloat( "Shadow far plane", &_Settings._ShadowFar, 1.f, 500.f ) )
        {
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( ImGui::Checkbox( "SSAO", &_Settings._SSAO ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::Checkbox( "SSAO blur", &_Settings._SSAOBlur ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSAO radius", &_Settings._SSAORadius, 0.05f, 5.f, "%.3f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSAO bias", &_Settings._SSAOBias, 0.0001f, 0.1f, "%.4f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSAO intensity", &_Settings._SSAOIntensity, 0.f, 3.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        int ssaoKernelSize = _Settings._SSAOKernelSize;
        if ( ImGui::SliderInt( "SSAO kernel size", &ssaoKernelSize, 4, 32 ) )
        {
          _Settings._SSAOKernelSize = std::max(4, std::min(32, ssaoKernelSize));
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( ImGui::Checkbox( "SSR", &_Settings._SSR ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSR intensity", &_Settings._SSRIntensity, 0.f, 2.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSR max roughness", &_Settings._SSRMaxRoughness, 0.05f, 1.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        int ssrMaxSteps = _Settings._SSRMaxSteps;
        if ( ImGui::SliderInt( "SSR max steps", &ssrMaxSteps, 4, 128 ) )
        {
          _Settings._SSRMaxSteps = std::max(4, std::min(128, ssrMaxSteps));
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( ImGui::SliderFloat( "SSR pixel stride", &_Settings._SSRPixelStride, 0.25f, 4.f, "%.2f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSR start bias", &_Settings._SSRStartBias, 0.25f, 8.f, "%.2f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSR max distance", &_Settings._SSRMaxDistance, 1.f, 100.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSR thickness", &_Settings._SSRThickness, 0.01f, 2.f, "%.3f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "SSR edge fade", &_Settings._SSRFade, 0.01f, 0.5f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::Checkbox( "Specular IBL", &_Settings._SpecularIBL ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "IBL intensity", &_Settings._SpecularIBLIntensity, 0.f, 3.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "IBL max roughness", &_Settings._SpecularIBLMaxRoughness, 0.05f, 1.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::Checkbox( "PBR direct lighting", &_Settings._PBRDirectLighting ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Direct light intensity", &_Settings._DirectLightIntensity, 0.f, 8.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::Checkbox( "Transparency", &_Settings._Transparency ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::Checkbox( "Refraction", &_Settings._Refraction ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        int refractionMaxSteps = _Settings._RefractionMaxSteps;
        if ( ImGui::SliderInt( "Refraction max steps", &refractionMaxSteps, 4, 128 ) )
        {
          _Settings._RefractionMaxSteps = std::max(4, std::min(128, refractionMaxSteps));
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( ImGui::SliderFloat( "Refraction pixel stride", &_Settings._RefractionPixelStride, 0.25f, 4.f, "%.2f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Refraction start bias", &_Settings._RefractionStartBias, 0.25f, 8.f, "%.2f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Refraction max distance", &_Settings._RefractionMaxDistance, 1.f, 100.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Refraction thickness", &_Settings._RefractionThickness, 0.01f, 2.f, "%.3f", ImGuiSliderFlags_Logarithmic ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Refraction edge fade", &_Settings._RefractionEdgeFade, 0.01f, 0.5f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

      }
    }

    if ( ImGui::Checkbox( "FXAA", &_Settings._FXAA ) )
    {}

    if ( ImGui::Checkbox( "Tone mapping", &_Settings._ToneMapping ) )
      _Renderer -> Notify(DirtyState::RenderSettings);

    if ( _Settings._ToneMapping )
    {
      if ( ImGui::SliderFloat( "Gamma", &_Settings._Gamma, .5f, 3.f ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::SliderFloat( "Exposure", &_Settings._Exposure, .1f, 5.f ) )
        _Renderer -> Notify(DirtyState::RenderSettings);
    }

    if ( RendererType::PathTracer == _RendererType )
    {
      static const char * PATH_TRACE_DEBUG_MODES[] = { "Off", "Tiles", "Albedo", "Metalness", "Roughness", "Normals", "UV", "BLAS"};
      if ( ImGui::Combo( "Debug view", &g_DebugMode, PATH_TRACE_DEBUG_MODES, 8 ) )
        _Renderer -> Notify(DirtyState::RenderSettings);
    }
    else if ( ( RendererType::SoftwareRasterizer == _RendererType ) || ( RendererType::OpenGLRasterizer == _RendererType ) )
    {
      g_DebugMode = 0;

      static const char * COLORorDEPTHorNORMALS[] = { "Color", "Depth", "Normals" };
      static const char * COLORorDEPTHorNORMALSorSHADOWSorSSAO[] = { "Color", "Depth", "Normals", "Shadows", "SSAO", "Specular IBL", "Material Params", "SSR", "Direct diffuse", "Direct specular" };

      static int bufferChoice = 0;
      int maxBufferChoice = ( RendererType::OpenGLRasterizer == _RendererType ) ? ( 10 ) : ( 3 );
      if ( ( RendererType::SoftwareRasterizer == _RendererType ) && ( bufferChoice > 2 ) )
        bufferChoice = 0;

      if ( ImGui::Combo("Buffer", &bufferChoice, ( RendererType::OpenGLRasterizer == _RendererType ) ? ( COLORorDEPTHorNORMALSorSHADOWSorSSAO ) : ( COLORorDEPTHorNORMALS ), maxBufferChoice ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      static int showWires = 0;
      if ( ImGui::Combo("Show wires", &showWires, YESorNO, 2) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( RendererType::SoftwareRasterizer == _RendererType )
      {
        if ( 0 == bufferChoice )
          g_DebugMode |= (int)RasterDebugModes::ColorBuffer;
        else if ( 1 == bufferChoice )
          g_DebugMode |= (int)RasterDebugModes::DepthBuffer;
        else if ( 2 == bufferChoice )
          g_DebugMode |= (int)RasterDebugModes::Normals;

        if ( showWires )
          g_DebugMode |= (int)RasterDebugModes::Wires;
      }
      else if ( RendererType::OpenGLRasterizer == _RendererType )
      {
        _Settings._ShowShadowMap = ( 3 == bufferChoice );

        if ( 0 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::ColorBuffer;
        else if ( 1 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::DepthBuffer;
        else if ( 2 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::Normals;
        else if ( 3 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::Shadows;
        else if ( 4 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::SSAO;
        else if ( 5 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::SpecularIBL;
        else if ( 6 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::MaterialParams;
        else if ( 7 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::SSR;
        else if ( 8 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::DirectDiffuse;
        else if ( 9 == bufferChoice )
          g_DebugMode |= (int)DeferredDebugModes::DirectSpecular;

        if ( showWires )
          g_DebugMode |= (int)DeferredDebugModes::Wires;
      }
    }

    _Renderer -> SetDebugMode(g_DebugMode);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawCameraUI
// ----------------------------------------------------------------------------
int Test5::DrawCameraUI()
{
  if ( ImGui::CollapsingHeader("Camera") )
  {
    ImGui::Text("Position : %f, %f, %f", _Scene -> GetCamera().GetPos().x, _Scene -> GetCamera().GetPos().y, _Scene -> GetCamera().GetPos().z);
    ImGui::Text("Pivot    : %f, %f, %f", _Scene -> GetCamera().GetPivot().x, _Scene -> GetCamera().GetPivot().y, _Scene -> GetCamera().GetPivot().z);
    ImGui::Text("Radius   : %f", _Scene -> GetCamera().GetRadius());

    float fov = _Scene -> GetCamera().GetFOVInDegrees();
    if ( ImGui::SliderFloat( "FOV", &fov, 5.f, 150.f ) )
    {
      _Scene -> GetCamera().SetFOVInDegrees(fov);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ((RendererType::SoftwareRasterizer == _RendererType) || ( RendererType::OpenGLRasterizer == _RendererType ) )
    {
      float zNear = 0.f, zFar = 0.f;
      _Scene -> GetCamera().GetZNearFar(zNear, zFar);
      float zVal = zNear;
      if ( ImGui::SliderFloat("zNear", &zVal, 0.01f, std::min(10.f, zFar)) )
      {
        zNear = zVal;
        _Scene -> GetCamera().SetZNearFar(zNear, zFar);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }

      zVal = zFar;
      if ( ImGui::SliderFloat("zFar", &zVal, zNear + 0.01f, 10000.f) )
      {
        zFar = zVal;
        _Scene -> GetCamera().SetZNearFar(zNear, zFar);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }

    if ( RendererType::PathTracer == _RendererType )
    {
      float focalDist = _Scene -> GetCamera().GetFocalDist();
      //float fStop = ( _Scene -> GetCamera().GetAperture() > 0.f ) ? ( focalDist / _Scene -> GetCamera().GetAperture() ) : ( 1.4f );
      if ( ImGui::SliderFloat( "Focal distance", &focalDist, 0.1f, 10.f ) )
      {
        _Scene -> GetCamera().SetFocalDist(focalDist);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }

      static float FStopValue[] = { 0.f, 1.f, 1.4f, 2.f, 2.8f, 4.f, 5.6f, 8.f, 11.f, 16.f, 22.f, 32.f, 45.f, 64.f };
      static const char * FStopModes[] = { "INFINITE", "1.0", "1.4", "2.0", "2.8", "4.0", "5.6", "8.0", "11.0", "16.0", "22.0", "32.0", "45.0", "64.0" };
      if ( ImGui::Combo( "FStop", &g_FStopMode, FStopModes, 14 ) )
      {
        float aperture = ( g_FStopMode > 0 ) ? ( focalDist / FStopValue[g_FStopMode] ) : ( 0.f );
        _Scene -> GetCamera().SetAperture(aperture);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }

    if ( ImGui::Button( "Reset##Camera" ) )
    {
      _Scene -> SetCamera(_DefaultCam);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawMeshInstanceUI
// ----------------------------------------------------------------------------
int Test5::DrawMeshInstanceUI()
{
  if ( !_Scene )
    return 0;

  std::vector<MeshInstance> & meshInstances = _Scene -> GetMeshInstances();
  if ( _SelectedMeshInstanceID >= static_cast<int>(meshInstances.size()) )
    _SelectedMeshInstanceID = -1;
  if ( _BoidsBinding.ContainsInstanceID(_SelectedMeshInstanceID) )
    _SelectedMeshInstanceID = -1;

  if ( ImGui::CollapsingHeader("Mesh Instances") )
  {
    ImGui::Checkbox("Enable gizmo", &_MeshGizmoEnabled);
    ImGui::Checkbox("Show bounding box", &_ShowSelectedMeshBBox);

    const char * operations[] = { "Translate", "Rotate" };
    ImGui::Combo("Operation", &_MeshGizmoOperation, operations, 2);

    const char * modes[] = { "Local", "World" };
    ImGui::Combo("Mode", &_MeshGizmoMode, modes, 2);

    ImGui::Checkbox("Snap", &_MeshGizmoSnap);
    if ( _MeshGizmoSnap )
    {
      ImGui::InputFloat("Translate snap", &_MeshGizmoTranslateSnap, 0.05f, 1.f, "%.3f");
      ImGui::InputFloat("Rotate snap", &_MeshGizmoRotateSnap, 1.f, 15.f, "%.1f");
    }

    if ( ImGui::BeginListBox("##MeshInstances") )
    {
      for ( int i = 0; i < static_cast<int>(meshInstances.size()); ++i )
      {
        if ( _BoidsBinding.ContainsInstanceID(i) )
          continue;

        const MeshInstance & inst = meshInstances[i];
        std::string instanceName = std::string("Instance #") + std::to_string(i);
        if ( !inst._Filename.empty() )
          instanceName += std::string(" : ") + inst._Filename;

        bool isSelected = ( _SelectedMeshInstanceID == i );
        if ( ImGui::Selectable(instanceName.c_str(), isSelected) )
        {
          _SelectedMeshInstanceID = i;
          _SelectedLightID = -1;
        }
      }
      ImGui::EndListBox();
    }

    if ( _BoidsBinding.Attached() )
      ImGui::TextDisabled("%d boid instances hidden.", _BoidsSettings._Count);

    if ( _SelectedMeshInstanceID >= 0 )
    {
      MeshInstance & inst = meshInstances[_SelectedMeshInstanceID];

      ImGui::Text("Mesh ID     : %d", inst._MeshID);
      ImGui::Text("Material ID : %d", inst._MaterialID);
      if ( ImGui::Checkbox("Visible", &inst._Visible) )
        NotifyMeshInstanceEdited();

      float translation[3] = { 0.f, 0.f, 0.f };
      float rotation[3]    = { 0.f, 0.f, 0.f };
      float scale[3]       = { 1.f, 1.f, 1.f };
      ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(inst._Transform), translation, rotation, scale);

      bool transformChanged = false;
      transformChanged |= ImGui::InputFloat3("Translation", translation);
      transformChanged |= ImGui::InputFloat3("Rotation", rotation);

      if ( transformChanged )
      {
        ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, glm::value_ptr(inst._Transform));
        NotifyMeshInstanceEdited();
      }

      if ( ImGui::Button("Reset transform") )
      {
        inst._Transform = Mat4x4(1.f);
        NotifyMeshInstanceEdited();
      }
    }

    ImGui::Text("Ctrl + Left Click selects the nearest mesh instance.");
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawBoidsUI
// ----------------------------------------------------------------------------
int Test5::DrawBoidsUI()
{
  if ( !_Scene )
    return 0;

  if ( ImGui::CollapsingHeader("Boids") )
  {
    bool enabled = _BoidsEnabled;
    if ( ImGui::Checkbox("Enable boids", &enabled) )
    {
      _BoidsEnabled = enabled;
      _SelectedMeshInstanceID = -1;

      if ( _BoidsEnabled )
      {
        if ( 0 != InitializeBoidsForScene(true) )
        {
          std::cout << "ERROR: Failed to initialize boids." << std::endl;
          _BoidsEnabled = false;
        }
        else
          _ReloadRenderer = true;
      }
      else
      {
        _BoidsBinding.Detach(*_Scene);
        NotifyBoidsInstancesEdited();
      }
    }

    if ( !_BoidsEnabled )
      return 0;

    ImGui::Checkbox("Pause", &_BoidsSettings._Paused);

    float boidColor[3] = { _BoidsSettings._Color.r, _BoidsSettings._Color.g, _BoidsSettings._Color.b };
    if ( ImGui::ColorEdit3("Color", boidColor) )
    {
      _BoidsSettings._Color = Vec3(boidColor[0], boidColor[1], boidColor[2]);
      if ( 0 == _BoidsBinding.SyncMaterial(*_Scene, _BoidsSettings) )
        _Renderer -> Notify(DirtyState::SceneMaterials);
    }

    if ( ImGui::Button("Reset##Boids") )
    {
      _BoidsSimulation.Reset(_BoidsSettings);
      _BoidsBinding.Attach(*_Scene, _BoidsSettings);
      _BoidsBinding.SyncTransforms(*_Scene, _BoidsSimulation, _BoidsSettings);
      NotifyBoidsInstancesEdited();
    }
    ImGui::SameLine();
    if ( ImGui::Button("Fit bounds to scene") )
    {
      ComputeBoidsBoundsFromScene();
      _BoidsSimulation.Reset(_BoidsSettings);
      _BoidsBinding.Attach(*_Scene, _BoidsSettings);
      _BoidsBinding.SyncTransforms(*_Scene, _BoidsSimulation, _BoidsSettings);
      NotifyBoidsInstancesEdited();
    }

    int count = _BoidsSettings._Count;
    if ( ImGui::SliderInt("Count", &count, 1, 512) )
    {
      _BoidsSettings._Count = count;
      _BoidsSimulation.Resize(_BoidsSettings);
      _BoidsBinding.Attach(*_Scene, _BoidsSettings);
      _BoidsBinding.SyncTransforms(*_Scene, _BoidsSimulation, _BoidsSettings);
      NotifyBoidsInstancesEdited();
    }

    int seed = static_cast<int>(_BoidsSettings._Seed);
    if ( ImGui::InputInt("Seed", &seed) )
      _BoidsSettings._Seed = static_cast<unsigned int>(std::max(seed, 1));

    bool syncTransforms = false;
    syncTransforms |= ImGui::SliderFloat("Scale", &_BoidsSettings._Scale, 0.02f, 0.5f);

    if ( ImGui::SliderFloat("Min speed", &_BoidsSettings._MinSpeed, 0.05f, 5.f) )
      _BoidsSettings._MinSpeed = std::min(_BoidsSettings._MinSpeed, _BoidsSettings._MaxSpeed);
    if ( ImGui::SliderFloat("Max speed", &_BoidsSettings._MaxSpeed, 0.05f, 8.f) )
      _BoidsSettings._MaxSpeed = std::max(_BoidsSettings._MaxSpeed, _BoidsSettings._MinSpeed);
    ImGui::SliderFloat("Max force", &_BoidsSettings._MaxForce, 0.05f, 12.f);

    if ( ImGui::SliderFloat("Neighbor radius", &_BoidsSettings._NeighborRadius, 0.1f, 5.f) )
      _BoidsSettings._NeighborRadius = std::max(_BoidsSettings._NeighborRadius, _BoidsSettings._SeparationRadius);
    if ( ImGui::SliderFloat("Separation radius", &_BoidsSettings._SeparationRadius, 0.05f, 2.f) )
      _BoidsSettings._SeparationRadius = std::min(_BoidsSettings._SeparationRadius, _BoidsSettings._NeighborRadius);

    ImGui::SliderFloat("Separation", &_BoidsSettings._SeparationWeight, 0.f, 5.f);
    ImGui::SliderFloat("Alignment", &_BoidsSettings._AlignmentWeight, 0.f, 5.f);
    ImGui::SliderFloat("Cohesion", &_BoidsSettings._CohesionWeight, 0.f, 5.f);
    ImGui::SliderFloat("Bounds", &_BoidsSettings._BoundsWeight, 0.f, 5.f);

    ImGui::SliderFloat("Bounds radius", &_BoidsSettings._BoundsRadius, 0.5f, 25.f);
    ImGui::SliderFloat("Bounds height", &_BoidsSettings._BoundsHeight, 0.5f, 25.f);

    if ( syncTransforms )
    {
      _BoidsBinding.SyncTransforms(*_Scene, _BoidsSimulation, _BoidsSettings);
      NotifyBoidsInstancesEdited();
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawBackgroundUI
// ----------------------------------------------------------------------------
int Test5::DrawBackgroundUI()
{
  if ( ImGui::CollapsingHeader("Background") )
  {
    if ( ImGui::Checkbox( "Show background", &_Settings._EnableBackGround ) )
      _Renderer -> Notify(DirtyState::RenderSettings);

    float rgb[3] = { _Settings._BackgroundColor.r, _Settings._BackgroundColor.g, _Settings._BackgroundColor.b };
    if ( ImGui::ColorEdit3("Background", rgb) )
    {
      _Settings._BackgroundColor = { rgb[0], rgb[1], rgb[2] };
      _Renderer -> Notify(DirtyState::RenderSettings);
    }

    if ( ImGui::Checkbox("Environment mapping", &_Settings._EnableSkybox) )
    {
      if ( ( _CurBackgroundId < 0 ) && _BackgroundNames.size() )
      {
        _CurBackgroundId = 0;
        _ReloadBackground = true;
      }
      _Renderer -> Notify(DirtyState::RenderSettings);
    }

    if ( _Settings._EnableSkybox )
    {
      if ( ImGui::SliderFloat("Env map rotation", &_Settings._SkyBoxRotation, 0.f, 360.f) )
        _Renderer -> Notify(DirtyState::RenderSettings);
    }

    if ( _Settings._EnableSkybox && _BackgroundNames.size() && ( _CurBackgroundId >= 0 ) )
    {
      int selectedBgdId = _CurBackgroundId;
      if ( ImGui::Combo( "Background selection", &selectedBgdId, _BackgroundNames.data(), static_cast<int>(_BackgroundNames.size()) ) )
      {
        if ( selectedBgdId != _CurBackgroundId )
        {
          _CurBackgroundId = selectedBgdId;
          _ReloadBackground = true;
        }
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

      if ( _Scene -> GetEnvMap().IsInitialized() && _Scene -> GetEnvMap().GetHandle() )
      {
        ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_Scene -> GetEnvMap().GetHandle());
        ImGui::Image(texture, ImVec2(128, 128));
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawMaterialsUI
// ----------------------------------------------------------------------------
int Test5::DrawMaterialsUI()
{
  if (ImGui::CollapsingHeader("Materials"))
  {
    std::vector<Material>& Materials = _Scene -> GetMaterials();
    std::vector<Texture*>& Textures = _Scene -> GetTextures();

    auto uploadMaterialPreviewTexture = []( Texture * iTexture, GLTexture & ioTEX )
    {
      if ( !iTexture )
        return;

      GLTextureDesc desc;
      desc._Target = ioTEX._Target;
      desc._Slot   = ioTEX._Slot;
      desc._Width  = iTexture -> GetWidth();
      desc._Height = iTexture -> GetHeight();

      if ( iTexture -> GetUCData() )
      {
        desc._InternalFormat = GL_RGBA8;
        desc._DataFormat     = GL_RGBA;
        desc._DataType       = GL_UNSIGNED_BYTE;
        desc._Data           = iTexture -> GetUCData();
      }
      else if ( iTexture -> GetFData() )
      {
        desc._InternalFormat = GL_RGBA32F;
        desc._DataFormat     = GL_RGBA;
        desc._DataType       = GL_FLOAT;
        desc._Data           = iTexture -> GetFData();
      }
      else
        return;

      GLUtil::CreateTexture(desc, ioTEX);
    };

    static int selectedMaterial = -1;
    if (selectedMaterial >= _MaterialNames.size())
      selectedMaterial = -1;

    bool newMaterial = false;
    if (ImGui::BeginListBox("##MaterialNames"))
    {
      for (int i = 0; i < _MaterialNames.size(); i++)
      {
        bool is_selected = (selectedMaterial == i);
        if (ImGui::Selectable(_MaterialNames[i].c_str(), is_selected))
        {
          selectedMaterial = i;
          newMaterial = true;
        }
      }
      ImGui::EndListBox();
    }

    if (selectedMaterial >= 0)
    {
      Material& curMat = Materials[selectedMaterial];

      float rgb[3] = { curMat._Albedo.r, curMat._Albedo.g, curMat._Albedo.b };
      if (ImGui::ColorEdit3("Albedo", rgb))
      {
        curMat._Albedo.r = rgb[0];
        curMat._Albedo.g = rgb[1];
        curMat._Albedo.b = rgb[2];
        _Renderer -> Notify(DirtyState::SceneMaterials);
      }

      rgb[0] = curMat._Emission.r, rgb[1] = curMat._Emission.g, rgb[2] = curMat._Emission.b;
      if (ImGui::ColorEdit3("Emission", rgb))
      {
        curMat._Emission.r = rgb[0];
        curMat._Emission.g = rgb[1];
        curMat._Emission.b = rgb[2];
        _Renderer -> Notify(DirtyState::SceneMaterials);
      }

      if (ImGui::SliderFloat("Metallic", &curMat._Metallic, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Roughness", &curMat._Roughness, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Reflectance", &curMat._Reflectance, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Subsurface", &curMat._Subsurface, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Sheen Tint", &curMat._SheenTint, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Anisotropic", &curMat._Anisotropic, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Specular Trans", &curMat._SpecTrans, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Specular Tint", &curMat._SpecTint, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Clearcoat", &curMat._Clearcoat, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("Clearcoat Gloss", &curMat._ClearcoatGloss, 0.f, 1.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      if (ImGui::SliderFloat("IOR", &curMat._IOR, 1.f, 3.f))
        _Renderer -> Notify(DirtyState::SceneMaterials);

      static const char* ALPHA_MODES[] = { "Opaque", "Blend", "Mask" };
      int alphaMode = (int)curMat._AlphaMode;
      if (ImGui::Combo("Alpha mode", &alphaMode, ALPHA_MODES, 3))
      {
        curMat._AlphaMode = (float)alphaMode;
        _Renderer -> Notify(DirtyState::SceneMaterials);
      }

      if (curMat._AlphaMode != 0.f)
      {
        if (ImGui::SliderFloat("Opacity", &curMat._Opacity, 0.f, 1.f))
          _Renderer -> Notify(DirtyState::SceneMaterials);
      }

      if (AlphaMode::Mask == (AlphaMode)curMat._AlphaMode)
      {
        if (ImGui::SliderFloat("Alpha cutoff", &curMat._AlphaCutoff, 0.f, 1.f))
          _Renderer -> Notify(DirtyState::SceneMaterials);
      }

      if (curMat._BaseColorTexId >= 0)
      {
        Texture* basecolorTexture = Textures[static_cast<int>(curMat._BaseColorTexId)];
        if (basecolorTexture)
        {
          if (newMaterial)
          {
            GLUtil::DeleteTEX(_AlbedoTEX);
            uploadMaterialPreviewTexture(basecolorTexture, _AlbedoTEX);
          }

          if (_AlbedoTEX._Handle)
          {
            ImGui::Text("Base color :");
            ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_AlbedoTEX._Handle);
            ImGui::Image(texture, ImVec2(256, 256));
          }
        }
      }

      if (curMat._MetallicRoughnessTexID >= 0)
      {
        Texture* metallicRoughnessTexture = Textures[static_cast<int>(curMat._MetallicRoughnessTexID)];
        if (metallicRoughnessTexture)
        {
          if (newMaterial)
          {
            GLUtil::DeleteTEX(_MetalRoughTEX);
            uploadMaterialPreviewTexture(metallicRoughnessTexture, _MetalRoughTEX);
          }

          if (_MetalRoughTEX._Handle)
          {
            ImGui::Text("Metallic Roughness :");
            ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_MetalRoughTEX._Handle);
            ImGui::Image(texture, ImVec2(256, 256));
          }
        }
      }

      if (curMat._NormalMapTexID >= 0)
      {
        Texture* normalMapTexture = Textures[static_cast<int>(curMat._NormalMapTexID)];
        if (normalMapTexture)
        {
          if (newMaterial)
          {
            GLUtil::DeleteTEX(_NormalMapTEX);
            uploadMaterialPreviewTexture(normalMapTexture, _NormalMapTEX);
          }

          if (_NormalMapTEX._Handle)
          {
            ImGui::Text("Normal map :");
            ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_NormalMapTEX._Handle);
            ImGui::Image(texture, ImVec2(256, 256));
          }
        }
      }

      if (curMat._EmissionMapTexID >= 0)
      {
        Texture* emissionMapTexture = Textures[static_cast<int>(curMat._EmissionMapTexID)];
        if (emissionMapTexture)
        {
          if (newMaterial)
          {
            GLUtil::DeleteTEX(_EmissionMapTEX);
            uploadMaterialPreviewTexture(emissionMapTexture, _EmissionMapTEX);
          }

          if (_EmissionMapTEX._Handle)
          {
            ImGui::Text("Emission map :");
            ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_EmissionMapTEX._Handle);
            ImGui::Image(texture, ImVec2(256, 256));
          }
        }
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawLightsUI
// ----------------------------------------------------------------------------
int Test5::DrawLightsUI()
{
  if ( !_Scene )
    return 0;

  if ( _SelectedLightID >= _Scene -> GetNbLights() )
    _SelectedLightID = -1;

  if ( ImGui::CollapsingHeader("Lights") )
  {
    if ( ImGui::Checkbox("Show lights", &_Settings._ShowLights) )
      NotifyLightEdited();

    if ( RendererType::OpenGLRasterizer == _RendererType )
    {
      if ( ImGui::Checkbox("Uniform ambient light", &_Settings._EnableUniformLight) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( _Settings._EnableUniformLight )
      {
        float ambientColor[3] = { _Settings._UniformLightCol.r, _Settings._UniformLightCol.g, _Settings._UniformLightCol.b };
        if ( ImGui::ColorEdit3("Ambient light color", ambientColor) )
        {
          _Settings._UniformLightCol = Vec3( ambientColor[0], ambientColor[1], ambientColor[2] );
          _Renderer -> Notify(DirtyState::RenderSettings);
        }
      }
    }

    ImGui::Checkbox("Enable light gizmo", &_LightGizmoEnabled);
    ImGui::Checkbox("Light gizmo snap", &_LightGizmoSnap);
    if ( _LightGizmoSnap )
      ImGui::InputFloat("Light translate snap", &_LightGizmoTranslateSnap, 0.05f, 1.f, "%.3f");

    if ( ImGui::BeginListBox("##Lights") )
    {
      for (int i = 0; i < _Scene -> GetNbLights(); i++)
      {
        std::string lightName("Light#");
        lightName += std::to_string(i);

        bool is_selected = ( _SelectedLightID == i );
        if (ImGui::Selectable(lightName.c_str(), is_selected))
        {
          _SelectedLightID = i;
          _SelectedMeshInstanceID = -1;
        }
      }
      ImGui::EndListBox();
    }

    if (ImGui::Button("Add light"))
    {
      Light newLight;
      _Scene -> AddLight(newLight);
      _SelectedLightID = _Scene -> GetNbLights() - 1;
      _SelectedMeshInstanceID = -1;
      NotifyLightEdited();
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove light"))
    {
      if ( _SelectedLightID >= 0 )
      {
        _Scene -> RemoveLight( _SelectedLightID );
        _SelectedLightID = -1;
        NotifyLightEdited();
      }
    }

    if ( _SelectedLightID >= 0 )
    {
      Light * curLight = _Scene -> GetLight(_SelectedLightID);
      if ( curLight )
      {
        const char * LightTypes[3] = { "Quad", "Sphere", "Distant" };

        int lightType = (int)curLight -> _Type;
        if ( ImGui::Combo("Type", &lightType, LightTypes, 3) )
        {
          if ( lightType != (int)curLight -> _Type )
          {
            curLight -> _Type = (float)lightType;
            NotifyLightEdited();
          }
        }

        if ( LightType::DistantLight == (LightType) lightType )
          ImGui::TextDisabled("Distant lights use Position as a direction; no translation gizmo is shown.");

        float pos[3] = { curLight -> _Pos.x, curLight -> _Pos.y, curLight -> _Pos.z };
        if ( ImGui::InputFloat3("Position", pos) )
        {
          curLight -> _Pos.x = pos[0];
          curLight -> _Pos.y = pos[1];
          curLight -> _Pos.z = pos[2];
          NotifyLightEdited();
        }

        if ( ImGui::SliderFloat( "Intensity", &curLight -> _Intensity, 0.001f, 100.f ) )
          NotifyLightEdited();

        if ( ImGui::Checkbox( "Cast shadow", &curLight -> _CastShadow ) )
          NotifyLightEdited();

        float rgb[3] = { curLight -> _Emission.r, curLight -> _Emission.g, curLight -> _Emission.b };
        if ( ImGui::ColorEdit3("Emission", rgb) )
        {
          curLight -> _Emission = Vec3( rgb[0], rgb[1], rgb[2] );
          NotifyLightEdited();
        }

        if ( LightType::SphereLight == (LightType) lightType )
        {
          if ( ImGui::SliderFloat("Light radius", &curLight -> _Radius, 0.001f, 1.f) )
          {
            curLight -> _Area = 4.0f * static_cast<float>(M_PI) * curLight -> _Radius * curLight -> _Radius;
            NotifyLightEdited();
          }
        }
        else if ( LightType::RectLight == (LightType) lightType )
        {
          float dirU[3] = { curLight -> _DirU.x, curLight -> _DirU.y, curLight -> _DirU.z };
          if ( ImGui::InputFloat3("DirU", dirU) )
          {
            curLight -> _DirU.x = dirU[0];
            curLight -> _DirU.y = dirU[1];
            curLight -> _DirU.z = dirU[2];
            curLight -> _Area = glm::length(glm::cross(curLight -> _DirU, curLight -> _DirV));
            NotifyLightEdited();
          }

          float dirV[3] = { curLight -> _DirV.x, curLight -> _DirV.y, curLight -> _DirV.z };
          if ( ImGui::InputFloat3("DirV", dirV) )
          {
            curLight -> _DirV.x = dirV[0];
            curLight -> _DirV.y = dirV[1];
            curLight -> _DirV.z = dirV[2];
            curLight -> _Area = glm::length(glm::cross(curLight -> _DirU, curLight -> _DirV));
            NotifyLightEdited();
          }
        }

        if ( LightType::DistantLight != (LightType) lightType )
        {
          if ( ImGui::SliderFloat("Shadow radius", &curLight -> _ShadowRadius, 0.f, 500.f) )
            NotifyLightEdited();
        }
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawSelectedMeshInstanceBBox
// ----------------------------------------------------------------------------
int Test5::DrawSelectedMeshInstanceBBox()
{
  if ( !_ShowSelectedMeshBBox || !_Scene )
    return 0;

  std::vector<MeshInstance> & meshInstances = _Scene -> GetMeshInstances();
  std::vector<Mesh*>        & meshes        = _Scene -> GetMeshes();

  if ( ( _SelectedMeshInstanceID < 0 ) || ( _SelectedMeshInstanceID >= static_cast<int>(meshInstances.size()) ) )
    return 0;

  const MeshInstance & inst = meshInstances[_SelectedMeshInstanceID];
  if ( !inst._Visible )
    return 0;

  if ( ( inst._MeshID < 0 ) || ( inst._MeshID >= static_cast<int>(meshes.size()) ) )
    return 0;

  Mesh * mesh = meshes[inst._MeshID];
  if ( !mesh )
    return 0;

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);

  Camera & cam = _Scene -> GetCamera();
  cam.ComputeLookAtMatrix(view);

  const float width  = static_cast<float>(std::max(1, _Settings._WindowResolution.x));
  const float height = static_cast<float>(std::max(1, _Settings._WindowResolution.y));
  cam.ComputePerspectiveProjMatrix(width / height, proj);

  Vec3 localCorners[8];
  mesh -> GetBoundingBox().Corners(localCorners);

  ImVec2 screenCorners[8];
  bool visibleCorners[8] = { false, false, false, false, false, false, false, false };
  ImGuiIO & io = ImGui::GetIO();

  for ( int i = 0; i < 8; ++i )
  {
    Vec4 worldCorner = inst._Transform * Vec4(localCorners[i], 1.f);
    Vec4 clipCorner = proj * view * worldCorner;
    if ( clipCorner.w <= 0.00001f )
      continue;

    Vec3 ndc = Vec3(clipCorner) / clipCorner.w;
    screenCorners[i].x = ( ndc.x * 0.5f + 0.5f ) * io.DisplaySize.x;
    screenCorners[i].y = ( 0.5f - ndc.y * 0.5f ) * io.DisplaySize.y;
    visibleCorners[i] = true;
  }

  static const int Edges[12][2] =
  {
    { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
    { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
  };

  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  const ImU32 boxColor = IM_COL32(255, 184, 48, 235);
  const float lineWidth = 2.f;
  for ( int i = 0; i < 12; ++i )
  {
    const int c0 = Edges[i][0];
    const int c1 = Edges[i][1];
    if ( visibleCorners[c0] && visibleCorners[c1] )
      drawList -> AddLine(screenCorners[c0], screenCorners[c1], boxColor, lineWidth);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawMeshInstanceGizmo
// ----------------------------------------------------------------------------
int Test5::DrawMeshInstanceGizmo()
{
  if ( !_MeshGizmoEnabled || !_Scene || !_Renderer )
    return 0;

  std::vector<MeshInstance> & meshInstances = _Scene -> GetMeshInstances();
  if ( ( _SelectedMeshInstanceID < 0 ) || ( _SelectedMeshInstanceID >= static_cast<int>(meshInstances.size()) ) )
    return 0;

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);

  Camera & cam = _Scene -> GetCamera();
  cam.ComputeLookAtMatrix(view);

  const float width  = static_cast<float>(std::max(1, _Settings._WindowResolution.x));
  const float height = static_cast<float>(std::max(1, _Settings._WindowResolution.y));
  cam.ComputePerspectiveProjMatrix(width / height, proj);

  ImGuiIO & io = ImGui::GetIO();
  ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
  ImGuizmo::SetRect(0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y);
  ImGuizmo::SetOrthographic(false);

  ImGuizmo::OPERATION operation = ( 0 == _MeshGizmoOperation ) ? ImGuizmo::TRANSLATE : ImGuizmo::ROTATE;
  ImGuizmo::MODE mode = ( 0 == _MeshGizmoMode ) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

  float snap[3] = { 0.f, 0.f, 0.f };
  const float * snapPtr = nullptr;
  if ( _MeshGizmoSnap )
  {
    const float snapValue = ( ImGuizmo::TRANSLATE == operation ) ? _MeshGizmoTranslateSnap : _MeshGizmoRotateSnap;
    snap[0] = snap[1] = snap[2] = snapValue;
    snapPtr = snap;
  }

  MeshInstance & inst = meshInstances[_SelectedMeshInstanceID];
  if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), operation, mode, glm::value_ptr(inst._Transform), nullptr, snapPtr) )
    NotifyMeshInstanceEdited();

  return 0;
}

// ----------------------------------------------------------------------------
// DrawLightGizmo
// ----------------------------------------------------------------------------
int Test5::DrawLightGizmo()
{
  if ( !_LightGizmoEnabled || !_Scene || !_Renderer )
    return 0;

  if ( ( _SelectedLightID < 0 ) || ( _SelectedLightID >= _Scene -> GetNbLights() ) )
    return 0;

  Light * curLight = _Scene -> GetLight(_SelectedLightID);
  if ( !curLight )
    return 0;

  LightType lightType = (LightType)curLight -> _Type;
  if ( LightType::DistantLight == lightType )
    return 0;

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);

  Camera & cam = _Scene -> GetCamera();
  cam.ComputeLookAtMatrix(view);

  const float width  = static_cast<float>(std::max(1, _Settings._WindowResolution.x));
  const float height = static_cast<float>(std::max(1, _Settings._WindowResolution.y));
  cam.ComputePerspectiveProjMatrix(width / height, proj);

  ImGuiIO & io = ImGui::GetIO();
  ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
  ImGuizmo::SetRect(0.f, 0.f, io.DisplaySize.x, io.DisplaySize.y);
  ImGuizmo::SetOrthographic(false);

  float snap[3] = { _LightGizmoTranslateSnap, _LightGizmoTranslateSnap, _LightGizmoTranslateSnap };
  const float * snapPtr = _LightGizmoSnap ? snap : nullptr;

  Mat4x4 lightTransform(1.f);
  lightTransform[3] = Vec4(curLight -> _Pos, 1.f);

  if ( ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, glm::value_ptr(lightTransform), nullptr, snapPtr) )
  {
    curLight -> _Pos.x = lightTransform[3].x;
    curLight -> _Pos.y = lightTransform[3].y;
    curLight -> _Pos.z = lightTransform[3].z;
    NotifyLightEdited();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// BuildPickingRay
// ----------------------------------------------------------------------------
bool Test5::BuildPickingRay( double iMouseX, double iMouseY, Vec3 & oRayOrigin, Vec3 & oRayDir ) const
{
  if ( !_MainWindow || !_Scene )
    return false;

  int windowWidth = 0;
  int windowHeight = 0;
  glfwGetWindowSize(_MainWindow.get(), &windowWidth, &windowHeight);
  if ( !windowWidth || !windowHeight || !_Settings._WindowResolution.x || !_Settings._WindowResolution.y )
    return false;

  const float mouseX = static_cast<float>(iMouseX) * static_cast<float>(_Settings._WindowResolution.x) / static_cast<float>(windowWidth);
  const float mouseY = static_cast<float>(iMouseY) * static_cast<float>(_Settings._WindowResolution.y) / static_cast<float>(windowHeight);

  const float ndcX = 2.f * mouseX / static_cast<float>(_Settings._WindowResolution.x) - 1.f;
  const float ndcY = 1.f - 2.f * mouseY / static_cast<float>(_Settings._WindowResolution.y);

  Mat4x4 view(1.f);
  Mat4x4 proj(1.f);
  Camera & cam = const_cast<Scene*>(_Scene.get()) -> GetCamera();
  cam.ComputeLookAtMatrix(view);
  cam.ComputePerspectiveProjMatrix(static_cast<float>(_Settings._WindowResolution.x) / static_cast<float>(_Settings._WindowResolution.y), proj);

  Mat4x4 invViewProj = glm::inverse(proj * view);
  Vec4 nearPoint = invViewProj * Vec4(ndcX, ndcY, -1.f, 1.f);
  Vec4 farPoint  = invViewProj * Vec4(ndcX, ndcY,  1.f, 1.f);
  if ( nearPoint.w != 0.f )
    nearPoint /= nearPoint.w;
  if ( farPoint.w != 0.f )
    farPoint /= farPoint.w;

  oRayOrigin = cam.GetPos();
  oRayDir = glm::normalize(Vec3(farPoint) - oRayOrigin);

  return glm::length(oRayDir) > 0.f;
}

// ----------------------------------------------------------------------------
// PickMeshInstance
// ----------------------------------------------------------------------------
bool Test5::PickMeshInstance( double iMouseX, double iMouseY, int & oMeshInstanceID ) const
{
  oMeshInstanceID = -1;
  if ( !_Scene )
    return false;

  Vec3 rayOrigin(0.f);
  Vec3 rayDir(0.f);
  if ( !BuildPickingRay(iMouseX, iMouseY, rayOrigin, rayDir) )
    return false;

  const std::vector<MeshInstance> & meshInstances = _Scene -> GetMeshInstances();
  const std::vector<Mesh*> & meshes = _Scene -> GetMeshes();

  float nearestDist = MAX_FLOAT;
  for ( int instID = 0; instID < static_cast<int>(meshInstances.size()); ++instID )
  {
    if ( _BoidsBinding.ContainsInstanceID(instID) )
      continue;

    const MeshInstance & inst = meshInstances[instID];
    if ( !inst._Visible )
      continue;

    if ( ( inst._MeshID < 0 ) || ( inst._MeshID >= static_cast<int>(meshes.size()) ) )
      continue;

    Mesh * mesh = meshes[inst._MeshID];
    if ( !mesh )
      continue;

    Mat4x4 invTransform = glm::inverse(inst._Transform);
    Vec4 localOrigin4 = invTransform * Vec4(rayOrigin, 1.f);
    Vec4 localDir4 = invTransform * Vec4(rayDir, 0.f);
    Vec3 localOrigin = Vec3(localOrigin4);
    Vec3 localDir = glm::normalize(Vec3(localDir4));

    float boxHitT = 0.f;
    if ( !MathUtil::IntersectRayAABB(localOrigin, localDir, mesh -> GetBoundingBox(), boxHitT) )
      continue;

    const std::vector<Vec3> & vertices = mesh -> GetVertices();
    const std::vector<Vec3i> & indices = mesh -> GetIndices();
    for ( int i = 0; i + 2 < static_cast<int>(indices.size()); i += 3 )
    {
      const Vec3i & i0 = indices[i + 0];
      const Vec3i & i1 = indices[i + 1];
      const Vec3i & i2 = indices[i + 2];

      float triHitT = 0.f;
      if ( MathUtil::IntersectRayTriangle(localOrigin, localDir, vertices[i0.x], vertices[i1.x], vertices[i2.x], triHitT) )
      {
        Vec3 localHit = localOrigin + localDir * triHitT;
        Vec3 worldHit = MathUtil::TransformPoint(localHit, inst._Transform);
        float worldDist = glm::length(worldHit - rayOrigin);
        if ( worldDist < nearestDist )
        {
          nearestDist = worldDist;
          oMeshInstanceID = instID;
        }
      }
    }
  }

  return ( oMeshInstanceID >= 0 );
}

// ----------------------------------------------------------------------------
// ProcessInput
// ----------------------------------------------------------------------------
int Test5::ProcessInput()
{
  // MOUSE INPUT
  static bool toggleZoom = false;
  double curMouseX = 0., curMouseY = 0.;
  glfwGetCursorPos(_MainWindow.get(), &curMouseX, &curMouseY);
  if ( !ImGui::GetIO().WantCaptureMouse )
  {
    const float MouseSensitivity[6] = { 1.f, 0.5f, 0.01f, 0.01f, .5f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ScrollInOut, ZoomInOut

    float deltaX = 0., deltaY = 0.;
    double mouseX = 0.f, mouseY = 0.f;

    // LEFT CLICK
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY)
      && !ImGuizmo::IsOver()
      && !ImGuizmo::IsUsing())
    {
      if ( !_MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2, mouseX, mouseY)
        && !_MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_3, mouseX, mouseY)
        && !_KeyInput.IsKeyDown(GLFW_KEY_LEFT_CONTROL)
        && !_KeyInput.IsKeyDown(GLFW_KEY_RIGHT_CONTROL) )
      {
        _SelectedMeshInstanceID = -1;
        _SelectedLightID = -1;
      }
    }
    // RIGHT CLICK
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2, mouseX, mouseY) )
    {
      _Scene -> GetCamera().SetCameraMode(CameraMode::FreeLook);

      deltaX = static_cast<float>(curMouseX - mouseX);
      deltaY = static_cast<float>(curMouseY - mouseY);
      _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);

      _Renderer -> Notify(DirtyState::SceneCamera);
    }
    // MIDDLE CLICK
    else if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_3, mouseX, mouseY) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY) ) // Left Pressed
      {
        deltaX = static_cast<float>(curMouseX - mouseX);
        deltaY = static_cast<float>(curMouseY - mouseY);
        _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);
      }
      else if ( _MouseInput.IsButtonReleased(GLFW_MOUSE_BUTTON_1, mouseX, mouseY) || toggleZoom ) // Left released
      {
        toggleZoom = true;
        deltaY = static_cast<float>(curMouseY - mouseY);
        float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[5] * deltaY;
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      else
      {
        deltaX = static_cast<float>(curMouseX - mouseX);
        deltaY = static_cast<float>(curMouseY - mouseY);
        _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, MouseSensitivity[2] * deltaY);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _MouseInput.IsScrolled(mouseX, mouseY) )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[4] * static_cast<float>(mouseY);
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }
  }

  if ( _MouseInput.IsButtonReleased(GLFW_MOUSE_BUTTON_2) )
    _Scene -> GetCamera().SetCameraMode(CameraMode::Orbit);
  if ( _MouseInput.IsButtonReleased(GLFW_MOUSE_BUTTON_3) )
    toggleZoom = false;

  // KEYBOARD INPUT
  {
    const float Velocity[2] = { 10.f, 100.f }; // Movements, Rotation

    if ( _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) )
      return 1; // Exit

    if ( _KeyInput.IsKeyDown(GLFW_KEY_W) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Walk(static_cast<float>(_DeltaTime * Velocity[0]));
      else
      {
        float newRadius = static_cast<float>(_Scene -> GetCamera().GetRadius() - _DeltaTime);
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_S) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Walk(static_cast<float>(-_DeltaTime * Velocity[0]));
      else
      {
        float newRadius = static_cast<float>(_Scene -> GetCamera().GetRadius() + _DeltaTime);
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_A) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Strafe(static_cast<float>(_DeltaTime * Velocity[0]), 0.f);
      else
        _Scene -> GetCamera().OffsetOrientations(static_cast<float>(_DeltaTime * Velocity[1]), 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_D) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Strafe(static_cast<float>(-_DeltaTime * Velocity[0]), 0.f);
      else
        _Scene -> GetCamera().OffsetOrientations(static_cast<float>(-_DeltaTime * Velocity[1]), 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_LEFT_CONTROL)
      || _KeyInput.IsKeyDown(GLFW_KEY_RIGHT_CONTROL) )
    {
      double mouseX = 0.f, mouseY = 0.f;
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY)
        && !ImGui::GetIO().WantCaptureMouse
        && !ImGuizmo::IsOver()
        && !ImGuizmo::IsUsing() )
      {
        int pickedInstance = -1;
        if ( PickMeshInstance(mouseX, mouseY, pickedInstance) )
        {
          _SelectedMeshInstanceID = pickedInstance;
          _SelectedLightID = -1;
        }
      }
    }
  }

  _KeyInput.ClearLastEvents();
  _MouseInput.ClearLastEvents(curMouseX, curMouseY);

  glfwPollEvents();

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test5::InitializeScene()
{
  Scene * newScene = new Scene;
  RenderSettings newSettings = _Settings;
  const std::string scenePath = ( _CurSceneId >= 0 ) && ( _CurSceneId < static_cast<int>(_SceneFiles.size()) ) ? _SceneFiles[_CurSceneId] : "";
  if ( !newScene || scenePath.empty() || !Loader::LoadScene(scenePath, *newScene, newSettings) )
  {
    std::cout << "Failed to load scene : " << scenePath << std::endl;
    delete newScene;
    return 1;
  }
  _Scene.reset(newScene);
  _Settings = newSettings;
  _SelectedMeshInstanceID = -1;
  _SelectedLightID = -1;

  // Scene should contain at least one light
  //Light * firstLight = _Scene -> GetLight(0);
  //if ( !firstLight )
  //{
  //  Light newLight;
  //  _Scene -> AddLight(newLight);
  //  firstLight = _Scene -> GetLight(0);
  //}

  if ( _Scene -> GetEnvMap().IsInitialized() )
  {
    const std::string & filename = Util::FileName(_Scene -> GetEnvMap().Filename());

    for ( int i = 0; i < _BackgroundNames.size(); ++i )
    {
      if ( 0 == strcmp(_BackgroundNames[i], filename.c_str()) )
      {
        _CurBackgroundId = i;
        break;
      }
    }
  }
  else
    _CurBackgroundId = -1;

  _MaterialNames.clear();
  const std::vector<Material> & Materials =  _Scene -> GetMaterials();
  for ( auto & mat : Materials )
    _MaterialNames.push_back(_Scene -> FindMaterialName(static_cast<int>(mat._ID)));
  GLUtil::DeleteTEX(_AlbedoTEX);
  GLUtil::DeleteTEX(_MetalRoughTEX);
  GLUtil::DeleteTEX(_NormalMapTEX);
  GLUtil::DeleteTEX(_EmissionMapTEX);

  _DefaultCam = _Scene -> GetCamera();

  _Settings._NbThreads = g_NbThreadsMax;

  _BoidsBinding.Reset();
  if ( _BoidsEnabled && ( 0 != InitializeBoidsForScene(true) ) )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeBoidsForScene
// ----------------------------------------------------------------------------
int Test5::InitializeBoidsForScene( bool iResetSimulation )
{
  if ( !_Scene )
    return 0;

  ComputeBoidsBoundsFromScene();

  if ( iResetSimulation )
    _BoidsSimulation.Reset(_BoidsSettings);
  else
    _BoidsSimulation.Resize(_BoidsSettings);

  if ( 0 != _BoidsBinding.Attach(*_Scene, _BoidsSettings) )
    return 1;

  if ( 0 != _BoidsBinding.SyncTransforms(*_Scene, _BoidsSimulation, _BoidsSettings) )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeRenderer
// ----------------------------------------------------------------------------
int Test5::InitializeRenderer()
{
  RendererBackend backend = RendererBackend::PathTracer;

  if ( RendererType::PathTracer == _RendererType )
    backend = RendererBackend::PathTracer;
  else if ( RendererType::SoftwareRasterizer == _RendererType )
    backend = RendererBackend::SoftwareRasterizer;
  else if ( RendererType::OpenGLRasterizer == _RendererType )
    backend = RendererBackend::DeferredRenderer;

  _Renderer = CreateRenderer(backend, *_Scene, _Settings);
  if ( !_Renderer )
  {
    std::cout << "Failed to initialize the renderer" << std::endl;
    return 1;
  }

  _Renderer -> Initialize();

  g_DebugMode = 0;
  _Renderer -> SetDebugMode(g_DebugMode);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateScene
// ----------------------------------------------------------------------------
int Test5::UpdateScene()
{
  if ( _ReloadScene )
  {
    _ReloadScene = false;

    if ( 0 != InitializeScene() )
    {
      std::cout << "ERROR: Scene initialization failed!" << std::endl;
      const std::string sceneName = ( _CurSceneId >= 0 ) && ( _CurSceneId < static_cast<int>(_SceneNames.size()) ) ? _SceneNames[_CurSceneId] : "selected scene";
      _SceneLoadError = "Unable to load scene: " + sceneName;
      return 0;
    }

    _ReloadRenderer = true;
  }

  if ( _ReloadRenderer )
  {
    _ReloadRenderer = false;

    // On macOS, framebuffer pixels can differ from the logical GLFW window size.
    // Keep the current framebuffer dimensions authoritative when switching
    // renderers so we do not feed backing-pixel sizes back into glfwSetWindowSize().
    SyncFramebufferResolution();

    // Initialize the renderer
    if ( 0 != InitializeRenderer() || !_Renderer )
    {
      std::cout << "ERROR: Renderer initialization failed!" << std::endl;
      return 1;
    }

    SyncFramebufferResolution();
  }

  if ( _ReloadBackground )
  {
    if ( _CurBackgroundId >= 0 )
      _Scene -> LoadEnvMap( _BackgroundFiles[_CurBackgroundId] );

    _Renderer -> Notify(DirtyState::SceneEnvMap);

    _ReloadBackground = false;
  }

  if ( 0 != UpdateBoids() )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateBoids
// ----------------------------------------------------------------------------
int Test5::UpdateBoids()
{
  if ( !_BoidsEnabled || !_Scene || !_BoidsBinding.Attached() || _BoidsSettings._Paused )
    return 0;

  if ( 0 != _BoidsSimulation.Update(static_cast<float>(_DeltaTime), _BoidsSettings) )
    return 1;

  if ( 0 != _BoidsBinding.SyncTransforms(*_Scene, _BoidsSimulation, _BoidsSettings) )
    return 1;

  NotifyBoidsInstancesEdited();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test5::UpdateCPUTime()
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
int Test5::Run()
{
  int ret = 0;

  do
  {
    if ( !_MainWindow )
    {
      ret = 1;
      break;
    }

    glfwSetWindowTitle( _MainWindow.get(), GetTestHeader() );
    glfwSetWindowUserPointer( _MainWindow.get(), this );

    glfwSetFramebufferSizeCallback( _MainWindow.get(), Test5::FramebufferSizeCallback );
    glfwSetMouseButtonCallback( _MainWindow.get(), Test5::MouseButtonCallback );
    glfwSetScrollCallback( _MainWindow.get(), Test5::MouseScrollCallback );
    glfwSetKeyCallback( _MainWindow.get(), Test5::KeyCallback );
    glfwSetDropCallback( _MainWindow.get(), Test5::DropCallback );

    glfwMakeContextCurrent( _MainWindow.get() );
    glfwSwapInterval( 0 ); // Disable vsync

    // Setup Dear ImGui context
    if ( 0 != InitializeUI() )
    {
      std::cout << "Failed to initialize ImGui!" << std::endl;
      ret = 1;
      break;
    }

    // Init openGL scene
    glewExperimental = GL_TRUE;
    if ( glewInit() != GLEW_OK )
    {
      std::cout << "Failed to initialize GLEW!" << std::endl;
      ret = 1;
      break;
    }

    // Initialize the scene
    if ( ( 0 != InitializeSceneFiles() ) || ( 0 != InitializeBackgroundFiles() ) || ( 0 != InitializeScene() ) )
    {
      std::cout << "ERROR: Scene initialization failed!" << std::endl;
      ret = 1;
      break;
    }

    // Initialize the renderer
    if ( 0 != InitializeRenderer() || !_Renderer )
    {
      std::cout << "ERROR: Renderer initialization failed!" << std::endl;
      ret = 1;
      break;
    }

    // Main loop
    glfwSetWindowSize( _MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y );
    SyncFramebufferResolution();
    glDisable( GL_DEPTH_TEST );

    while ( !glfwWindowShouldClose( _MainWindow.get() ) )
    {
      UpdateCPUTime();

      if ( 0 != UpdateScene() )
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

      glfwSwapBuffers( _MainWindow.get() );

      _NbRenderedFrames++;
    }

  } while ( 0 );

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwSetFramebufferSizeCallback( _MainWindow.get(), nullptr );
  glfwSetMouseButtonCallback( _MainWindow.get(), nullptr );
  glfwSetScrollCallback( _MainWindow.get(), nullptr );
  glfwSetKeyCallback( _MainWindow.get(), nullptr );
  glfwSetDropCallback( _MainWindow.get(), nullptr );

  return ret;
}

}
