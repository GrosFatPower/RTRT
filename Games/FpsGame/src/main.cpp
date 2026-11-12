#include "FpsGameApp.h"
#include "PathUtils.h"
#include "Localization.h"
#include "UserPreferences.h"
#include "MathUtil.h"

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
  std::cout << "  --unit-tests" << std::endl;
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
    else if ( option == "--unit-tests" )
    {
      std::cout << "==========================================" << std::endl;
      std::cout << "Running FPS Game Localization & Preferences Tests" << std::endl;
      std::cout << "==========================================" << std::endl;

      RTRT::LocalizationManager & locMgr = RTRT::LocalizationManager::Instance();
      locMgr.Initialize();

      if ( !locMgr.IsComplete(RTRT::Language::English) )
      {
        std::cerr << "FAIL: English localization table has missing keys!" << std::endl;
        return 1;
      }
      if ( !locMgr.IsComplete(RTRT::Language::French) )
      {
        std::cerr << "FAIL: French localization table has missing keys!" << std::endl;
        return 1;
      }

      // Check key retrieval
      RTRT::Loc::SetLanguage(RTRT::Language::English);
      const char * enTitle = RTRT::Loc::Tr(RTRT::LocId::Fps_Title);
      if ( !enTitle || std::string(enTitle) != "RTRT TACTICAL FPS" )
      {
        std::cerr << "FAIL: English Fps_Title mismatch: " << ( enTitle ? enTitle : "null" ) << std::endl;
        return 1;
      }

      RTRT::Loc::SetLanguage(RTRT::Language::French);
      const char * frTitle = RTRT::Loc::Tr(RTRT::LocId::Fps_Title);
      if ( !frTitle || std::string(frTitle) != "RTRT FPS TACTIQUE" )
      {
        std::cerr << "FAIL: French Fps_Title mismatch: " << ( frTitle ? frTitle : "null" ) << std::endl;
        return 1;
      }

      const char * frDeploy = RTRT::Loc::Tr(RTRT::LocId::Fps_Menu_Deploy);
      if ( !frDeploy || std::string(frDeploy) != "DÉPLOYER / CHOISIR CARTE" )
      {
        std::cerr << "FAIL: French Fps_Menu_Deploy mismatch: " << ( frDeploy ? frDeploy : "null" ) << std::endl;
        return 1;
      }

      // Test UserPreferences persistence roundtrip
      {
        RTRT::UserPreferences prefs;
        prefs._Language = RTRT::Language::French;
        prefs._Audio._MasterVolume = 0.65f;
        prefs._VSync = true;
        prefs._CustomRenderResolution = { 1920, 1080 };

        const std::string testFile = RTRT::UserPreferences::GetDefaultFilePath("fps_test_prefs.json");
        if ( !prefs.SaveToFile(testFile) )
        {
          std::cerr << "FAIL: Failed to save test preferences to " << testFile << std::endl;
          return 1;
        }

        RTRT::UserPreferences loaded;
        if ( !loaded.LoadFromFile(testFile) )
        {
          std::cerr << "FAIL: Failed to load test preferences from " << testFile << std::endl;
          return 1;
        }

        std::remove(testFile.c_str());

        if ( loaded._Language != RTRT::Language::French ||
             std::abs(loaded._Audio._MasterVolume - 0.65f) > 0.001f ||
             !loaded._VSync ||
             loaded._CustomRenderResolution.x != 1920 ||
             loaded._CustomRenderResolution.y != 1080 )
        {
          std::cerr << "FAIL: Preferences roundtrip values mismatched!" << std::endl;
          return 1;
        }
      }

      // Restore default English
      RTRT::Loc::SetLanguage(RTRT::Language::English);

      std::cout << "[PASS] FPS Game Localization (100% EN & FR) and Preferences test passed." << std::endl;
      std::cout << "==========================================" << std::endl;
      return 0;
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

    FPS::FpsGameApp app(mainWindow, g_ScreenWidth, g_ScreenHeight);
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
