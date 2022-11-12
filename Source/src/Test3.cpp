#include "Test3.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <deque>
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

static long  g_Frame         = 0;
static float g_FrameRate     = 60.f;
static float g_CurLoopTime   = 0.f;
static float g_TimeDelta     = 0.f;

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
}

Test3::Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  _ScreenWitdh  = iScreenWidth;
  _ScreenHeight = iScreenHeight;
}

Test3::~Test3()
{
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
    glfwTerminate();
    return 1;
  }

  glViewport(0, 0, _ScreenWitdh, _ScreenHeight);
  glDisable(GL_DEPTH_TEST);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  std::deque<float> lastDeltas;

  double oldCpuTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    g_Frame++;

    g_CurLoopTime = glfwGetTime();

    g_TimeDelta = g_CurLoopTime - oldCpuTime;
    oldCpuTime = g_CurLoopTime;

    lastDeltas.push_back(g_TimeDelta);
    while ( lastDeltas.size() > 60 )
      lastDeltas.pop_front();
    
    double totalDelta = 0.;
    for ( auto delta : lastDeltas )
      totalDelta += delta;
    double averageDelta = totalDelta / lastDeltas.size();

    if ( averageDelta > 0. )
      g_FrameRate = 1. / (float)averageDelta;

    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::Begin("Test 3");

      ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", averageDelta * 1000.f, g_FrameRate);

      ImGui::End();
    }

    // Rendering
    ImGui::Render();

    glfwGetFramebufferSize(_MainWindow, &_ScreenWitdh, &_ScreenHeight);
    glViewport(0, 0, _ScreenWitdh, _ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(_MainWindow);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return ret;
}

}
