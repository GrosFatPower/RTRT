#include "Test5.h"

#include "Loader.h"
#include "PathTracer.h"

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
    ImGui::Begin("Test 5");

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
  glfwPollEvents();

  // Keyboard input
  {
    const float velocity = 100.f;

    if ( _KeyInput.IsKeyDown(GLFW_KEY_W) )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() - _TimeDelta;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }
    if ( _KeyInput.IsKeyDown(GLFW_KEY_S) )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() + _TimeDelta;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _Renderer -> Notify(DirtyState::SceneCamera);
      }
    }
    if ( _KeyInput.IsKeyDown(GLFW_KEY_A) )
    {
      _Scene -> GetCamera().OffsetOrientations(_TimeDelta * velocity, 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }
    if ( _KeyInput.IsKeyDown(GLFW_KEY_D) )
    {
      _Scene -> GetCamera().OffsetOrientations(-_TimeDelta * velocity, 0.f);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }
  }

  // Mouse input
  double curMouseX = 0., curMouseY = 0.;
  glfwGetCursorPos(_MainWindow.get(), &curMouseX, &curMouseY);
  {
    const float MouseSensitivity[5] = { 1.f, 0.5f, 0.01f, 0.01f, .5f }; // Yaw, Pitch, StafeRight, StrafeUp, ZoomInOut

    double deltaX = 0., deltaY = 0.;
    double mouseX = 0.f, mouseY = 0.f;
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_1, mouseX, mouseY) ) // Right click
    {
      deltaX = curMouseX - mouseX;
      deltaY = curMouseY - mouseY;
      _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);
      _Renderer -> Notify(DirtyState::SceneCamera);
    }
    if ( _MouseInput.IsButtonPressed(GLFW_MOUSE_BUTTON_3, mouseX, mouseY) ) // Middle click
    {
      deltaX = curMouseX - mouseX;
      deltaY = curMouseY - mouseY;
      _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, MouseSensitivity[2] * deltaY);
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

  _KeyInput.ClearLastEvents();
  _MouseInput.ClearLastEvents(curMouseX, curMouseY);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test5::InitializeScene()
{
  std::string sceneName = g_AssetsDir + "TexturedBox.scene";

  Scene * newScene = new Scene;
  if ( !newScene || !Loader::LoadScene(sceneName, *newScene, _Settings) )
  {
    std::cout << "Failed to load scene : " << sceneName << std::endl;
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

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test5::UpdateCPUTime()
{
  double oldCpuTime = _CPULoopTime;
  _CPULoopTime = glfwGetTime();

  _TimeDelta = _CPULoopTime - oldCpuTime;

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
    if ( 0 != InitializeScene() )
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

    while ( !glfwWindowShouldClose( _MainWindow.get() ) && !_KeyInput.IsKeyReleased(GLFW_KEY_ESCAPE) )
    {
      UpdateCPUTime();

      ProcessInput();

      _Renderer -> Update();

      _Renderer -> RenderToTexture();

      _Renderer -> RenderToScreen();

      _Renderer -> Done();

      DrawUI();

      glfwSwapBuffers( _MainWindow.get() );
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
