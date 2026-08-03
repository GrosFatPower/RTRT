#include "RenderTestFramework.h"

#include "Loader.h"
#include "PathUtils.h"
#include "RenderSettings.h"
#include "Scene.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

namespace fs = std::filesystem;

namespace
{

static const int S_SkipReturnCode = 77;

class GLTestContext
{
public:
  ~GLTestContext()
  {
    if ( _Window )
      glfwDestroyWindow(_Window);
    if ( _Initialized )
      glfwTerminate();
  }

  bool Initialize()
  {
    if ( !glfwInit() )
      return false;
    _Initialized = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    _Window = glfwCreateWindow(64, 64, "RenderRegression", nullptr, nullptr);
    if ( !_Window )
      return false;

    glfwMakeContextCurrent(_Window);
    glfwSwapInterval(0);
    glewExperimental = GL_TRUE;
    if ( GLEW_OK != glewInit() )
      return false;
    glGetError();
    return true;
  }

private:
  GLFWwindow * _Window = nullptr;
  bool _Initialized = false;
};

void PrintUsage( const char * iExeName )
{
  std::cout << "Usage: " << iExeName << " [--list|--unit|--case <name>|--all] [--update-baselines] [--artifacts <directory>]" << std::endl;
}

bool WriteMetrics( const fs::path & iPath, const RTRT::Tests::ImageMetrics & iMetrics )
{
  std::error_code error;
  fs::create_directories(iPath.parent_path(), error);
  std::ofstream output(iPath);
  if ( !output )
    return false;

  output << "Mean absolute error: " << iMetrics._MeanAbsoluteError << "\n";
  output << "Maximum absolute error: " << iMetrics._MaxAbsoluteError << "\n";
  output << "Mismatch count: " << iMetrics._MismatchCount << "\n";
  output << "Mismatch ratio: " << iMetrics._MismatchRatio << "\n";
  return output.good();
}

int RunRenderCase( const RTRT::Tests::RenderTestCase & iTestCase, bool iUpdateBaselines, const fs::path & iArtifactsDir )
{
  RTRT::Scene scene;
  RTRT::RenderSettings settings;
  const std::string scenePath = RTRT::PathUtils::GetAssetPath(iTestCase._ScenePath);
  if ( !RTRT::Loader::LoadScene(scenePath, scene, settings) )
  {
    std::cerr << "Failed to load scene: " << scenePath << std::endl;
    return 1;
  }

  iTestCase.ApplySettings(settings);
  if ( !iTestCase.ApplyScene(scene) )
  {
    std::cerr << "Failed to load environment map for " << iTestCase._Name << std::endl;
    return 1;
  }
  std::unique_ptr<RTRT::Renderer> renderer = RTRT::CreateRenderer(iTestCase._Backend, scene, settings);
  if ( !renderer || ( 0 != renderer->Initialize() ) )
  {
    std::cerr << "Failed to initialize renderer for " << iTestCase._Name << std::endl;
    return 1;
  }

  int result = 0;
  for ( int frame = 0; frame < iTestCase._FrameCount; ++frame )
  {
    if ( ( 0 != renderer->Update() ) || ( 0 != renderer->RenderToTexture() ) )
    {
      std::cerr << "Failed to render frame " << frame << " for " << iTestCase._Name << std::endl;
      result = 1;
      break;
    }

    // The non-visible GLFW context has no swap operation to submit queued GPU work.
    glFlush();
    if ( 0 != renderer->Done() )
    {
      std::cerr << "Failed to finalize frame " << frame << " for " << iTestCase._Name << std::endl;
      result = 1;
      break;
    }
  }

  RTRT::RenderImage actual;
  if ( ( 0 == result ) && ( 0 != renderer->ReadbackFinalColor(actual) ) )
  {
    std::cerr << "Failed to read back final color for " << iTestCase._Name << std::endl;
    result = 1;
  }

  if ( 0 != result )
    return result;

  const fs::path baselinePath = fs::path(RTRT::PathUtils::GetAssetPath("..")) / iTestCase._BaselinePath;
  if ( iUpdateBaselines )
  {
    if ( !RTRT::Tests::WritePFM(baselinePath, actual) )
    {
      std::cerr << "Failed to write baseline: " << baselinePath << std::endl;
      return 1;
    }
    RTRT::Tests::WriteDiagnosticPNG(iArtifactsDir / iTestCase._Name / "actual.png", actual);
    std::cout << "Updated baseline: " << baselinePath << std::endl;
    return 0;
  }

  RTRT::RenderImage expected;
  if ( !RTRT::Tests::ReadPFM(baselinePath, expected) )
  {
    std::cerr << "Baseline is not initialized: " << baselinePath << std::endl;
    return S_SkipReturnCode;
  }

  RTRT::Tests::ImageMetrics metrics;
  if ( !RTRT::Tests::CompareImages(actual, expected, iTestCase._PixelErrorThreshold, metrics) )
  {
    std::cerr << "Image dimensions do not match for " << iTestCase._Name << std::endl;
    return 1;
  }

  if ( RTRT::Tests::MatchesThresholds(metrics, iTestCase) )
  {
    std::cout << "Render test passed: " << iTestCase._Name << std::endl;
    return 0;
  }

  const fs::path artifactPath = iArtifactsDir / iTestCase._Name;
  RTRT::Tests::WritePFM(artifactPath / "actual.pfm", actual);
  RTRT::Tests::WritePFM(artifactPath / "expected.pfm", expected);
  RTRT::Tests::WriteDiagnosticPNG(artifactPath / "actual.png", actual);
  RTRT::Tests::WriteDiagnosticPNG(artifactPath / "expected.png", expected);
  RTRT::Tests::WriteDiffPNG(artifactPath / "diff.png", actual, expected);
  WriteMetrics(artifactPath / "metrics.txt", metrics);
  std::cerr << "Render test failed: " << iTestCase._Name << std::endl;
  return 1;
}

}

int main( int iArgc, char ** iArgv )
{
  bool runUnitTests = false;
  bool runAll = false;
  bool updateBaselines = false;
  std::string caseName;
  fs::path artifactsDir = "Tests/Artifacts";

  for ( int i = 1; i < iArgc; ++i )
  {
    const std::string argument = iArgv[i];
    if ( "--list" == argument )
    {
      for ( const RTRT::Tests::RenderTestCase & testCase : RTRT::Tests::GetRenderTestCases() )
        std::cout << testCase._Name << std::endl;
      return 0;
    }
    if ( "--unit" == argument )
      runUnitTests = true;
    else if ( "--all" == argument )
      runAll = true;
    else if ( "--update-baselines" == argument )
      updateBaselines = true;
    else if ( "--case" == argument && ( i + 1 < iArgc ) )
      caseName = iArgv[++i];
    else if ( "--artifacts" == argument && ( i + 1 < iArgc ) )
      artifactsDir = iArgv[++i];
    else
    {
      PrintUsage(iArgv[0]);
      return 1;
    }
  }

  if ( !runUnitTests && !runAll && caseName.empty() )
  {
    PrintUsage(iArgv[0]);
    return 1;
  }

  RTRT::PathUtils::Initialize(iArgv[0]);
  if ( runUnitTests || runAll )
  {
    const int result = RTRT::Tests::RunUnitTests(artifactsDir / "unit");
    if ( 0 != result )
      return result;
  }

  std::vector<RTRT::Tests::RenderTestCase> selectedCases;
  for ( const RTRT::Tests::RenderTestCase & testCase : RTRT::Tests::GetRenderTestCases() )
  {
    if ( runAll || ( testCase._Name == caseName ) )
      selectedCases.push_back(testCase);
  }

  if ( !caseName.empty() && selectedCases.empty() )
  {
    std::cerr << "Unknown render case: " << caseName << std::endl;
    return 1;
  }

  if ( selectedCases.empty() )
    return 0;

  GLTestContext context;
  if ( !context.Initialize() )
  {
    std::cerr << "Skipped render tests: no compatible OpenGL context." << std::endl;
    return S_SkipReturnCode;
  }

  for ( const RTRT::Tests::RenderTestCase & testCase : selectedCases )
  {
    const int result = RunRenderCase(testCase, updateBaselines, artifactsDir);
    if ( 0 != result )
      return result;
  }

  return 0;
}
