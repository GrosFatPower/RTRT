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

protected:
  int InitializeUI();
  int InitializeScene();
  int InitializeRenderer();
  int InitializeMapBoids( bool iResetSimulation );

  int ProcessInput();
  int UpdateGame();
  int UpdateBoids();
  int UpdateCPUTime();

  int DrawUI();
  void DrawDebugPanel();
  int DrawSettingsUI();

  void SyncFramebufferResolution( bool iNotifyRenderer = false );

  void SetMouseCaptured( bool iCaptured );

  void ApplyRendererDefaults();

  FpsGameEditorContext MakeEditorContext();
  FpsGameHudContext MakeHudContext() const;

protected:
  std::unique_ptr<Scene>    _Scene;
  RenderSettings            _Settings;
  std::unique_ptr<Renderer> _Renderer;
  bool                      _ReloadScene = false;
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

  bool                      _RenderToFile = false;
  std::filesystem::path     _CaptureOutputPath;
};

}

#endif /* _Test6_ */
