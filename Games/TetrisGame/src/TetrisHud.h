#ifndef _TetrisHud_
#define _TetrisHud_

#include "GameMenuUI.h"
#include "GamepadInput.h"
#include "RenderSettings.h"
#include "RenderStatsUI.h"
#include "TetrisGameLogic.h"
#include "TetrisSceneManager.h"
#include "TetrisTypes.h"

namespace RTRT
{
class AudioEngine;
class Renderer;
}

namespace Tetris
{
using namespace RTRT;

struct TetrisMultiplayerState
{
  int                     _NumPlayers      = 1;
  TetrisGameMode          _MpMode          = TetrisGameMode::Versus;
  TetrisMultiplayerPreset _Preset          = TetrisMultiplayerPreset::Auto;
  TetrisControlDevice     _PlayerDevices[2] = { TetrisControlDevice::KeyboardFull, TetrisControlDevice::None };
  int                     _TargetRoundWins = 2;
  int                     _RoundWins[2]    = { 0, 0 };
  int                     _LastRoundWinner = -1; // -1 = tie/none, 0 = P1, 1 = P2
  int                     _StartLevel      = 1;
  bool                    _RequestStart1P  = false;
  TetrisGameMode          _Requested1PMode = TetrisGameMode::Marathon;
  bool                    _RequestStart2P  = false;
  bool                    _RequestNextRound = false;
  bool                    _RequestRematch  = false;
};

class TetrisHud
{
public:
  TetrisHud();
  virtual ~TetrisHud();

  void Initialize();

  int Draw( TetrisAppState & ioAppState,
            TetrisGameLogic & ioLogic,
            TetrisSceneManager & ioSceneMgr,
            Scene & ioScene,
            Renderer * ioRenderer,
            RenderSettings & ioSettings,
            TetrisRendererMode & ioRendererMode,
            TetrisHandlingConfig & ioHandling,
            const GameMenuInput & iMenuInput,
            double iFrameRate,
            double iDeltaTime,
            unsigned int iNbRenderedFrames,
            bool & oReloadRenderer,
            bool & oRequestExit );

  void SetGamepadInput( GamepadInput * iGamepad ) { _Gamepad = iGamepad; }
  void SetGamepadInputs( GamepadInput * iGamepad1, GamepadInput * iGamepad2 ) { _Gamepad = iGamepad1; _Gamepad2 = iGamepad2; }
  void SetPlayerLogics( TetrisGameLogic * iLogics ) { _Logics = iLogics; }
  void SetMultiplayerState( TetrisMultiplayerState * iMpState ) { _MpState = iMpState; }
  void SetAudioEngine( AudioEngine * iAudio ) { _Audio = iAudio; }

  bool IsSettingsOpen() const { return _ShowInGameOptions; }
  void ToggleSettings();

  TetrisMenuPage GetMenuPage() const { return _MenuPage; }
  void SetMenuPage( TetrisMenuPage iPage );

  static const char * GetControlDeviceName( TetrisControlDevice iDevice );
  static const char * GetControlDeviceShortBadge( TetrisControlDevice iDevice );

protected:
  void DrawTitleScreenMenus( TetrisAppState & ioAppState,
                             TetrisGameLogic & ioLogic,
                             TetrisSceneManager & ioSceneMgr,
                             Scene & ioScene,
                             Renderer * ioRenderer,
                             RenderSettings & ioSettings,
                             TetrisRendererMode & ioRendererMode,
                             TetrisHandlingConfig & ioHandling,
                             const GameMenuInput & iMenuInput,
                             double iFrameRate,
                             double iDeltaTime,
                             unsigned int iNbRenderedFrames,
                             bool & oReloadRenderer,
                             bool & oRequestExit );

  void DrawMainMenuPage( TetrisAppState & ioAppState,
                         TetrisGameLogic & ioLogic,
                         TetrisRendererMode iRendererMode,
                         const GameMenuInput & iMenuInput,
                         bool & oRequestExit );

  void DrawModeSelectPage( TetrisAppState & ioAppState,
                           TetrisGameLogic & ioLogic,
                           const GameMenuInput & iMenuInput );

  void DrawMultiplayerLobbyPage( TetrisAppState & ioAppState,
                                 const GameMenuInput & iMenuInput );

  void DrawHighScoresPage( TetrisGameLogic & ioLogic,
                           const GameMenuInput & iMenuInput );

  void DrawOptionsPage( TetrisGameLogic & ioLogic,
                        TetrisSceneManager & ioSceneMgr,
                        Scene & ioScene,
                        Renderer * ioRenderer,
                        RenderSettings & ioSettings,
                        TetrisRendererMode & ioRendererMode,
                        TetrisHandlingConfig & ioHandling,
                        const GameMenuInput & iMenuInput,
                        double iFrameRate,
                        double iDeltaTime,
                        unsigned int iNbRenderedFrames,
                        bool & oReloadRenderer,
                        bool & oBackRequested );

  void DrawGameplayOverlay( const TetrisGameLogic & iLogic, Scene & ioScene );
  void DrawMultiplayerGameplayOverlay( Scene & ioScene );
  void DrawActionBanner( const TetrisGameLogic & iLogic );

  void DrawInGamePauseOrEndModal( TetrisAppState & ioAppState,
                                  TetrisGameLogic & ioLogic,
                                  TetrisSceneManager & ioSceneMgr,
                                  Scene & ioScene,
                                  Renderer * ioRenderer,
                                  RenderSettings & ioSettings,
                                  TetrisRendererMode & ioRendererMode,
                                  TetrisHandlingConfig & ioHandling,
                                  const GameMenuInput & iMenuInput,
                                  double iFrameRate,
                                  double iDeltaTime,
                                  unsigned int iNbRenderedFrames,
                                  bool & oReloadRenderer );

protected:
  GameUITheme              _Theme;
  GameUIFonts              _Fonts;

  TetrisMenuPage           _MenuPage          = TetrisMenuPage::MainMenu;
  GameMenuNavState         _MainMenuNav;
  GameMenuNavState         _PauseMenuNav;
  GameMenuNavState         _GameOverMenuNav;

  int                      _SelectedModeIndex   = 0;
  int                      _SelectedMpModeIndex = 0;
  int                      _OptionsTabIndex     = 0;
  bool                     _ShowInGameOptions   = false;
  GamepadInput *           _Gamepad             = nullptr;
  GamepadInput *           _Gamepad2            = nullptr;
  TetrisGameLogic *        _Logics              = nullptr;
  TetrisMultiplayerState * _MpState             = nullptr;
  AudioEngine *            _Audio               = nullptr;

  RenderStatsUIState       _StatsUIState;
};

} // namespace Tetris

#endif /* _TetrisHud_ */
