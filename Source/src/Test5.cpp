#include "Test5.h"

#include "Loader.h"
#include "PathTracer.h"
#include "SoftwareRasterizer.h"
#include "Util.h"

#include <string>
#include <iostream>
#include <thread>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace RTRT
{

const char * Test5::GetTestHeader() { return "Renderers"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

static int g_DebugMode    = 0;
static int g_FStopMode    = 0;
static unsigned int g_NbThreadsMax = std::thread::hardware_concurrency();

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void Test5::KeyCallback(GLFWwindow* window, const int key, const int scancode, const int action, const int mods)
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  if ( ( action == GLFW_PRESS ) ||  ( action == GLFW_RELEASE ) )
    this_ -> _KeyInput.AddEvent(key, action, mods);
}

// ----------------------------------------------------------------------------
// MouseButtonCallback
// ----------------------------------------------------------------------------
void Test5::MouseButtonCallback(GLFWwindow* window, const int button, const int action, const int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
    if ( !this_ )
      return;

    double mouseX = 0., mouseY = 0.;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if ( ( action == GLFW_PRESS ) ||  ( action == GLFW_RELEASE ) )
      this_ -> _MouseInput.AddButtonEvent(button, action, mouseX, mouseY);
  }
}

// ----------------------------------------------------------------------------
// MouseScrollCallback
// ----------------------------------------------------------------------------
void Test5::MouseScrollCallback(GLFWwindow * window, const double xoffset, const double yoffset)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
    if ( !this_ )
      return;

    this_ -> _MouseInput.AddScrollEvent(xoffset, yoffset);
  }
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void Test5::FramebufferSizeCallback(GLFWwindow* window, const int width, const int height)
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;

  this_ -> _Settings._WindowResolution.x = width;
  this_ -> _Settings._WindowResolution.y = height;
  this_ -> _Settings._RenderResolution.x = width;
  this_ -> _Settings._RenderResolution.y = height;

  glViewport(0, 0, this_ -> _Settings._WindowResolution.x, this_ -> _Settings._WindowResolution.y);

  if ( this_ -> _Renderer )
    this_ -> _Renderer -> Notify(DirtyState::RenderSettings);
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
// InitializeSceneFiles
// ----------------------------------------------------------------------------
int Test5::InitializeSceneFiles()
{
  std::vector<std::string> sceneNames;
  Util::RetrieveSceneFiles(g_AssetsDir, _SceneFiles, &sceneNames);

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
  Util::RetrieveBackgroundFiles(g_AssetsDir + "HDR\\", _BackgroundFiles, &backgroundNames);

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
  const char* glsl_version = "#version 130";
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

  {
    ImGui::Begin("Test 5 : Viewer");

    // FPS graph
    {
      static std::vector<float> s_FrameRateHistory;
      static int                s_LastFrameIndex = -1;
      static double             s_AccumTime = 0.f;
      static float              s_Max = 300.;

      if ( -1 == s_LastFrameIndex )
      {
        s_FrameRateHistory.assign( 120, 0.f );
        s_LastFrameIndex = 0;
        s_FrameRateHistory[s_LastFrameIndex] = (float)_FrameRate;
      }

      s_AccumTime += _DeltaTime;
      while ( s_AccumTime > ( 1.f / 60 ) )
      {
        s_AccumTime -= 0.1;
        s_Max = *std::max_element( s_FrameRateHistory.begin(), s_FrameRateHistory.end() );

        s_LastFrameIndex++;
        if ( s_LastFrameIndex >= 120 )
          s_LastFrameIndex = 0;
        s_FrameRateHistory[s_LastFrameIndex] = (float)_FrameRate;
      }

      int offset = ( s_LastFrameIndex >= 119 ) ? ( 0 ) : ( s_LastFrameIndex + 1 );

      char overlay[32];
      sprintf( overlay, "%.1f FPS", _FrameRate );
      ImGui::PlotLines( "Frame rate", &s_FrameRateHistory[0], s_FrameRateHistory.size(), offset, overlay, -0.1f, s_Max, ImVec2( 0, 80.0f ) );
    }

    // Renderer selection
    {
      static const char * Renderers[] = {"PathTracer", "SoftwareRasterizer"};
      //_RendererType
      int selectedRenderer = (int)_RendererType;
      if ( ImGui::Combo( "Renderer", &selectedRenderer, Renderers, 2 ) )
      {
        _RendererType = (RendererType) selectedRenderer;
        _ReloadRenderer = true;
      }
    }

    // Scene selection
    {
      int selectedSceneId = _CurSceneId;
      if ( ImGui::Combo("Scene selection", &selectedSceneId, &_SceneNames[0], _SceneNames.size()) )
      {
        if ( selectedSceneId != _CurSceneId )
        {
          _CurSceneId = selectedSceneId;
          _ReloadScene = true;
        }
      }
    }

    if ( ImGui::Button( "Capture image" ) )
    {
      _CaptureOutputPath = "./" + std::string( _SceneNames[_CurSceneId] ) + "_" + std::to_string( _NbRenderedFrames ) + "frames.png";
      _RenderToFile = true;
    }

    if (ImGui::CollapsingHeader("Rendering stats"))
    {
      ImGui::Text("Window width : %d height : %d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
      ImGui::Text("Render width : %d height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);

      ImGui::Text( "Render time           : %.3f ms/frame", _FrameTime * 1000. );

      if ( RendererType::PathTracer == _RendererType )
      {
        ImGui::Text( "Path trace time       : %.3f ms", _Renderer -> AsPathTracer() -> GetPathTraceTime() * 1000. );
        ImGui::Text( "Accumulate time       : %.3f ms", _Renderer -> AsPathTracer() -> GetAccumulateTime() * 1000. );
        ImGui::Text( "Denoise time          : %.3f ms", _Renderer -> AsPathTracer() -> GetDenoiseTime() * 1000. );
        ImGui::Text( "Render to screen time : %.3f ms", _Renderer -> AsPathTracer() -> GetRenderToScreenTime() * 1000. );

        ImGui::Text( "Frame number          : %d", _Renderer -> AsPathTracer() -> GetFrameNum() );
        ImGui::Text( "Nb complete frames    : %d", _Renderer -> AsPathTracer() -> GetNbCompleteFrames() );
      }

    }

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
        static const char * NEARESTorBILNEAR[] = { "Nearest", "Bilinear" };
        static const char * PHONGorFLAT[]      = { "Flat", "Phong" };

        int sampling = (int)_Settings._BilinearSampling;
        if ( ImGui::Combo("Texture sampling", &sampling, NEARESTorBILNEAR, 2) )
          _Renderer -> Notify(DirtyState::RenderSettings);
        _Settings._BilinearSampling = !!sampling;

        int shadingType = (int)_Settings._ShadingType;
        if ( ImGui::Combo("Shading", &shadingType, PHONGorFLAT, 2) )
          _Renderer -> Notify(DirtyState::RenderSettings);
        _Settings._ShadingType = (ShadingType)shadingType;

        int useWBuffer = !!_Settings._WBuffer;
        if ( ImGui::Combo("W-Buffer", &useWBuffer, YESorNO, 2) )
          _Renderer -> Notify(DirtyState::RenderSettings);
        _Settings._WBuffer = !!useWBuffer;
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
      else if ( RendererType::SoftwareRasterizer == _RendererType )
      {
        static const char * COLORorDEPTHorNORMALS[] = { "Color", "Depth", "Normals" };

        g_DebugMode = 0;
        static int bufferChoice = 0;
        if ( ImGui::Combo("Buffer", &bufferChoice, COLORorDEPTHorNORMALS, 3) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        static int showWires = 0;
        if ( ImGui::Combo("Show wires", &showWires, YESorNO, 2) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( 0 == bufferChoice )
          g_DebugMode += (int)RasterDebugModes::ColorBuffer;
        else if ( 1 == bufferChoice )
          g_DebugMode += (int)RasterDebugModes::DepthBuffer;
        else if ( 2 == bufferChoice )
          g_DebugMode += (int)RasterDebugModes::Normals;

        if ( showWires )
          g_DebugMode += (int)RasterDebugModes::Wires;
      }
      _Renderer -> SetDebugMode(g_DebugMode);
    }

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

      if ( RendererType::SoftwareRasterizer == _RendererType )
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
        float fStop = ( _Scene -> GetCamera().GetAperture() > 0.f ) ? ( focalDist / _Scene -> GetCamera().GetAperture() ) : ( 1.4f );
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

      if ( ImGui::Button( "Reset" ) )
      {
        _Scene -> SetCamera(_DefaultCam);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }

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
        if ( ImGui::Combo( "Background selection", &selectedBgdId, _BackgroundNames.data(), _BackgroundNames.size() ) )
        {
          if ( selectedBgdId != _CurBackgroundId )
          {
            _CurBackgroundId = selectedBgdId;
            _ReloadBackground = true;
          }
          _Renderer -> Notify(DirtyState::RenderSettings);
        }

        if ( _Scene -> GetEnvMap().IsInitialized() && _Scene -> GetEnvMap().GetGLTexID() )
        {
          ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_Scene -> GetEnvMap().GetGLTexID());
          ImGui::Image(texture, ImVec2(128, 128));
        }
      }
    }

   if ( ImGui::CollapsingHeader("Materials") )
    {
      std::vector<Material> & Materials =  _Scene -> GetMaterials();
      std::vector<Texture*> & Textures = _Scene -> GetTextures();

      static int selectedMaterial = -1;
      if ( selectedMaterial >= _MaterialNames.size() )
        selectedMaterial = -1;

      bool newMaterial = false;
      if ( ImGui::BeginListBox("##MaterialNames") )
      {
        for (int i = 0; i < _MaterialNames.size(); i++)
        {
          bool is_selected = ( selectedMaterial == i );
          if (ImGui::Selectable(_MaterialNames[i].c_str(), is_selected))
          {
            selectedMaterial = i;
            newMaterial = true;
          }
        }
        ImGui::EndListBox();
      }

      if ( selectedMaterial >= 0 )
      {
        Material & curMat = Materials[selectedMaterial];

        float rgb[3] = { curMat._Albedo.r, curMat._Albedo.g, curMat._Albedo.b };
        if ( ImGui::ColorEdit3("Albedo", rgb) )
        {
          curMat._Albedo.r = rgb[0];
          curMat._Albedo.g = rgb[1];
          curMat._Albedo.b = rgb[2];
          _Renderer -> Notify(DirtyState::SceneMaterials);
        }

        rgb[0] = curMat._Emission.r, rgb[1] = curMat._Emission.g, rgb[2] = curMat._Emission.b;
        if ( ImGui::ColorEdit3("Emission", rgb) )
        {
          curMat._Emission.r = rgb[0];
          curMat._Emission.g = rgb[1];
          curMat._Emission.b = rgb[2];
          _Renderer -> Notify(DirtyState::SceneMaterials);
        }

        if ( ImGui::SliderFloat("Metallic", &curMat._Metallic, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Roughness", &curMat._Roughness, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Reflectance", &curMat._Reflectance, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Subsurface", &curMat._Subsurface, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Sheen Tint", &curMat._SheenTint, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Anisotropic", &curMat._Anisotropic, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Specular Trans", &curMat._SpecTrans, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Specular Tint", &curMat._SpecTint, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Clearcoat", &curMat._Clearcoat, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("Clearcoat Gloss", &curMat._ClearcoatGloss, 0.f, 1.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        if ( ImGui::SliderFloat("IOR", &curMat._IOR, 1.f, 3.f) )
          _Renderer -> Notify(DirtyState::SceneMaterials);

        static const char * ALPHA_MODES[] = { "Opaque", "Blend", "Mask" };
        int alphaMode = (int)curMat._AlphaMode;
        if ( ImGui::Combo( "Alpha mode", &alphaMode, ALPHA_MODES, 3 ) )
        {
          curMat._AlphaMode = (float)alphaMode;
          _Renderer -> Notify( DirtyState::SceneMaterials );
        }

        if ( curMat._AlphaMode != 0.f )
        {
          if ( ImGui::SliderFloat("Opacity", &curMat._Opacity, 0.f, 1.f) )
            _Renderer -> Notify(DirtyState::SceneMaterials);
        }

        if ( AlphaMode::Mask == (AlphaMode)curMat._AlphaMode )
        {
          if ( ImGui::SliderFloat("Alpha cutoff", &curMat._AlphaCutoff, 0.f, 1.f) )
            _Renderer -> Notify(DirtyState::SceneMaterials);
        }

        if ( curMat._BaseColorTexId >= 0 )
        {
          Texture * basecolorTexture = Textures[curMat._BaseColorTexId];
          if ( basecolorTexture )
          {
            if ( newMaterial )
            {
              GLUtil::DeleteTEX(_AlbedoTEX);
              if ( basecolorTexture -> GetUCData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA8, basecolorTexture -> GetWidth(), basecolorTexture -> GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, basecolorTexture -> GetUCData(), _AlbedoTEX);
              else if ( basecolorTexture -> GetFData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA32F, basecolorTexture -> GetWidth(), basecolorTexture -> GetHeight(), GL_RGBA, GL_FLOAT, basecolorTexture -> GetFData(), _AlbedoTEX);
            }

            if ( _AlbedoTEX._ID )
            {
              ImGui::Text("Base color :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_AlbedoTEX._ID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }

        if ( curMat._MetallicRoughnessTexID >= 0 )
        {
          Texture * metallicRoughnessTexture = Textures[curMat._MetallicRoughnessTexID];
          if ( metallicRoughnessTexture )
          {
            if ( newMaterial )
            {
              GLUtil::DeleteTEX(_MetalRoughTEX);
              if ( metallicRoughnessTexture -> GetUCData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA8, metallicRoughnessTexture -> GetWidth(), metallicRoughnessTexture -> GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, metallicRoughnessTexture -> GetUCData(), _MetalRoughTEX);
              else if ( metallicRoughnessTexture -> GetFData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA32F, metallicRoughnessTexture -> GetWidth(), metallicRoughnessTexture -> GetHeight(), GL_RGBA, GL_FLOAT, metallicRoughnessTexture -> GetFData(), _MetalRoughTEX);
            }

            if ( _MetalRoughTEX._ID )
            {
              ImGui::Text("Metallic Roughness :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_MetalRoughTEX._ID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }

        if ( curMat._NormalMapTexID >= 0 )
        {
          Texture * normalMapTexture = Textures[curMat._NormalMapTexID];
          if ( normalMapTexture )
          {
            if ( newMaterial )
            {
              GLUtil::DeleteTEX(_NormalMapTEX);
              if ( normalMapTexture -> GetUCData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA8, normalMapTexture -> GetWidth(), normalMapTexture -> GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, normalMapTexture -> GetUCData(), _NormalMapTEX);
              else if ( normalMapTexture -> GetFData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA32F, normalMapTexture -> GetWidth(), normalMapTexture -> GetHeight(), GL_RGBA, GL_FLOAT, normalMapTexture -> GetFData(), _NormalMapTEX);
            }

            if ( _NormalMapTEX._ID )
            {
              ImGui::Text("Normal map :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_NormalMapTEX._ID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }

        if ( curMat._EmissionMapTexID >= 0 )
        {
          Texture * emissionMapTexture = Textures[curMat._EmissionMapTexID];
          if ( emissionMapTexture )
          {
            if ( newMaterial )
            {
              GLUtil::DeleteTEX(_EmissionMapTEX);
              if ( emissionMapTexture -> GetUCData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA8, emissionMapTexture -> GetWidth(), emissionMapTexture -> GetHeight(), GL_RGBA, GL_UNSIGNED_BYTE, emissionMapTexture -> GetUCData(), _EmissionMapTEX);
              else if ( emissionMapTexture -> GetFData() )
                GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA32F, emissionMapTexture -> GetWidth(), emissionMapTexture -> GetHeight(), GL_RGBA, GL_FLOAT, emissionMapTexture -> GetFData(), _EmissionMapTEX);
            }

            if ( _EmissionMapTEX._ID )
            {
              ImGui::Text("Emission map :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_EmissionMapTEX._ID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }
      }
    }

    if ( ImGui::CollapsingHeader("Lights") )
    {
      if ( ImGui::Checkbox("Show lights", &_Settings._ShowLights) )
        _Renderer -> Notify(DirtyState::SceneLights);

      static int selectedLight = -1;
      if ( ImGui::BeginListBox("##Lights") )
      {
        for (int i = 0; i < _Scene -> GetNbLights(); i++)
        {
          std::string lightName("Light#");
          lightName += std::to_string(i);

          bool is_selected = ( selectedLight == i );
          if (ImGui::Selectable(lightName.c_str(), is_selected))
          {
            selectedLight = i;
          }
        }
        ImGui::EndListBox();
      }

      if (ImGui::Button("Add light"))
      {
        Light newLight;
        _Scene -> AddLight(newLight);
        selectedLight = _Scene -> GetNbLights() - 1;
        _Renderer -> Notify(DirtyState::SceneLights);
      }
      ImGui::SameLine();
      if (ImGui::Button("Remove light"))
      {
        if ( selectedLight >= 0 )
        {
          _Scene -> RemoveLight( selectedLight );
          selectedLight = -1;
          _Renderer -> Notify(DirtyState::SceneLights);
        }
      }

      if ( selectedLight >= 0 )
      {
        Light * curLight = _Scene -> GetLight(selectedLight);
        if ( curLight )
        {
          const char * LightTypes[3] = { "Quad", "Sphere", "Distant" };

          int lightType = (int)curLight -> _Type;
          if ( ImGui::Combo("Type", &lightType, LightTypes, 3) )
          {
            if ( lightType != (int)curLight -> _Type )
            {
              curLight -> _Type = (float)lightType;
              _Renderer -> Notify(DirtyState::SceneLights);
            }
          }

          float pos[3] = { curLight -> _Pos.x, curLight -> _Pos.y, curLight -> _Pos.z };
          if ( ImGui::InputFloat3("Position", pos) )
          {
            curLight -> _Pos.x = pos[0];
            curLight -> _Pos.y = pos[1];
            curLight -> _Pos.z = pos[2];
            _Renderer -> Notify(DirtyState::SceneLights);
          }

          if ( ImGui::SliderFloat( "Intensity", &curLight -> _Intensity, 0.001f, 100.f ) )
            _Renderer -> Notify(DirtyState::SceneLights);

          float rgb[3] = { curLight -> _Emission.r, curLight -> _Emission.g, curLight -> _Emission.b };
          if ( ImGui::ColorEdit3("Emission", rgb) )
          {
            curLight -> _Emission = Vec3( rgb[0], rgb[1], rgb[2] );
            _Renderer -> Notify(DirtyState::SceneLights);
          }

          if ( LightType::SphereLight == (LightType) lightType )
          {
            if ( ImGui::SliderFloat("Light radius", &curLight -> _Radius, 0.001f, 1.f) )
            {
              curLight -> _Area = 4.0f * M_PI * curLight -> _Radius * curLight -> _Radius;
              _Renderer -> Notify(DirtyState::SceneLights);
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
              _Renderer -> Notify(DirtyState::SceneLights);
            }

            float dirV[3] = { curLight -> _DirV.x, curLight -> _DirV.y, curLight -> _DirV.z };
            if ( ImGui::InputFloat3("DirV", dirV) )
            {
              curLight -> _DirV.x = dirV[0];
              curLight -> _DirV.y = dirV[1];
              curLight -> _DirV.z = dirV[2];
              curLight -> _Area = glm::length(glm::cross(curLight -> _DirU, curLight -> _DirV));
              _Renderer -> Notify(DirtyState::SceneLights);
            }
          }
        }
      }
    }

    ImGui::End();
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
// ProcessInput
// ----------------------------------------------------------------------------
int Test5::ProcessInput()
{
  // MOUSE INPUT
  static bool toggleZoom = false;
  double curMouseX = 0., curMouseY = 0.;
  glfwGetCursorPos(_MainWindow.get(), &curMouseX, &curMouseY);
  {
    const float MouseSensitivity[6] = { 1.f, 0.5f, 0.01f, 0.01f, .5f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ScrollInOut, ZoomInOut

    double deltaX = 0., deltaY = 0.;
    double mouseX = 0.f, mouseY = 0.f;
     // RIGHT CLICK
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2, mouseX, mouseY) )
    {
      _Scene -> GetCamera().SetCameraMode(CameraMode::FreeLook);

      deltaX = curMouseX - mouseX;
      deltaY = curMouseY - mouseY;
      _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);

      _Renderer -> Notify(DirtyState::SceneCamera);
    }
    // MIDDLE CLICK
    else if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_3, mouseX, mouseY) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY) ) // Left Pressed
      {
        deltaX = curMouseX - mouseX;
        deltaY = curMouseY - mouseY;
        _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);
      }
      else if ( _MouseInput.IsButtonReleased(GLFW_MOUSE_BUTTON_1, mouseX, mouseY) || toggleZoom ) // Left released
      {
        toggleZoom = true;
        deltaY = curMouseY - mouseY;
        float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[5] * deltaY;
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      else
      {
        deltaX = curMouseX - mouseX;
        deltaY = curMouseY - mouseY;
        _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, MouseSensitivity[2] * deltaY);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _MouseInput.IsScrolled(mouseX, mouseY) )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[4] * mouseY;
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
        _Scene -> GetCamera().Walk(_DeltaTime * Velocity[0]);
      else
      {
        float newRadius = _Scene -> GetCamera().GetRadius() - _DeltaTime;
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_S) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Walk(-_DeltaTime * Velocity[0]);
      else
      {
        float newRadius = _Scene -> GetCamera().GetRadius() + _DeltaTime;
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_A) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Strafe(_DeltaTime * Velocity[0], 0.f);
      else
        _Scene -> GetCamera().OffsetOrientations(_DeltaTime * Velocity[1], 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }

    if ( _KeyInput.IsKeyDown(GLFW_KEY_D) )
    {
      if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_2) )
        _Scene -> GetCamera().Strafe(-_DeltaTime * Velocity[0], 0.f);
      else
        _Scene -> GetCamera().OffsetOrientations(-_DeltaTime * Velocity[1], 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
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
  if ( !newScene || ( _CurSceneId < 0 ) || !Loader::LoadScene(_SceneFiles[_CurSceneId], *newScene, _Settings) )
  {
    std::cout << "Failed to load scene : " << _SceneFiles[_CurSceneId] << std::endl;
    return 1;
  }
  _Scene.reset(newScene);

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
    _MaterialNames.push_back(_Scene -> FindMaterialName(mat._ID));
  GLUtil::DeleteTEX(_AlbedoTEX);
  GLUtil::DeleteTEX(_MetalRoughTEX);
  GLUtil::DeleteTEX(_NormalMapTEX);
  GLUtil::DeleteTEX(_EmissionMapTEX);

  _DefaultCam = _Scene -> GetCamera();

  _Settings._NbThreads = g_NbThreadsMax;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeRenderer
// ----------------------------------------------------------------------------
int Test5::InitializeRenderer()
{
  Renderer * newRenderer = nullptr;
  
  if ( RendererType::PathTracer == _RendererType )
    newRenderer = new PathTracer(*_Scene, _Settings);
  else if ( RendererType::SoftwareRasterizer == _RendererType )
    newRenderer = new SoftwareRasterizer(*_Scene, _Settings);

  if ( !newRenderer )
  {
    std::cout << "Failed to initialize the renderer" << std::endl;
    return 1;
  }
  _Renderer.reset(newRenderer);

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
      return 1;
    }

    _ReloadRenderer = true;
  }

  if ( _ReloadRenderer )
  {
    _ReloadRenderer = false;

    // Initialize the renderer
    if ( 0 != InitializeRenderer() || !_Renderer )
    {
      std::cout << "ERROR: Renderer initialization failed!" << std::endl;
      return 1;
    }

    glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  }

  if ( _ReloadBackground )
  {
    if ( _CurBackgroundId >= 0 )
      _Scene -> LoadEnvMap( _BackgroundFiles[_CurBackgroundId] );

    _Renderer -> Notify(DirtyState::SceneEnvMap);

    _ReloadBackground = false;
  }

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
    glViewport( 0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y );
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
  glfwSetKeyCallback( _MainWindow.get(), nullptr );

  return ret;
}

}
