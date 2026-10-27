#ifndef _FpsGameApp_
#define _FpsGameApp_

#include "Boids.h"
#include "FpsGame.h"
#include "FpsGameBenchmark.h"
#include "FpsGameEditor.h"
#include "FpsGameHud.h"
#include "FpsGameMap.h"
#include "KeyInput.h"
#include "MouseInput.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace RTRT
{

class FpsGameApp
{
public:
  FpsGameApp( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~FpsGameApp();

  int Run();
  void EnableAutomaticSoftwareBenchmark( const std::string & iLabel,
                                         bool iSIMD,
                                         unsigned int iTileSize,
                                         const std::string & iOptimization = "none",
                                         const std::string & iPose = "fixed" );

  static const char * GetTestHeader();
  static const char * GetAppHeader();

protected:
  static void KeyCallback( GLFWwindow * iWindow, const int iKey, const int iScancode, const int iAction, const int iMods );
  static void MouseButtonCallback( GLFWwindow * iWindow, const int iButton, const int iAction, const int iMods );
  static void MouseScrollCallback( GLFWwindow * iWindow, const double iOffsetX, const double iOffsetY );
  static void FramebufferSizeCallback( GLFWwindow * iWindow, const int iWidth, const int iHeight );
  static void DropCallback( GLFWwindow * iWindow, int iCount, const char ** iPaths );

protected:
  int InitializeUI();
  int InitializeScene();
  int InitializeRenderer();
  int InitializeMapBoids( bool iResetSimulation );
  int RefreshMapBoids();
  int RefreshPropCollisionColliders();

  int ProcessInput();
  int UpdateGame();
  int UpdateBoids();
  int UpdateCPUTime();

  int DrawUI();
  int DrawMainMenuBar();
  int DrawHelpUI();
  void DrawDebugPanel();
  int DrawGameSettingsUI();
  int DrawRenderSettingsUI();
  int DrawRenderStatsUI();
  int DrawBenchmarkUI();

  void SyncFramebufferResolution( bool iNotifyRenderer = false );

  bool SetEditorMode( bool iEnabled );
  void SetMouseCaptured( bool iCaptured );
  void SetRendererMode( FpsRendererMode iMode );
  void HandleDroppedFiles( int iCount, const char ** iPaths );
  void ApplyBenchmarkPose();
  void StartBenchmark();
  void UpdateBenchmark();
  void FinishBenchmark();

  void ApplyRendererDefaults();
  void InitializeCpuTimings();
  void ResetCpuTimings();
  void BeginCpuTiming( int iTimingID );
  void EndCpuTiming( int iTimingID );
  void SetCpuTimingEnabled( int iTimingID, bool iEnabled );

  FpsGameEditorContext MakeEditorContext();
  FpsGameHudContext MakeHudContext() const;

protected:
  std::shared_ptr<GLFWwindow> _MainWindow;
  int                         _ScreenWidth = 0;
  int                         _ScreenHeight = 0;

  std::unique_ptr<Scene>    _Scene;
  RenderSettings            _Settings;
  std::unique_ptr<Renderer> _Renderer;
  bool                      _ReloadScene = false;
  bool                      _ReloadBoids = false;
  bool                      _ReloadRenderer = false;

  FpsGameSettings           _GameSettings;
  FpsGameWorld              _GameWorld;
  FpsGameSceneBinding       _SceneBinding;
  FpsGameMap                _Map;
  std::string               _MapPath;
  bool                      _MapLoaded = false;
  FpsGameEditor             _Editor;
  FpsGameHud                _Hud;
  std::vector<BoidSettings>      _BoidSettings;
  std::vector<BoidSimulation>    _BoidSimulations;
  std::vector<BoidSceneBinding>  _BoidBindings;

  KeyInput                  _KeyInput;
  MouseInput                _MouseInput;

  bool                      _MouseCaptured = false;
  bool                      _ExitToTestSelection = false;
  bool                      _ShowMenuBar = true;
  bool                      _AltMenuToggleArmed = false;
  bool                      _ShowDebugPanel = true;
  bool                      _ShowRenderSettingsPanel = true;
  bool                      _ShowRenderStatsPanel = true;
  bool                      _ShowHelpDialog = false;
  bool                      _HasLastMousePos = false;
  double                    _LastMouseX = 0.;
  double                    _LastMouseY = 0.;

  double                    _CPUTime = 0.;
  double                    _DeltaTime = 0.;
  double                    _FrameRate = 0.;
  double                    _FrameTime = 0.;
  unsigned int              _NbRenderedFrames = 0;
  int                       _DebugMode = 0;
  int                       _DeferredDebugView = 0;
  bool                      _DeferredShowWires = false;
  int                       _SoftwareDebugView = 0;
  bool                      _SoftwareShowWires = false;

  bool                      _RenderToFile = false;
  std::filesystem::path     _CaptureOutputPath;

  enum CpuTimingID
  {
    CpuProcessInput = 0,
    CpuUpdateGame,
    CpuUpdateBoids,
    CpuBoidsSimulation,
    CpuBoidsSceneSync,
    CpuBoidsRendererNotify,
    CpuRendererUpdate,
    CpuRenderToTexture,
    CpuRenderToScreen,
    CpuRenderToFile,
    CpuDrawUI,
    CpuSwapBuffers,
    CpuTimingCount
  };

  std::vector<FpsCpuTiming> _CpuTimings;
  std::vector<FpsCpuTiming> _DisplayedCpuTimings;
  double                    _CpuTimingStart[CpuTimingCount] = {};

  FpsGameBenchmark _Benchmark;
  bool             _AutomaticBenchmark = false;
  bool             _AutomaticBenchmarkSIMD = false;
  unsigned int     _AutomaticBenchmarkTileSize = 64;
  std::string      _AutomaticBenchmarkOptimization = "none";
};

using Test6 = FpsGameApp;

}

#endif /* _FpsGameApp_ */
