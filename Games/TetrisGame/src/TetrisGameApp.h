#ifndef _TetrisGameApp_
#define _TetrisGameApp_

#include "KeyInput.h"
#include "MouseInput.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "TetrisGameLogic.h"
#include "TetrisHud.h"
#include "TetrisSceneManager.h"
#include "TetrisTypes.h"

#include <memory>
#include <string>

struct GLFWwindow;

namespace RTRT
{

class TetrisGameApp
{
public:
  TetrisGameApp( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~TetrisGameApp();

  int Run( const std::string & iCapturePath = "", int iMaxFrames = -1 );

  static const char * GetAppHeader() { return "Tetris 3D"; }

protected:
  static void KeyCallback( GLFWwindow * iWindow, int iKey, int iScancode, int iAction, int iMods );
  static void MouseButtonCallback( GLFWwindow * iWindow, int iButton, int iAction, int iMods );
  static void MouseScrollCallback( GLFWwindow * iWindow, double iOffsetX, double iOffsetY );
  static void FramebufferSizeCallback( GLFWwindow * iWindow, int iWidth, int iHeight );

protected:
  int InitializeUI();
  int InitializeScene();
  int InitializeRenderer();

  int ProcessInput();
  int UpdateGame( float iDeltaTime );
  int DrawUI();

  void SyncFramebufferResolution( bool iNotifyRenderer = false );
  void SetRendererMode( TetrisRendererMode iMode );
  void ApplyRendererDefaults();

protected:
  std::shared_ptr<GLFWwindow> _MainWindow;
  int                         _ScreenWidth  = 0;
  int                         _ScreenHeight = 0;

  KeyInput                    _KeyInput;
  MouseInput                  _MouseInput;

  std::unique_ptr<Scene>      _Scene;
  RenderSettings              _Settings;
  std::unique_ptr<Renderer>   _Renderer;

  TetrisGameLogic             _GameLogic;
  TetrisSceneManager          _SceneManager;
  TetrisHud                   _Hud;

  TetrisRendererMode          _RendererMode = TetrisRendererMode::Deferred;
  TetrisHandlingConfig        _HandlingConfig;
  TetrisInputState            _InputState;

  bool                        _ReloadRenderer = false;
  bool                        _ReloadScene    = false;

  // Profiling and timings
  double                      _FrameRate         = 60.;
  double                      _FrameTime         = 16.6;
  double                      _DeltaTime         = 0.016;
  unsigned int                _NbRenderedFrames  = 0;
  double                      _LastTime          = 0.;

  // Mouse drag for FreeOrbit camera
  bool                        _IsMouseDragging   = false;
  double                      _LastMouseX        = 0.;
  double                      _LastMouseY        = 0.;
};

}

#endif /* _TetrisGameApp_ */
