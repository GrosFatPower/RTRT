#include "Test4.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Global functions
// ----------------------------------------------------------------------------
void Test4::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto * const this_ = static_cast<Test4*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  // ToDo
}

void Test4::MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test4*>(glfwGetWindowUserPointer(window));
    if ( !this_ )
      return;

    // ToDo
  }
}

void Test4::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  auto * const this_ = static_cast<Test4*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;

  this_ -> _Settings._WindowResolution.x = width;
  this_ -> _Settings._WindowResolution.y = height;
  this_ -> _Settings._RenderResolution.x = width  * ( this_ -> _Settings._RenderScale * 0.01f );
  this_ -> _Settings._RenderResolution.y = height * ( this_ -> _Settings._RenderScale * 0.01f );

  glViewport(0, 0, this_ -> _Settings._WindowResolution.x, this_ -> _Settings._WindowResolution.y);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Test4::Test4( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  _Settings._WindowResolution.x = iScreenWidth;
  _Settings._WindowResolution.y = iScreenHeight;
  _Settings._RenderResolution.x = iScreenWidth  * ( _Settings._RenderScale * 0.01f );
  _Settings._RenderResolution.y = iScreenHeight * ( _Settings._RenderScale * 0.01f );
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test4::~Test4()
{
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test4::UpdateCPUTime()
{
  double oldCpuTime = _CPULoopTime;
  _CPULoopTime = glfwGetTime();

  _TimeDelta = _CPULoopTime - oldCpuTime;
  oldCpuTime = _CPULoopTime;

  _LastDeltas.push_back(_TimeDelta);
  while ( _LastDeltas.size() > 30 )
    _LastDeltas.pop_front();
    
  double totalDelta = 0.;
  for ( auto delta : _LastDeltas )
    totalDelta += delta;
  _AverageDelta = totalDelta / _LastDeltas.size();

  if ( _AverageDelta > 0. )
    _FrameRate = 1. / (float)_AverageDelta;

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
int Test4::DrawUI()
{
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // Frame info
  {
    ImGui::Begin("Test 4");

    ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);

    ImGui::Text("Window width %d: height : %d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    ImGui::Text("Render width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);

    ImGui::End();
  }

  // Rendering
  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test4::Run()
{
  int ret = 0;

  if ( !_MainWindow )
    return 1;

  glfwSetWindowTitle(_MainWindow, "Test 4 : CPU Rasterizer");
  glfwSetWindowUserPointer(_MainWindow, this);

  glfwSetFramebufferSizeCallback(_MainWindow, Test4::FramebufferSizeCallback);
  glfwSetMouseButtonCallback(_MainWindow, Test4::MousebuttonCallback);
  glfwSetKeyCallback(_MainWindow, Test4::KeyCallback);

  glfwMakeContextCurrent(_MainWindow);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Load Fonts
  io.Fonts->AddFontDefault();

  // Setup Platform/Renderer backends
  const char* glsl_version = "#version 130";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Init openGL scene

  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    glfwTerminate();
    return 1;
  }

  glfwSetWindowSize(_MainWindow, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glDisable(GL_DEPTH_TEST);

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  _CPULoopTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    UpdateCPUTime();

    glfwPollEvents();

    glfwGetFramebufferSize(_MainWindow, &_Settings._WindowResolution.x, &_Settings._WindowResolution.y);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    DrawUI();

    glfwSwapBuffers(_MainWindow);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return ret;
}

}
