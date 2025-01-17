#include "Test5.h"

#include "Loader.h"
#include "PathTracer.h"
#include "Util.h"

#include <string>
#include <iostream>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace RTRT
{

const char * Test5::GetTestHeader() { return "Test 5 : Renderers"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

static int g_DebugMode = 0;

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
      static const char * Renderers[] = {"PathTracer", "SoftwareRasterizer (not available)"};
      //_RendererType
      int selectedRenderer = (int)_RendererType;
      if ( ImGui::Combo( "Renderer", &selectedRenderer, Renderers, 2 ) )
      {
        //_RendererType = (RendererType) selectedRenderer; // ToDo
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
      ImGui::Text("Window width %d: height : %d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
      ImGui::Text("Render width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);

      ImGui::Text("Render time %.3f ms/frame", _FrameTime * 1000.);

    }

    if (ImGui::CollapsingHeader("Settings"))
    {
      static bool vSync = true;
      if ( ImGui::Checkbox( "VSync", &vSync ) )
      {
        if ( vSync )
          glfwSwapInterval( 1 );
        else
          glfwSwapInterval( 0 );
      }

      int scale = _Settings._RenderScale;
      if ( ImGui::SliderInt("Render scale", &scale, 5, 200) )
      {
        _Settings._RenderScale = scale;
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

      if ( ImGui::SliderFloat( "Interactive res Ratio", &_Settings._LowResRatio, 0.05f, 1.f ) )
      {
        _Renderer -> Notify(DirtyState::RenderSettings);
      }

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

      if ( ImGui::Checkbox( "FXAA", &_Settings._FXAA ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( ImGui::Checkbox( "Tone mapping", &_Settings._ToneMapping ) )
        _Renderer -> Notify(DirtyState::RenderSettings);

      if ( _Settings._ToneMapping )
      {
        if ( ImGui::SliderFloat( "Gamma", &_Settings._Gamma, .5f, 3.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);

        if ( ImGui::SliderFloat( "Exposure", &_Settings._Exposure, .1f, 5.f ) )
          _Renderer -> Notify(DirtyState::RenderSettings);
      }

      static const char * PATH_TRACE_DEBUG_MODES[] = { "Off", "Albedo", "Metalness", "Roughness", "Normals", "UV", "Tiles"};
      if ( ImGui::Combo( "Debug view", &g_DebugMode, PATH_TRACE_DEBUG_MODES, 7 ) )
        _Renderer -> SetDebugMode(g_DebugMode);
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

        if ( _Scene -> GetEnvMap().IsInitialized() && _Scene -> GetEnvMap().GetTexID() )
        {
          ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_Scene -> GetEnvMap().GetTexID());
          ImGui::Image(texture, ImVec2(128, 128));
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
  // Keyboard input
  {
    const float velocity = 100.f;

    if ( _KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) )
      return 1; // Exit

    if ( _KeyInput.IsKeyDown(GLFW_KEY_W) )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() - _DeltaTime;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }
    if ( _KeyInput.IsKeyDown(GLFW_KEY_S) )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() + _DeltaTime;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }
    if ( _KeyInput.IsKeyDown(GLFW_KEY_A) )
    {
      _Scene -> GetCamera().OffsetOrientations(_DeltaTime * velocity, 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }
    if ( _KeyInput.IsKeyDown(GLFW_KEY_D) )
    {
      _Scene -> GetCamera().OffsetOrientations(-_DeltaTime * velocity, 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }
  }

  // Mouse input
  double curMouseX = 0., curMouseY = 0.;
  glfwGetCursorPos(_MainWindow.get(), &curMouseX, &curMouseY);
  {
    const float MouseSensitivity[6] = { 1.f, 0.5f, 0.01f, 0.01f, .5f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ScrollInOut, ZoomInOut

    static bool toggleZoom = false;
    double deltaX = 0., deltaY = 0.;
    double mouseX = 0.f, mouseY = 0.f;
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_3, mouseX, mouseY) ) // Middle click
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

    if ( _MouseInput.IsButtonReleased(GLFW_MOUSE_BUTTON_3, mouseX, mouseY) )
      toggleZoom = false;

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
  Light * firstLight = _Scene -> GetLight(0);
  if ( !firstLight )
  {
    Light newLight;
    _Scene -> AddLight(newLight);
    firstLight = _Scene -> GetLight(0);
  }

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

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeRenderer
// ----------------------------------------------------------------------------
int Test5::InitializeRenderer()
{
  Renderer * newRenderer = nullptr;
  
  if ( RendererType::PathTracerRenderer == _RendererType )
    newRenderer = new PathTracer(*_Scene, _Settings);

  if ( !newRenderer )
  {
    std::cout << "Failed to initialize the renderer" << std::endl;
    return 1;
  }
  _Renderer.reset(newRenderer);

  _Renderer -> Initialize();

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
    glfwSwapInterval( 1 ); // Enable vsync

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
