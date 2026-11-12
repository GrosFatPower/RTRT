#ifndef _TetrisGameApp_
#define _TetrisGameApp_

#include "AudioEngine.h"
#include "GameMenuUI.h"
#include "GamepadInput.h"
#include "KeyInput.h"
#include "MouseInput.h"
#include "NetSession.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "TetrisGameLogic.h"
#include "TetrisHud.h"
#include "TetrisNetProtocol.h"
#include "TetrisSceneManager.h"
#include "TetrisTypes.h"

#include <memory>
#include <string>

struct GLFWwindow;

namespace Tetris
{
using namespace RTRT;

struct TetrisPlayerAudioTracker
{
  int             _PrevPieceOriginX = 0;
  int             _PrevPieceOriginY = 0;
  TetrisPieceType _PrevActivePiece  = TetrisPieceType::None;
  TetrisRotation  _PrevRotation     = TetrisRotation::Spawn;
  TetrisPieceType _PrevHoldPiece    = TetrisPieceType::None;
  unsigned int    _PrevLockEventId  = 0;
  unsigned int    _PrevLineClearId  = 0;
  int             _PrevComboCount   = -1;
  int             _PrevLevel        = 1;
  bool            _PrevGameOver     = false;
  bool            _PrevVictory      = false;

  void Reset( const TetrisGameLogic & iLogic )
  {
    _PrevPieceOriginX = iLogic.GetActiveOrigin().x;
    _PrevPieceOriginY = iLogic.GetActiveOrigin().y;
    _PrevActivePiece  = iLogic.GetActivePiece();
    _PrevRotation     = iLogic.GetActiveRotation();
    _PrevHoldPiece    = iLogic.GetHoldPiece();
    _PrevLockEventId  = iLogic.GetLockEventId();
    _PrevLineClearId  = iLogic.GetLineClearEventId();
    _PrevComboCount   = iLogic.GetComboCount();
    _PrevLevel        = iLogic.GetLevel();
    _PrevGameOver     = iLogic.IsGameOver();
    _PrevVictory      = iLogic.IsVictory();
  }
};

class TetrisGameApp
{
public:
  TetrisGameApp( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~TetrisGameApp();

  int Run( const std::string & iCapturePath = "", int iMaxFrames = -1 );

  void SetStartInGame( bool iStartInGame ) { _AppState = iStartInGame ? TetrisAppState::InGame : TetrisAppState::TitleScreen; }

  void ToggleFullscreen();
  void SetFullscreen( bool iFullscreen );
  bool IsFullscreen() const;

  static const char * GetAppHeader() { return "Tetris 3D"; }

  // Public static helpers for input device assignment & routing (testable by unit tests)
  static void ResolveDevicesForPreset( TetrisMultiplayerPreset iPreset,
                                       int iNumPlayers,
                                       bool iGp1Connected,
                                       bool iGp2Connected,
                                       TetrisControlDevice & oP1Device,
                                       TetrisControlDevice & oP2Device );

  static void RouteKeyPressToInput( int iKey,
                                    TetrisControlDevice iDevice,
                                    TetrisInputState & ioInput );

  static void PollHeldKeysForDevice( const KeyInput & iKeyInput,
                                     TetrisControlDevice iDevice,
                                     bool & oLeftHeld,
                                     bool & oRightHeld,
                                     bool & oSoftDropHeld );

protected:
  static void KeyCallback( GLFWwindow * iWindow, int iKey, int iScancode, int iAction, int iMods );
  static void MouseButtonCallback( GLFWwindow * iWindow, int iButton, int iAction, int iMods );
  static void MouseScrollCallback( GLFWwindow * iWindow, double iOffsetX, double iOffsetY );
  static void FramebufferSizeCallback( GLFWwindow * iWindow, int iWidth, int iHeight );

protected:
  int InitializeUI();
  int InitializeScene();
  int InitializeRenderer();
  void InitializeSoundBank();

  int ProcessInput();
  int UpdateGame( float iDeltaTime );
  int DrawUI();

  void ResolvePlayerDevices();
  void PollGamepadForPlayer( GamepadInput & ioGamepad, TetrisInputState & ioInput, bool iInActiveGameplay );
  void StartMatch( int iNumPlayers, TetrisGameMode iMode, int iStartLevel, bool iResetSeries, unsigned int iExplicitSeed = 0 );
  void CheckPendingMatchRequests();
  void UpdateNetwork( float iDeltaTime );
  void SendHelloPacket();
  void HostLaunchOnlineMatch( bool iResetSeries );
  void EvaluatePlayerAudio( int iPlayerIdx, const Vec3 & iBoardPos, bool iUseSpatial3D );
  bool IsInActiveGameplay() const;

  void SyncFramebufferResolution( bool iNotifyRenderer = false );
  void SetRendererMode( TetrisRendererMode iMode );
  void ApplyRendererDefaults();

protected:
  std::shared_ptr<GLFWwindow> _MainWindow;
  int                         _ScreenWidth  = 0;
  int                         _ScreenHeight = 0;

  KeyInput                    _KeyInput;
  MouseInput                  _MouseInput;
  GamepadInput                _GamepadInputs[2];

  std::unique_ptr<Scene>      _Scene;
  RenderSettings              _Settings;
  std::unique_ptr<Renderer>   _Renderer;

  TetrisGameLogic             _GameLogic[2];
  TetrisSceneManager          _SceneManager;
  TetrisHud                   _Hud;
  AudioEngine                 _Audio;
  NetSession                  _NetSession;

  TetrisMultiplayerState      _MpState;
  bool                        _RoundResolved    = false;
  float                       _NetSnapshotTimer = 0.0f;
  uint32_t                    _NetOutboundSeq   = 0;
  uint32_t                    _NetAttackEventSeq= 0;
  std::string                 _LastSentNickname;

  TetrisAppState              _AppState     = TetrisAppState::TitleScreen;
  TetrisAppState              _PrevAppState = TetrisAppState::TitleScreen;
  int                         _PrevLevel    = 1;
  double                      _PrevElapsed  = 0.0;
  TetrisRendererMode          _RendererMode = TetrisRendererMode::Deferred;
  TetrisHandlingConfig        _HandlingConfig;
  TetrisInputState            _InputState[2];
  GameMenuInput               _MenuInput;

  TetrisPlayerAudioTracker    _AudioTrackers[2];

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
