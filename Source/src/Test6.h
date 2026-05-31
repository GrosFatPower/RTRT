#ifndef _Test6_
#define _Test6_

#include "BaseTest.h"
#include "Boids.h"
#include "FpsGame.h"
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


class Test6 : public BaseTest
{
public:
  Test6( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test6();

  int Run();

  static const char * GetTestHeader();

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
  void DrawDebugPanel();
  int DrawGameSettingsUI();
  int DrawRenderSettingsUI();
  int DrawRenderStatsUI();

  void SyncFramebufferResolution( bool iNotifyRenderer = false );

  void SetMouseCaptured( bool iCaptured );
  void HandleDroppedFiles( int iCount, const char ** iPaths );

  void ApplyRendererDefaults();
  void InitializeCpuTimings();
  void ResetCpuTimings();
  void BeginCpuTiming( int iTimingID );
  void EndCpuTiming( int iTimingID );
  void SetCpuTimingEnabled( int iTimingID, bool iEnabled );

  FpsGameEditorContext MakeEditorContext();
  FpsGameHudContext MakeHudContext() const;

protected:
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
  bool                      _ShowDebugPanel = true;
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
};

}

#endif /* _Test6_ */
