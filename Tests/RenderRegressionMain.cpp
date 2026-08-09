#include "RenderTestFramework.h"

#include "DeferredRenderer.h"
#include "Loader.h"
#include "PathUtils.h"
#include "RenderSettings.h"
#include "Scene.h"

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <chrono>
#include <cstdio>

namespace fs = std::filesystem;

namespace
{

static const int S_SkipReturnCode = 77;

FILE * OpenTraceFile( const fs::path & iPath )
{
#if defined(_WIN32)
  FILE * file = nullptr;
  fopen_s(&file, iPath.string().c_str(), "w");
  return file;
#else
  return fopen(iPath.string().c_str(), "w");
#endif
}

int DuplicateFileDescriptor( int iDescriptor )
{
#if defined(_WIN32)
  return _dup(iDescriptor);
#else
  return dup(iDescriptor);
#endif
}

int GetFileDescriptor( FILE * iFile )
{
#if defined(_WIN32)
  return _fileno(iFile);
#else
  return fileno(iFile);
#endif
}

int RedirectFileDescriptor( int iSource, int iDestination )
{
#if defined(_WIN32)
  return _dup2(iSource, iDestination);
#else
  return dup2(iSource, iDestination);
#endif
}

void CloseFileDescriptor( int iDescriptor )
{
#if defined(_WIN32)
  _close(iDescriptor);
#else
  close(iDescriptor);
#endif
}

enum class RenderCaseStatus
{
  Passed,
  Failed,
  Skipped,
  Updated,
  Captured
};

struct RenderCaseOutcome
{
  RenderCaseStatus _Status = RenderCaseStatus::Failed;
  std::string _Reason;
};

class TraceCapture
{
public:
  explicit TraceCapture( const fs::path & iPath )
  {
    std::error_code error;
    fs::create_directories(iPath.parent_path(), error);
    _File = OpenTraceFile(iPath);
    if ( !_File )
      return;

    std::cout.flush();
    std::cerr.flush();
    fflush(stdout);
    fflush(stderr);
    _StdOut = DuplicateFileDescriptor(GetFileDescriptor(stdout));
    _StdErr = DuplicateFileDescriptor(GetFileDescriptor(stderr));
    if ( ( _StdOut < 0 ) || ( _StdErr < 0 ) )
      return;
    RedirectFileDescriptor(GetFileDescriptor(_File), GetFileDescriptor(stdout));
    RedirectFileDescriptor(GetFileDescriptor(_File), GetFileDescriptor(stderr));
    _Active = true;
  }

  ~TraceCapture()
  {
    if ( _Active )
    {
      std::cout.flush();
      std::cerr.flush();
      fflush(stdout);
      fflush(stderr);
      RedirectFileDescriptor(_StdOut, GetFileDescriptor(stdout));
      RedirectFileDescriptor(_StdErr, GetFileDescriptor(stderr));
    }
    if ( _StdOut >= 0 )
      CloseFileDescriptor(_StdOut);
    if ( _StdErr >= 0 )
      CloseFileDescriptor(_StdErr);
    if ( _File )
      fclose(_File);
  }

  bool IsActive() const { return _Active; }

private:
  FILE * _File = nullptr;
  int _StdOut = -1;
  int _StdErr = -1;
  bool _Active = false;
};

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
  std::cout << "Usage: " << iExeName << " [--list|--unit|--case <name>|--all] [--update-baselines] [--artifacts <directory>] [--manifest <file>]" << std::endl;
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

RenderCaseOutcome RunRenderCase( const RTRT::Tests::RenderTestCase & iTestCase, bool iUpdateBaselines, const fs::path & iArtifactsDir )
{
  RenderCaseOutcome outcome;
  RTRT::Scene scene;
  RTRT::RenderSettings settings;
  const std::string scenePath = RTRT::PathUtils::GetAssetPath(iTestCase._ScenePath);
  if ( !RTRT::Loader::LoadScene(scenePath, scene, settings) )
  {
    std::cerr << "Failed to load scene: " << scenePath << std::endl;
    outcome._Reason = "scene load failed";
    return outcome;
  }

  iTestCase.ApplySettings(settings);
  if ( !iTestCase.ApplyScene(scene) )
  {
    std::cerr << "Failed to load environment map for " << iTestCase._Name << std::endl;
    outcome._Reason = "environment map load failed";
    return outcome;
  }
  std::unique_ptr<RTRT::Renderer> renderer = RTRT::CreateRenderer(iTestCase._Backend, scene, settings);
  if ( !renderer )
  {
    std::cerr << "Failed to create renderer for " << iTestCase._Name << std::endl;
    outcome._Reason = "renderer creation failed";
    return outcome;
  }
  int debugMode = iTestCase._DebugMode;
  if ( iTestCase._DiagnosticOnly && ( RTRT::RendererBackend::DeferredRenderer == iTestCase._Backend ) )
    debugMode |= (int)RTRT::DeferredDebugModes::Diagnostic;
  renderer->SetDebugMode(debugMode);
  if ( 0 != renderer->Initialize() )
  {
    std::cerr << "Failed to initialize renderer for " << iTestCase._Name << std::endl;
    outcome._Reason = "renderer initialization failed";
    return outcome;
  }

  int result = 0;
  for ( int frame = 0; frame < iTestCase._FrameCount; ++frame )
  {
    if ( ( 0 != renderer->Update() ) || ( 0 != renderer->RenderToTexture() ) )
    {
      std::cerr << "Failed to render frame " << frame << " for " << iTestCase._Name << std::endl;
      outcome._Reason = "render failed at frame " + std::to_string(frame);
      result = 1;
      break;
    }

    // The non-visible GLFW context has no swap operation to submit queued GPU work.
    glFlush();
    if ( 0 != renderer->Done() )
    {
      std::cerr << "Failed to finalize frame " << frame << " for " << iTestCase._Name << std::endl;
      outcome._Reason = "finalization failed at frame " + std::to_string(frame);
      result = 1;
      break;
    }
  }

  RTRT::RenderImage actual;
  if ( ( 0 == result ) && ( 0 != renderer->ReadbackFinalColor(actual) ) )
  {
    std::cerr << "Failed to read back final color for " << iTestCase._Name << std::endl;
    outcome._Reason = "final color readback failed";
    result = 1;
  }

  if ( 0 != result )
    return outcome;

  if ( iTestCase._DiagnosticOnly )
  {
    const fs::path artifactPath = iArtifactsDir / iTestCase._Name;
    if ( !RTRT::Tests::WritePFM(artifactPath / "actual.pfm", actual) || !RTRT::Tests::WriteDiagnosticPNG(artifactPath / "actual.png", actual) )
    {
      std::cerr << "Failed to write diagnostic capture for " << iTestCase._Name << std::endl;
      outcome._Reason = "diagnostic write failed";
      return outcome;
    }
    outcome._Status = RenderCaseStatus::Captured;
    outcome._Reason = "diagnostic capture";
    return outcome;
  }

  const fs::path baselinePath = fs::path(RTRT::PathUtils::GetAssetPath("..")) / iTestCase._BaselinePath;
  if ( iUpdateBaselines )
  {
    if ( !RTRT::Tests::WritePFM(baselinePath, actual) )
    {
      std::cerr << "Failed to write baseline: " << baselinePath << std::endl;
      outcome._Reason = "baseline write failed";
      return outcome;
    }
    RTRT::Tests::WriteDiagnosticPNG(iArtifactsDir / iTestCase._Name / "actual.png", actual);
    outcome._Status = RenderCaseStatus::Updated;
    return outcome;
  }

  RTRT::RenderImage expected;
  if ( !RTRT::Tests::ReadPFM(baselinePath, expected) )
  {
    std::cerr << "Baseline is not initialized: " << baselinePath << std::endl;
    outcome._Status = RenderCaseStatus::Skipped;
    outcome._Reason = "baseline is not initialized";
    return outcome;
  }

  RTRT::Tests::ImageMetrics metrics;
  if ( !RTRT::Tests::CompareImages(actual, expected, iTestCase._PixelErrorThreshold, metrics) )
  {
    std::cerr << "Image dimensions do not match for " << iTestCase._Name << std::endl;
    outcome._Reason = "image dimensions do not match";
    return outcome;
  }

  if ( RTRT::Tests::MatchesThresholds(metrics, iTestCase) )
  {
    outcome._Status = RenderCaseStatus::Passed;
    return outcome;
  }

  const fs::path artifactPath = iArtifactsDir / iTestCase._Name;
  RTRT::Tests::WritePFM(artifactPath / "actual.pfm", actual);
  RTRT::Tests::WritePFM(artifactPath / "expected.pfm", expected);
  RTRT::Tests::WriteDiagnosticPNG(artifactPath / "actual.png", actual);
  RTRT::Tests::WriteDiagnosticPNG(artifactPath / "expected.png", expected);
  RTRT::Tests::WriteDiffPNG(artifactPath / "diff.png", actual, expected);
  WriteMetrics(artifactPath / "metrics.txt", metrics);
  outcome._Reason = "image mismatch";
  return outcome;
}

bool EnableConsoleColors()
{
#if defined(_WIN32)
  HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD mode = 0;
  if ( ( INVALID_HANDLE_VALUE == output ) || !GetConsoleMode(output, &mode) )
    return false;
  return SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
  return isatty(GetFileDescriptor(stdout)) != 0;
#endif
}

void PrintCaseStatus( const RTRT::Tests::RenderTestCase & iTestCase, const RenderCaseOutcome & iOutcome, double iSeconds, const fs::path & iArtifactsDir, bool iUseColor )
{
  const char * label = "FAIL";
  const char * color = "\x1b[31m";
  if ( RenderCaseStatus::Passed == iOutcome._Status ) { label = "PASS"; color = "\x1b[32m"; }
  if ( RenderCaseStatus::Skipped == iOutcome._Status ) { label = "SKIP"; color = "\x1b[33m"; }
  if ( RenderCaseStatus::Updated == iOutcome._Status ) { label = "UPDATED"; color = "\x1b[32m"; }
  if ( RenderCaseStatus::Captured == iOutcome._Status ) { label = "CAPTURED"; color = "\x1b[32m"; }
  if ( iUseColor ) std::cout << color;
  std::cout << "[" << label << "]";
  if ( iUseColor ) std::cout << "\x1b[0m";
  std::cout << " " << iTestCase._Name << " (" << iSeconds << " s)";
  if ( !iOutcome._Reason.empty() )
    std::cout << " - " << iOutcome._Reason;
  if ( RenderCaseStatus::Failed == iOutcome._Status )
    std::cout << "; see " << ( iArtifactsDir / iTestCase._Name ).generic_string();
  std::cout << std::endl;
}

}

int main( int iArgc, char ** iArgv )
{
  bool runUnitTests = false;
  bool runAll = false;
  bool updateBaselines = false;
  bool listCases = false;
  bool customManifest = false;
  std::string caseName;
  fs::path artifactsDir = "Tests/Artifacts";
  fs::path manifestPath;

  for ( int i = 1; i < iArgc; ++i )
  {
    const std::string argument = iArgv[i];
    if ( "--list" == argument )
      listCases = true;
    else if ( "--unit" == argument )
      runUnitTests = true;
    else if ( "--all" == argument )
      runAll = true;
    else if ( "--update-baselines" == argument )
      updateBaselines = true;
    else if ( "--case" == argument && ( i + 1 < iArgc ) )
      caseName = iArgv[++i];
    else if ( "--artifacts" == argument && ( i + 1 < iArgc ) )
      artifactsDir = iArgv[++i];
    else if ( "--manifest" == argument && ( i + 1 < iArgc ) )
    {
      manifestPath = iArgv[++i];
      customManifest = true;
    }
    else
    {
      PrintUsage(iArgv[0]);
      return 1;
    }
  }

  if ( !listCases && !runUnitTests && !runAll && caseName.empty() )
  {
    PrintUsage(iArgv[0]);
    return 1;
  }

  RTRT::PathUtils::Initialize(iArgv[0]);
  const bool useColor = EnableConsoleColors();
  if ( !customManifest )
    manifestPath = fs::path(RTRT::PathUtils::GetAssetPath("..")) / "Tests/RenderTests.json";

  if ( runUnitTests || runAll )
  {
    const int result = RTRT::Tests::RunUnitTests(artifactsDir / "unit", useColor);
    if ( 0 != result )
      return result;
  }

  std::vector<RTRT::Tests::RenderTestCase> allTestCases;
  if ( listCases || runAll || !caseName.empty() )
  {
    std::string manifestError;
    if ( !RTRT::Tests::LoadRenderTestCases(manifestPath, allTestCases, manifestError) )
    {
      std::cerr << "Failed to load render-test manifest '" << manifestPath << "': " << manifestError << std::endl;
      return 1;
    }
  }

  if ( listCases )
  {
    for ( const RTRT::Tests::RenderTestCase & testCase : allTestCases )
      std::cout << testCase._Name << std::endl;
    return 0;
  }

  std::vector<RTRT::Tests::RenderTestCase> selectedCases;
  for ( const RTRT::Tests::RenderTestCase & testCase : allTestCases )
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

  int passed = 0;
  int failed = 0;
  int skipped = 0;
  int updated = 0;
  int captured = 0;
  for ( const RTRT::Tests::RenderTestCase & testCase : selectedCases )
  {
    const auto start = std::chrono::steady_clock::now();
    const fs::path caseArtifactsDir = artifactsDir / testCase._Name;
    RenderCaseOutcome outcome;
    {
      TraceCapture trace(caseArtifactsDir / "trace.log");
      if ( !trace.IsActive() )
      {
        outcome._Reason = "unable to create trace log";
      }
      else
        outcome = RunRenderCase(testCase, updateBaselines, artifactsDir);
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    PrintCaseStatus(testCase, outcome, seconds, artifactsDir, useColor);
    if ( RenderCaseStatus::Passed == outcome._Status ) ++passed;
    else if ( RenderCaseStatus::Skipped == outcome._Status ) ++skipped;
    else if ( RenderCaseStatus::Updated == outcome._Status ) ++updated;
    else if ( RenderCaseStatus::Captured == outcome._Status ) ++captured;
    else ++failed;
  }

  std::cout << "Summary: " << passed << " passed, " << failed << " failed, " << skipped << " skipped, " << updated << " updated, " << captured << " captured." << std::endl;
  if ( failed > 0 )
    return 1;
  if ( ( 0 == passed ) && ( 0 == updated ) && ( 0 == captured ) && ( skipped > 0 ) )
    return S_SkipReturnCode;
  return 0;
}
