#ifndef _FpsGameBenchmark_
#define _FpsGameBenchmark_

#include "FpsGame.h"
#include "FpsGameMap.h"
#include "FpsGameTiming.h"
#include "Renderer.h"

#include <string>
#include <vector>

namespace RTRT
{

class Scene;
struct RenderSettings;

struct FpsGameBenchmarkPassTiming
{
  std::string _Name;
  double      _Seconds = 0.;
  bool        _GPU = false;
  bool        _Enabled = false;
};

struct FpsGameBenchmarkSaveContext
{
  std::string      _MapPath;
  FpsGameMap       _SceneSnapshot;
  FpsPlayer        _Player;
  const Scene      * _Scene = nullptr;
  const RenderSettings * _Settings = nullptr;
};

class FpsGameBenchmark
{
public:
  void Start( FpsRendererMode iRendererMode );
  void Cancel( const char * iReason = nullptr );
  void Update( const std::vector<FpsCpuTiming> & iCpuTimings,
               const Renderer & iRenderer,
               FpsRendererMode iActiveRendererMode,
               bool iRendererReady );
  bool SaveResult( const FpsGameBenchmarkSaveContext & iContext );

  bool IsRunning() const { return _Running; }
  bool IsCompleted() const { return _Completed; }
  const std::string & GetStatus() const { return _Status; }
  const std::string & GetResultPath() const { return _ResultPath; }
  FpsRendererMode GetRendererMode() const { return _RendererMode; }

  int & GetWarmupFrames() { return _WarmupFrames; }
  int & GetSampleFrames() { return _SampleFrames; }
  int GetWarmupDone() const { return _WarmupDone; }
  int GetSamplesDone() const { return _SamplesDone; }
  double GetCpuFrameSeconds() const { return _CpuFrameSeconds; }
  double GetRendererFrameSeconds() const { return _RendererFrameSeconds; }
  const std::vector<FpsCpuTiming> & GetCpuTotals() const { return _CpuTotals; }
  const std::vector<FpsGameBenchmarkPassTiming> & GetRendererTotals() const { return _RendererTotals; }

  static const Vec3 & GetPosition();
  static float GetYaw();
  static float GetPitch();
  static const char * GetRendererName( FpsRendererMode iRendererMode );

protected:
  bool _Running = false;
  bool _Completed = false;
  std::string _Status;
  std::string _ResultPath;
  int _WarmupFrames = 10;
  int _SampleFrames = 120;
  int _WarmupDone = 0;
  int _SamplesDone = 0;
  FpsRendererMode _RendererMode = FpsRendererMode::Deferred;
  double _CpuFrameSeconds = 0.;
  double _RendererFrameSeconds = 0.;
  std::vector<FpsCpuTiming> _CpuTotals;
  std::vector<FpsGameBenchmarkPassTiming> _RendererTotals;
};

}

#endif /* _FpsGameBenchmark_ */
