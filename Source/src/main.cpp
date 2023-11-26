#include "Test1.h"
#include "Test2.h"
#include "Test3.h"
#include "Test4.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static int g_ScreenWidth  = 1920;
static int g_ScreenHeight = 1080;

// ----------------------------------------------------------------------------
// Global functions
// ----------------------------------------------------------------------------
static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

// ----------------------------------------------------------------------------
// TestSelectionPanel
// ----------------------------------------------------------------------------
int TestSelectionPanel( GLFWwindow * iMainWindow )
{
  if ( !iMainWindow )
    return 0;

  int selectedTest = 0;

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Load Fonts
  io.Fonts->AddFontDefault();

  // Setup Platform/Renderer backends
  const char* glsl_version = "#version 130";
  ImGui_ImplGlfw_InitForOpenGL(iMainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Our state
  const char * TestNames[] = { "Test 1 : Render to texture",
                               "Test 2 : Scene loader",
                               "Test 3 : Basic ray tracing",
                               "Test 4 : CPU Rasterizer" };
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  while (!glfwWindowShouldClose(iMainWindow) && !selectedTest)
  {
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::Begin("Test selection");

      int comboSelection = 0;
      if ( ImGui::Combo("Test selection", &comboSelection, TestNames, 4) )
      {
        if ( 0 == comboSelection )
          selectedTest = 1;
        else if ( 1 == comboSelection )
          selectedTest = 2;
        else if ( 2 == comboSelection )
          selectedTest = 3;
        else if ( 3 == comboSelection )
          selectedTest = 4;
      }

      ImGui::End();
    }

    // Rendering
    ImGui::Render();

    glfwGetFramebufferSize(iMainWindow, &g_ScreenWidth, &g_ScreenHeight);
    glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(iMainWindow);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return selectedTest;
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main(int, char**)
{
  int failure = 0;

  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if ( !glfwInit() )
  {
    std::cout << "Failed to initialize GLFW!" << std::endl;
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // Create window with graphics context
  GLFWwindow * mainWindow = glfwCreateWindow(g_ScreenWidth, g_ScreenHeight, "RTRT - Tests", NULL, NULL);
  if ( !mainWindow )
  {
    std::cout << "Failed to create a window!" << std::endl;
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(mainWindow);
  glfwSwapInterval(1); // Enable vsync

  while ( !glfwWindowShouldClose(mainWindow) )
  {
    int selectedTest = TestSelectionPanel(mainWindow);

    if ( 1 == selectedTest )
    {
      RTRT::Test1 test1(mainWindow, g_ScreenWidth, g_ScreenHeight);
      failure = test1.Run();
    }

    else if ( 2 == selectedTest )
    {
      RTRT::Test2 test2(mainWindow, g_ScreenWidth, g_ScreenHeight);
      failure = test2.Run();
    }

    else if ( 3 == selectedTest )
    {
      RTRT::Test3 test3(mainWindow, g_ScreenWidth, g_ScreenHeight);
      failure = test3.Run();
    }

    else if ( 4 == selectedTest )
    {
      RTRT::Test4 test4(mainWindow, g_ScreenWidth, g_ScreenHeight);
      failure = test4.Run();
    }

    else
    {
      break;
    }
  }

  glfwDestroyWindow(mainWindow);
  glfwTerminate();

  // Exit program
  return failure;
}
