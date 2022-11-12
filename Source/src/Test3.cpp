#include "Test3.h"
#include "QuadMesh.h"
#include "ShaderProgram.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static double g_MouseX       = 0.;
static double g_MouseY       = 0.;
static bool   g_LeftClick    = false;
static bool   g_RightClick   = false;

static GLuint g_FrameBufferID   = 0;
static GLuint g_ScreenTextureID = 0;

static std::string g_AssetsDir = "..\\..\\Assets\\";
static int         g_CurSceneIndex = 0;
static int         g_LoadingState = 0;

// ----------------------------------------------------------------------------
// Global functions
// ----------------------------------------------------------------------------
static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (action == GLFW_PRESS)
    std::cout << "EVENT : KEY PRESSED" << std::endl;
}

static void MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    glfwGetCursorPos(window, &g_MouseX, &g_MouseY);

    if ( ( GLFW_MOUSE_BUTTON_1 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : LEFT CLICK (" << g_MouseX << "," << g_MouseY << ")" << std::endl;
      g_LeftClick = true;
    }
    else
      g_LeftClick = false;

    if ( ( GLFW_MOUSE_BUTTON_2 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : RIGHT CLICK (" << g_MouseX << "," << g_MouseY << ")" << std::endl;
      g_RightClick = true;
    }
    else
      g_RightClick = false;
  }
}

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;

  glViewport(0, 0, width, height);

  glBindTexture(GL_TEXTURE_2D, g_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
}

Test3::Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  _ScreenWidth  = iScreenWidth;
  _ScreenHeight = iScreenHeight;
}

Test3::~Test3()
{
  glDeleteFramebuffers(1, &g_FrameBufferID);
  glDeleteTextures(1, &g_ScreenTextureID);

  if (_Quad)
    delete _Quad;
  _Quad = nullptr;
  if (_RTTShader)
    delete _RTTShader;
  _RTTShader = nullptr;
  if (_RTSShader)
    delete _RTSShader;
  _RTSShader = nullptr;
}

int Test3::InitializeFrameBuffer()
{
  // Screen texture
  glGenTextures(1, &g_ScreenTextureID);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _ScreenWidth, _ScreenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // FrameBuffer
  glGenFramebuffers(1, &g_FrameBufferID);
  glBindFramebuffer(GL_FRAMEBUFFER, g_FrameBufferID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ScreenTextureID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  return 0;
}

int Test3::RecompileShaders()
{
  if (_RTTShader)
    delete _RTTShader;
  _RTTShader = nullptr;
  if (_RTSShader)
    delete _RTSShader;
  _RTSShader = nullptr;

  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Default.glsl");

  _RTTShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !_RTTShader )
    return 1;

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  _RTSShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !_RTSShader )
    return 1;

  return 0;
}

int Test3::UpdateUniforms()
{
  if ( _RTTShader )
  {
    _RTTShader -> Use();
    GLuint RTTProgramID = _RTTShader -> GetShaderProgramID();
    glUniform1f(glGetUniformLocation(RTTProgramID, "u_Time"), _CPULoopTime);
    _RTTShader -> StopUsing();
  }
  else
    return 1;

  if ( _RTSShader )
  {
    _RTSShader -> Use();
    GLuint RTSProgramID = _RTSShader -> GetShaderProgramID();
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_ScreenTexture"), 0);
    _RTSShader -> StopUsing();
  }
  else
    return 1;

  return 0;
}

int Test3::DrawUI()
{
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  {
    ImGui::Begin("Test 3");

    ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);

    ImGui::End();
  }

  // Rendering
  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  return 0;
}

int Test3::UpdateCPUTime()
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

int Test3::Run()
{
  int ret = 0;

  if ( !_MainWindow )
    return 1;

  glfwSetWindowTitle(_MainWindow, "Test 3 : Basic ray tracing");

  glfwSetFramebufferSizeCallback(_MainWindow, FramebufferSizeCallback);
  glfwSetMouseButtonCallback(_MainWindow, MousebuttonCallback);
  //glfwSetKeyCallback(_MainWindow, keyCallback);

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
    return 1;
  }

  // Shader compilation
  if ( ( 0 != RecompileShaders() ) || !_RTTShader || !_RTSShader )
  {
    std::cout << "Shader compilation failed!" << std::endl;
    return 1;
  }

  // Quad
  _Quad = new QuadMesh();

  // Frame buffer
  if ( 0 != InitializeFrameBuffer() )
  {
    std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
    return 1;
  }

  glViewport(0, 0, _ScreenWidth, _ScreenHeight);
  glDisable(GL_DEPTH_TEST);

  // Main loop
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  _CPULoopTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    _Frame++;

    UpdateCPUTime();

    glfwPollEvents();

    glfwGetFramebufferSize(_MainWindow, &_ScreenWidth, &_ScreenHeight);
    glViewport(0, 0, _ScreenWidth, _ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    UpdateUniforms();

    // Render to texture
    glBindFramebuffer(GL_FRAMEBUFFER, g_FrameBufferID);
    _Quad -> Render(*_RTTShader);

    // Render to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    _Quad -> Render(*_RTSShader);

    // UI
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
