#ifndef _GameMenuUI_
#define _GameMenuUI_

#include "MathUtil.h"
#include "RenderStatsUI.h"

#include <string>
#include <vector>

struct ImFont;
struct GLFWwindow;

namespace RTRT
{

class AudioEngine;
class Renderer;
class Scene;
struct RenderSettings;

// ----------------------------------------------------------------------------
// GameUITheme
// ----------------------------------------------------------------------------
struct GameUITheme
{
  Vec4  _BackgroundDim   = Vec4(0.02f, 0.03f, 0.05f, 0.52f);
  Vec4  _PanelBg         = Vec4(0.06f, 0.08f, 0.12f, 0.90f);
  Vec4  _CardBg          = Vec4(0.09f, 0.12f, 0.18f, 0.88f);
  Vec4  _CardHoverBg     = Vec4(0.14f, 0.20f, 0.30f, 0.96f);
  Vec4  _AccentPrimary   = Vec4(0.00f, 0.90f, 0.95f, 1.00f);
  Vec4  _AccentSecondary = Vec4(0.70f, 0.30f, 1.00f, 1.00f);
  Vec4  _TextPrimary     = Vec4(0.96f, 0.97f, 1.00f, 1.00f);
  Vec4  _TextMuted       = Vec4(0.62f, 0.68f, 0.78f, 1.00f);
  Vec4  _DangerColor     = Vec4(1.00f, 0.32f, 0.36f, 1.00f);
  Vec4  _SuccessColor    = Vec4(0.25f, 0.95f, 0.55f, 1.00f);
  float _CornerRounding  = 8.0f;

  static GameUITheme CreateArcadeTheme();
  static GameUITheme CreateTacticalTheme();
};

// ----------------------------------------------------------------------------
// GameUIFonts
// ----------------------------------------------------------------------------
struct GameUIFonts
{
  ImFont * _TitleFont  = nullptr; // ~48px bold title
  ImFont * _HeaderFont = nullptr; // ~24px section / button header
  ImFont * _BodyFont   = nullptr; // ~17px body & descriptions
  ImFont * _MonoFont   = nullptr; // ~22px monospace digital readouts
};

// ----------------------------------------------------------------------------
// GameMenuInput
// ----------------------------------------------------------------------------
struct GameMenuInput
{
  bool _NavUp    = false;
  bool _NavDown  = false;
  bool _NavLeft  = false;
  bool _NavRight = false;
  bool _Confirm  = false;
  bool _Back     = false;
  bool _PrevTab  = false;
  bool _NextTab  = false;

  void Clear()
  {
    _NavUp    = false;
    _NavDown  = false;
    _NavLeft  = false;
    _NavRight = false;
    _Confirm  = false;
    _Back     = false;
    _PrevTab  = false;
    _NextTab  = false;
  }
};

// ----------------------------------------------------------------------------
// GameMenuNavState
// ----------------------------------------------------------------------------
struct GameMenuNavState
{
  int _SelectedIndex = 0;
  int _ActiveTab     = 0;

  void Reset()
  {
    _SelectedIndex = 0;
  }
};

// ----------------------------------------------------------------------------
// GameMenuItem
// ----------------------------------------------------------------------------
struct GameMenuItem
{
  std::string _Label;
  std::string _Description;
  std::string _RightBadge;
  bool        _Enabled  = true;
  bool        _IsDanger = false;
};

// ----------------------------------------------------------------------------
// GameSelectionCard
// ----------------------------------------------------------------------------
struct GameSelectionCard
{
  std::string _Title;
  std::string _Subtitle;
  std::string _Description;
  std::string _StatLabel;
  Vec4        _AccentColor = Vec4(0.0f, 0.9f, 0.95f, 1.0f);
};

// ----------------------------------------------------------------------------
// GameKeybindEntry
// ----------------------------------------------------------------------------
struct GameKeybindEntry
{
  std::string _Category;
  std::string _Action;
  std::string _PrimaryKey;
  std::string _SecondaryKey;
  std::string _GamepadButton;
};

// ----------------------------------------------------------------------------
// GameStatCard
// ----------------------------------------------------------------------------
struct GameStatCard
{
  std::string _Title;
  std::string _MainValue;
  std::string _SubValue1;
  std::string _SubValue2;
  Vec4        _AccentColor = Vec4(0.0f, 0.9f, 0.95f, 1.0f);
};

// ----------------------------------------------------------------------------
// GameMenuUI
// ----------------------------------------------------------------------------
namespace GameMenuUI
{

void InitializeFonts( GameUIFonts & oFonts );

void PushThemeStyle( const GameUITheme & iTheme );
void PopThemeStyle();

void DrawBackgroundOverlay( const GameUITheme & iTheme, float iAlphaScale = 1.0f );

void DrawTitleHeader( const char * iTitle,
                      const char * iSubtitle,
                      const GameUITheme & iTheme,
                      const GameUIFonts & iFonts,
                      float iTopY = 38.0f );

void DrawFooterBar( const char * iLeftStatus,
                    const char * iRightNavigationHints,
                    const GameUITheme & iTheme,
                    const GameUIFonts & iFonts );

bool BeginMenuPanel( const char * iWindowId,
                     const char * iPanelTitle,
                     const Vec2 & iCenterPos,
                     const Vec2 & iSize,
                     const GameUITheme & iTheme,
                     const GameUIFonts & iFonts );
void EndMenuPanel();

int DrawMenuButtonList( const std::vector<GameMenuItem> & iItems,
                        GameMenuNavState & ioNavState,
                        const GameMenuInput & iInput,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts,
                        float iButtonHeight = 54.0f );

bool DrawSelectionCards( const std::vector<GameSelectionCard> & iCards,
                         int & ioSelectedIndex,
                         const GameMenuInput & iInput,
                         const GameUITheme & iTheme,
                         const GameUIFonts & iFonts,
                         bool * oOutConfirmed = nullptr );

bool DrawTabBar( const std::vector<const char *> & iTabNames,
                 int & ioActiveTab,
                 const GameUITheme & iTheme,
                 const GameUIFonts & iFonts );

void DrawKeybindingTable( const std::vector<GameKeybindEntry> & iEntries,
                          const GameUITheme & iTheme,
                          const GameUIFonts & iFonts );

void DrawStatCardsGrid( const std::vector<GameStatCard> & iCards,
                        int iColumns,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts );

bool DrawActionButton( const char * iLabel,
                       const Vec2 & iSize,
                       bool iIsPrimary,
                       const GameUITheme & iTheme,
                       const GameUIFonts & iFonts );

void DrawSectionHeader( const char * iTitle,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts );

void DrawAudioSettings( AudioEngine & ioAudio,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts );

void DrawSharedRendererSettings( Scene & ioScene,
                                 Renderer * ioRenderer,
                                 RenderSettings & ioSettings,
                                 int & ioRendererModeIndex,
                                 const char * const * iRendererModeNames,
                                 int iRendererModeCount,
                                 bool & oReloadRenderer,
                                 RenderStatsUIState & ioStatsState,
                                 double iFrameRate,
                                 double iDeltaTime,
                                 unsigned int iNbRenderedFrames );

void SetWindowFullscreen( GLFWwindow * iWindow, bool iFullscreen, RenderSettings & ioSettings );
void ToggleWindowFullscreen( GLFWwindow * iWindow, RenderSettings & ioSettings );

} // namespace GameMenuUI

} // namespace RTRT

#endif /* _GameMenuUI_ */
