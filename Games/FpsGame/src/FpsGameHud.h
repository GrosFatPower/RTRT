#ifndef _FpsGameHud_
#define _FpsGameHud_

#include "FpsGame.h"
#include "GameMenuUI.h"
#include "GamepadInput.h"
#include "RenderSettings.h"
#include "RenderStatsUI.h"

#include <string>

namespace RTRT
{

class Renderer;
class Scene;

// ----------------------------------------------------------------------------
// FpsGameHudContext
// Plain struct: inputs are filled by the app each frame, outputs ("_Request*")
// are consumed by the app after Draw.
// ----------------------------------------------------------------------------
struct FpsGameHudContext
{
  // Inputs
  FpsAppState             _AppState = FpsAppState::TitleScreen;
  bool                    _Paused = false;
  bool                    _EditorEnabled = false;
  bool                    _DevPanelsVisible = false;
  FpsGameSettings       * _GameSettings = nullptr;
  const FpsGameWorld    * _GameWorld = nullptr;
  Scene                 * _Scene = nullptr;
  Renderer              * _Renderer = nullptr;
  RenderSettings        * _RenderSettings = nullptr;
  const GameMenuInput   * _MenuInput = nullptr;
  GamepadInput          * _Gamepad = nullptr;
  float                 * _GamepadLookSensitivity = nullptr;
  std::string             _MapName;
  std::string             _MapPath;
  double                  _FrameRate = 60.;
  double                  _DeltaTime = 0.016;
  unsigned int            _NbRenderedFrames = 0;

  // Outputs
  bool                    _RequestStartGame = false;
  bool                    _RequestBrowseMap = false;
  std::string             _RequestMapPath;
  bool                    _RequestResume = false;
  bool                    _RequestRespawn = false;
  bool                    _RequestEditor = false;
  bool                    _RequestTitle = false;
  bool                    _RequestMapSelect = false;
  bool                    _RequestExit = false;
  bool                    _RequestReloadScene = false;
  bool                    _RequestToggleDevPanels = false;
  bool                    _WeaponDirty = false;
  int                     _RequestRendererMode = -1;
};

// ----------------------------------------------------------------------------
// FpsGameHud
// ----------------------------------------------------------------------------
class FpsGameHud
{
public:
  FpsGameHud();

  void Initialize();
  void Draw( FpsGameHudContext & ioContext );
  void DrawCrosshair();

  bool IsSettingsOpen() const { return _ShowInGameOptions; }
  void SetSettingsOpen( bool iOpen ) { _ShowInGameOptions = iOpen; }

  FpsMenuPage GetMenuPage() const { return _MenuPage; }
  void SetMenuPage( FpsMenuPage iPage );

protected:
  void DrawTitleScreenMenus( FpsGameHudContext & ioContext );
  void DrawMainMenuPage( FpsGameHudContext & ioContext );
  void DrawMapSelectPage( FpsGameHudContext & ioContext );
  void DrawOptionsPage( FpsGameHudContext & ioContext, bool & oBackRequested );
  void DrawGameplayHud( const FpsGameHudContext & iContext );
  void DrawInGamePauseModal( FpsGameHudContext & ioContext );

protected:
  GameUITheme         _Theme;
  GameUIFonts         _Fonts;

  FpsMenuPage         _MenuPage = FpsMenuPage::MainMenu;
  GameMenuNavState    _MainMenuNav;
  GameMenuNavState    _PauseMenuNav;

  int                 _SelectedMapIndex = 0;
  int                 _OptionsTabIndex = 0;
  bool                _ShowInGameOptions = false;

  RenderStatsUIState  _StatsUIState;
};

}

#endif /* _FpsGameHud_ */
