#include "PathUtils.h"
#include "TetrisGameApp.h"
#include "TetrisUnitTests.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
  std::cout << "  --unit-tests       Run headless Tetris Guideline unit tests" << std::endl;
  std::cout << "  --start-in-game    Skip Title Screen and launch directly into gameplay" << std::endl;
  std::cout << "  --screenshot PATH  Render frames and save screenshot to PATH" << std::endl;
  std::cout << "  --help, -h         Show this help message" << std::endl;
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
#if defined(_WIN32)
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD fileType = ( hOut != INVALID_HANDLE_VALUE && hOut != nullptr ) ? GetFileType(hOut) : FILE_TYPE_UNKNOWN;
  if ( ( fileType != FILE_TYPE_PIPE ) && ( fileType != FILE_TYPE_DISK ) )
  {
    if ( AttachConsole(ATTACH_PARENT_PROCESS) )
    {
      FILE * dummy = nullptr;
      freopen_s(&dummy, "CONOUT$", "w", stdout);
      freopen_s(&dummy, "CONOUT$", "w", stderr);
      std::ios::sync_with_stdio();
    }
  }
#endif

  RTRT::PathUtils::Initialize( ( iArgc > 0 ) ? iArgv[0] : nullptr );

  std::string screenshotPath;
  bool startInGame = false;

  for ( int i = 1; i < iArgc; ++i )
  {
    const std::string arg = ( iArgv && iArgv[i] ) ? iArgv[i] : "";
    if ( ( arg == "--help" ) || ( arg == "-h" ) )
    {
      PrintUsage( ( iArgc > 0 ) ? iArgv[0] : nullptr );
      return 0;
    }
    else if ( arg == "--unit-tests" )
    {
      return Tetris::TetrisUnitTests::RunAll();
    }
    else if ( arg == "--start-in-game" )
    {
      startInGame = true;
    }
    else if ( ( arg == "--screenshot" ) && ( ( i + 1 ) < iArgc ) )
    {
      screenshotPath = iArgv[++i];
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

    Tetris::TetrisGameApp app(mainWindow, screenWidth, screenHeight);
    app.SetStartInGame(startInGame);
    result = app.Run(screenshotPath, screenshotPath.empty() ? -1 : 15);
  }

  glfwTerminate();
  return result;
}
