#ifndef _RenderTestFramework_
#define _RenderTestFramework_

#include "Renderer.h"
#include "RendererFactory.h"

#include <filesystem>
#include <string>
#include <vector>

namespace RTRT
{

struct RenderSettings;
class Scene;

namespace Tests
{

struct ImageMetrics
{
  float  _MeanAbsoluteError = 0.f;
  float  _MaxAbsoluteError = 0.f;
  float  _MismatchRatio = 0.f;
  size_t _MismatchCount = 0;
};

struct RenderTestCase
{
  std::string     _Name;
  std::string     _ScenePath;
  RendererBackend _Backend = RendererBackend::DeferredRenderer;
  int             _Width = 320;
  int             _Height = 180;
  int             _FrameCount = 1;
  float           _MeanAbsoluteErrorThreshold = 0.f;
  float           _MaxAbsoluteErrorThreshold = 0.f;
  float           _PixelErrorThreshold = 0.f;
  float           _MismatchRatioThreshold = 0.f;
  std::filesystem::path _BaselinePath;
  bool            _OverrideCamera = false;
  Vec3            _CameraPosition = Vec3(0.f);
  Vec3            _CameraPivot = Vec3(0.f);
  float           _CameraFOV = 80.f;
  float           _CameraNear = 1.f;
  float           _CameraFar = 1000.f;
  std::string     _EnvironmentMapPath;
  bool            _SpecularIBL = true;
  bool            _SSR = true;
  bool            _Accumulate = true;
  bool            _AutoScale = true;
  bool            _TiledRendering = false;
  bool            _Denoise = false;
  int             _DebugMode = 0;
  bool            _DiagnosticOnly = false;
  int             _SamplesPerPixel = 1;
  int             _Bounces = 4;
  bool            _SoftwareOptimized = false;
  bool            _WBuffer = true;
  bool            _SoftwareSIMD = false;
  bool            _SoftwareFallback = false;
  bool            _Transparency = true;

  void ApplySettings( RenderSettings & ioSettings ) const;
  bool ApplyScene( Scene & ioScene ) const;
};

bool WritePFM( const std::filesystem::path & iPath, const RenderImage & iImage );
bool ReadPFM( const std::filesystem::path & iPath, RenderImage & oImage );
bool WriteDiagnosticPNG( const std::filesystem::path & iPath, const RenderImage & iImage, float iScale = 1.f );
bool WriteDiffPNG( const std::filesystem::path & iPath, const RenderImage & iActual, const RenderImage & iExpected );
bool CompareImages( const RenderImage & iActual, const RenderImage & iExpected, float iPixelErrorThreshold, ImageMetrics & oMetrics );
bool MatchesThresholds( const ImageMetrics & iMetrics, const RenderTestCase & iTestCase );

bool LoadRenderTestCases( const std::filesystem::path & iPath, std::vector<RenderTestCase> & oTestCases, std::string & oError );
bool ParseRenderTestCases( const std::string & iContents, std::vector<RenderTestCase> & oTestCases, std::string & oError );
int RunUnitTests( const std::filesystem::path & iArtifactsDir, bool iUseColor = false, bool iQuiet = false );

}

}

#endif /* _RenderTestFramework_ */
