#include "Test1.h"
#include "Test2.h"
#include "Test3.h"
#include "Test4.h"
#include "Test5.h"
#include "Test6.h"
#include "PathUtils.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>
#include <memory>
#include <algorithm>
#include <cctype>

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static int g_ScreenWidth  = 640;
static int g_ScreenHeight = 480;

// ----------------------------------------------------------------------------
// ParseTestArg
// ----------------------------------------------------------------------------
static int ParseTestArg( const std::string & iArg )
{
  if ( 1 == iArg.size() )
  {
    if ( ( iArg[0] >= '1' ) && ( iArg[0] <= '6' ) )
      return ( iArg[0] - '0' );
    return 0;
  }

  if ( 5 == iArg.size() )
  {
    std::string lowerArg( iArg );
    std::transform( lowerArg.begin(), lowerArg.end(), lowerArg.begin(), [](unsigned char c) { return (char)std::tolower(c); } );

    if ( lowerArg.rfind("test", 0) == 0 )
    {
      if ( ( lowerArg[4] >= '1' ) && ( lowerArg[4] <= '6' ) )
        return ( lowerArg[4] - '0' );
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// PrintUsage
// ----------------------------------------------------------------------------
static void PrintUsage( const char * iExeName )
{
  const char * exeName = ( iExeName && iExeName[0] ) ? iExeName : "RenderLab";

  std::cout << "Usage: " << exeName << " [Test1|Test2|Test3|Test4|Test5|Test6|1|2|3|4|5|6]" << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << exeName << std::endl;
  std::cout << "  " << exeName << " Test6" << std::endl;
  std::cout << "  " << exeName << " 6" << std::endl;
}

// ----------------------------------------------------------------------------
// RunSelectedTest
// ----------------------------------------------------------------------------
static int RunSelectedTest( const int iSelectedTest, std::shared_ptr<GLFWwindow> iMainWindow )
{
  int failure = 0;

  if ( 1 == iSelectedTest )
  {
    RTRT::Test1 test1(iMainWindow, g_ScreenWidth, g_ScreenHeight);
    failure = test1.Run();
  }
  else if ( 2 == iSelectedTest )
  {
    RTRT::Test2 test2(iMainWindow, g_ScreenWidth, g_ScreenHeight);
    failure = test2.Run();
  }
  else if ( 3 == iSelectedTest )
  {
    RTRT::Test3 test3(iMainWindow, g_ScreenWidth, g_ScreenHeight);
    failure = test3.Run();
  }
  else if ( 4 == iSelectedTest )
  {
    RTRT::Test4 test4(iMainWindow, g_ScreenWidth, g_ScreenHeight);
    failure = test4.Run();
  }
  else if ( 5 == iSelectedTest )
  {
    RTRT::Test5 test5(iMainWindow, g_ScreenWidth, g_ScreenHeight);
    failure = test5.Run();
  }
  else if ( 6 == iSelectedTest )
  {
    RTRT::Test6 test6(iMainWindow, g_ScreenWidth, g_ScreenHeight);
    failure = test6.Run();
  }

  return failure;
}

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
  const char* glsl_version = "#version 410";
  ImGui_ImplGlfw_InitForOpenGL(iMainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Our state
  const char * TestHeaders[] = { RTRT::Test1::GetTestHeader(),
                                 RTRT::Test2::GetTestHeader(),
                                 RTRT::Test3::GetTestHeader(),
                                 RTRT::Test4::GetTestHeader(),
                                 RTRT::Test5::GetTestHeader(),
                                 RTRT::Test6::GetTestHeader() };
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

      for ( int i = 0; i < 6; ++i )
      {
        std::string buttonName = std::string( "Test " ) + std::to_string( i + 1 );
        if ( ImGui::Button( buttonName.c_str() ) )
        {
          selectedTest = i + 1;
        }
        ImGui::SameLine();
        ImGui::Text(": %s", TestHeaders[i]);
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
int main(int iArgc, char** iArgv)
{
  int failure = 0;
  int cliSelectedTest = 0;

  RTRT::PathUtils::Initialize( ( iArgv ) ? ( iArgv[0] ) : nullptr );

  if ( iArgc > 2 )
  {
    PrintUsage( ( iArgv ) ? ( iArgv[0] ) : nullptr );
    return 1;
  }

  if ( 2 == iArgc )
  {
    const std::string arg = ( iArgv && iArgv[1] ) ? iArgv[1] : "";
    cliSelectedTest = ParseTestArg( arg );
    if ( !cliSelectedTest )
    {
      PrintUsage( ( iArgv ) ? ( iArgv[0] ) : nullptr );
      return 1;
    }
  }

  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if ( !glfwInit() )
  {
    std::cout << "Failed to initialize GLFW!" << std::endl;
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

  // Create window with graphics context
  {
    auto mainWindow = std::shared_ptr<GLFWwindow>(glfwCreateWindow(g_ScreenWidth, g_ScreenHeight, "RTRT - Tests", NULL, NULL), [](GLFWwindow*window){glfwDestroyWindow(window);});
    if ( !mainWindow )
    {
      std::cout << "Failed to create a window!" << std::endl;
      glfwTerminate();
      return 1;
    }
    glfwMakeContextCurrent(mainWindow.get());
    glfwSwapInterval(1); // Enable vsync

    while ( !glfwWindowShouldClose(mainWindow.get()) )
    {
      int selectedTest = cliSelectedTest;

      if ( !selectedTest )
        selectedTest = TestSelectionPanel(mainWindow.get());

      if ( selectedTest < 1 || selectedTest > 6 )
      {
        break;
      }

      failure = RunSelectedTest( selectedTest, mainWindow );

      if ( cliSelectedTest )
        break;
    }
  }
  glfwTerminate();

  // Exit program
  return failure;
}
