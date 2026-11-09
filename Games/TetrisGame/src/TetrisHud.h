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
  void SetAudioEngine( AudioEngine * iAudio ) { _Audio = iAudio; }

  bool IsSettingsOpen() const { return _ShowInGameOptions; }
  void ToggleSettings();

  TetrisMenuPage GetMenuPage() const { return _MenuPage; }
  void SetMenuPage( TetrisMenuPage iPage );

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
  GameUITheme        _Theme;
  GameUIFonts        _Fonts;

  TetrisMenuPage     _MenuPage          = TetrisMenuPage::MainMenu;
  GameMenuNavState   _MainMenuNav;
  GameMenuNavState   _PauseMenuNav;
  GameMenuNavState   _GameOverMenuNav;

  int                _SelectedModeIndex = 0;
  int                _OptionsTabIndex   = 0;
  bool               _ShowInGameOptions = false;
  GamepadInput *     _Gamepad           = nullptr;
  AudioEngine *      _Audio             = nullptr;

  RenderStatsUIState _StatsUIState;
};

} // namespace Tetris

#endif /* _TetrisHud_ */
