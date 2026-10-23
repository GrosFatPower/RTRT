#include "FpsGameApp.h"
#include "PathUtils.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <memory>
#include <string>
#include <algorithm>
#include <cstdlib>

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static int g_ScreenWidth  = 1280;
static int g_ScreenHeight = 720;

// ----------------------------------------------------------------------------
// PrintUsage
// ----------------------------------------------------------------------------
static void PrintUsage( const char * iExeName )
{
  const char * exeName = ( iExeName && iExeName[0] ) ? iExeName : "FpsGame";

  std::cout << "Usage: " << exeName << " [OPTIONS]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << "  --benchmark-software-control" << std::endl;
  std::cout << "  --benchmark-software LABEL [scalar|simd] TILE_SIZE [OPTIMIZATION] [fixed|ground|sky]" << std::endl;
  std::cout << "Examples:" << std::endl;
  std::cout << "  " << exeName << std::endl;
  std::cout << "  " << exeName << " --benchmark-software-control" << std::endl;
  std::cout << "  " << exeName << " --benchmark-software v2-test simd 64 direct-color fixed" << std::endl;
}

// ----------------------------------------------------------------------------
// glfw_error_callback
// ----------------------------------------------------------------------------
static void glfw_error_callback( int error, const char * description )
{
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main( int iArgc, char ** iArgv )
{
  int failure = 0;
  bool automaticSoftwareBenchmark = false;
  std::string benchmarkLabel = "v2-control";
  bool benchmarkSIMD = false;
  unsigned int benchmarkTileSize = 64;
  std::string benchmarkOptimization = "none";
  std::string benchmarkPose = "fixed";

  RTRT::PathUtils::Initialize( ( iArgv ) ? ( iArgv[0] ) : nullptr );

  if ( iArgc == 2 )
  {
    const std::string option = ( iArgv && iArgv[1] ) ? iArgv[1] : "";
    if ( option == "--help" || option == "-h" )
    {
      PrintUsage( ( iArgv ) ? ( iArgv[0] ) : nullptr );
      return 0;
    }
    else if ( option == "--benchmark-software-control" )
    {
      automaticSoftwareBenchmark = true;
    }
    else
    {
      PrintUsage( ( iArgv ) ? ( iArgv[0] ) : nullptr );
      return 1;
    }
  }
  else if ( ( iArgc >= 5 ) && ( iArgc <= 7 ) )
  {
    const std::string option = ( iArgv && iArgv[1] ) ? iArgv[1] : "";
    benchmarkLabel = ( iArgv && iArgv[2] ) ? iArgv[2] : "";
    const std::string simdMode = ( iArgv && iArgv[3] ) ? iArgv[3] : "";
    const int tileSize = ( iArgv && iArgv[4] ) ? std::atoi(iArgv[4]) : 0;
    if ( iArgc >= 6 && iArgv && iArgv[5] )
      benchmarkOptimization = iArgv[5];
    benchmarkPose = ( 7 == iArgc && iArgv && iArgv[6] ) ? iArgv[6] : "fixed";

    const bool validOptimization = ( benchmarkOptimization == "none" ) ||
                                   ( benchmarkOptimization == "incremental" ) ||
                                   ( benchmarkOptimization == "compact-hits" ) ||
                                   ( benchmarkOptimization == "direct-color" ) ||
                                   ( benchmarkOptimization == "frustum-culling" ) ||
                                   ( benchmarkOptimization == "pbo-upload" );
    const bool validPose = ( benchmarkPose == "fixed" ) || ( benchmarkPose == "ground" ) || ( benchmarkPose == "sky" );

    automaticSoftwareBenchmark = ( option == "--benchmark-software" ) && !benchmarkLabel.empty() &&
                                 ( ( simdMode == "scalar" ) || ( simdMode == "simd" ) ) &&
                                 ( tileSize >= 8 ) && validOptimization && validPose;
    benchmarkSIMD = ( simdMode == "simd" );
    benchmarkTileSize = static_cast<unsigned int>(std::max(8, tileSize));

    if ( !automaticSoftwareBenchmark )
    {
      PrintUsage( ( iArgv ) ? ( iArgv[0] ) : nullptr );
      return 1;
    }
  }
  else if ( iArgc > 2 )
  {
    PrintUsage( ( iArgv ) ? ( iArgv[0] ) : nullptr );
    return 1;
  }

  // Setup GLFW
  glfwSetErrorCallback(glfw_error_callback);
  if ( !glfwInit() )
  {
    std::cout << "Failed to initialize GLFW!" << std::endl;
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  {
    auto mainWindow = std::shared_ptr<GLFWwindow>(
      glfwCreateWindow(g_ScreenWidth, g_ScreenHeight, "RTRT - FPS Game", NULL, NULL),
      []( GLFWwindow * window ) { glfwDestroyWindow(window); }
    );

    if ( !mainWindow )
    {
      std::cout << "Failed to create GLFW window!" << std::endl;
      glfwTerminate();
      return 1;
    }

    glfwMakeContextCurrent(mainWindow.get());
    glfwSwapInterval(1);

    RTRT::FpsGameApp app(mainWindow, g_ScreenWidth, g_ScreenHeight);
    if ( automaticSoftwareBenchmark )
    {
      app.EnableAutomaticSoftwareBenchmark(benchmarkLabel, benchmarkSIMD, benchmarkTileSize,
                                           benchmarkOptimization, benchmarkPose);
    }

    failure = app.Run();
  }

  glfwTerminate();
  return failure;
}
