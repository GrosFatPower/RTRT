#include "PathUtils.h"
#include "TetrisGameApp.h"
#include "TetrisUnitTests.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include <string>

// ----------------------------------------------------------------------------
// PrintUsage
// ----------------------------------------------------------------------------
static void PrintUsage( const char * iExeName )
{
  const char * exeName = ( iExeName && iExeName[0] ) ? iExeName : "TetrisGame";
  std::cout << "Usage: " << exeName << " [OPTIONS]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --unit-tests   Run headless Tetris Guideline unit tests" << std::endl;
  std::cout << "  --help, -h     Show this help message" << std::endl;
}

// ----------------------------------------------------------------------------
// glfw_error_callback
// ----------------------------------------------------------------------------
static void glfw_error_callback( int error, const char * description )
{
  std::cerr << "GLFW Error " << error << ": " << ( description ? description : "" ) << std::endl;
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main( int iArgc, char ** iArgv )
{
  RTRT::PathUtils::Initialize( ( iArgc > 0 ) ? iArgv[0] : nullptr );

  std::string screenshotPath;

  if ( iArgc >= 2 )
  {
    const std::string arg = ( iArgv && iArgv[1] ) ? iArgv[1] : "";
    if ( ( arg == "--help" ) || ( arg == "-h" ) )
    {
      PrintUsage( ( iArgc > 0 ) ? iArgv[0] : nullptr );
      return 0;
    }
    if ( arg == "--unit-tests" )
    {
      return RTRT::TetrisUnitTests::RunAll();
    }
    if ( ( arg == "--screenshot" ) && ( iArgc >= 3 ) )
    {
      screenshotPath = iArgv[2];
    }
    else
    {
      PrintUsage( ( iArgc > 0 ) ? iArgv[0] : nullptr );
      return 1;
    }
  }

  // Setup GLFW
  glfwSetErrorCallback(glfw_error_callback);
  if ( !glfwInit() )
  {
    std::cerr << "Failed to initialize GLFW!" << std::endl;
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  int result = 0;
  {
    const int screenWidth = 1280;
    const int screenHeight = 720;

    auto mainWindow = std::shared_ptr<GLFWwindow>(
      glfwCreateWindow(screenWidth, screenHeight, "RTRT - Tetris 3D", NULL, NULL),
      []( GLFWwindow * window ) { glfwDestroyWindow(window); }
    );

    if ( !mainWindow )
    {
      std::cerr << "Failed to create GLFW window!" << std::endl;
      glfwTerminate();
      return 1;
    }

    glfwMakeContextCurrent(mainWindow.get());
    glfwSwapInterval(1);

    RTRT::TetrisGameApp app(mainWindow, screenWidth, screenHeight);
    result = app.Run(screenshotPath, screenshotPath.empty() ? -1 : 15);
  }

  glfwTerminate();
  return result;
}
