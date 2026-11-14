#include "FpsGameHud.h"

#include "AudioEngine.h"
#include "Localization.h"
#include "Renderer.h"
#include "Scene.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace FPS
{
using namespace RTRT;

namespace
{

static const char * g_MapPaths[2] = { "FPSMaps/ccity.fpsmap", "FPSMaps/default.fpsmap" };

// ----------------------------------------------------------------------------
// GetRendererModeLabel
// ----------------------------------------------------------------------------
const char * GetRendererModeLabel( FpsRendererMode iMode )
{
  switch ( iMode )
  {
    case FpsRendererMode::Software: return Loc::Tr(LocId::Fps_Renderer_Software);
    default:                        return Loc::Tr(LocId::Fps_Renderer_Deferred);
  }
}

// ----------------------------------------------------------------------------
// PathEndsWith
// ----------------------------------------------------------------------------
bool PathEndsWith( const std::string & iPath, const char * iName )
{
  std::string path = iPath;
  std::replace(path.begin(), path.end(), '\\', '/');
  const std::string name(iName);
  return ( path.size() >= name.size() ) && ( 0 == path.compare(path.size() - name.size(), name.size(), name) );
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
FpsGameHud::FpsGameHud()
: _Theme(GameUITheme::CreateTacticalTheme())
{
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
void FpsGameHud::Initialize()
{
  _Theme = GameUITheme::CreateTacticalTheme();
  GameMenuUI::InitializeFonts(_Fonts);
}

// ----------------------------------------------------------------------------
// SetMenuPage
// ----------------------------------------------------------------------------
void FpsGameHud::SetMenuPage( FpsMenuPage iPage )
{
  _MenuPage = iPage;
  _MainMenuNav.Reset();
}

// ----------------------------------------------------------------------------
// DrawMainMenuPage
// ----------------------------------------------------------------------------
void FpsGameHud::DrawMainMenuPage( FpsGameHudContext & ioContext )
{
  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.57f);

  if ( GameMenuUI::BeginMenuPanel("##FpsMainMenu", Loc::Tr(LocId::Fps_MainMenuTitle), center, Vec2(520.0f, 0.0f), _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { Loc::Tr(LocId::Fps_Menu_Deploy),     Loc::Tr(LocId::Fps_Menu_DeployDesc),     Loc::Tr(LocId::Fps_Menu_DeployBadge),     true, false },
      { Loc::Tr(LocId::Fps_Menu_QuickStart), Loc::Tr(LocId::Fps_Menu_QuickStartDesc), Loc::Tr(LocId::Fps_Menu_QuickStartBadge), true, false },
      { Loc::Tr(LocId::Fps_Menu_Editor),     Loc::Tr(LocId::Fps_Menu_EditorDesc),     "F3",                                     true, false },
      { Loc::Tr(LocId::Fps_Menu_Options),    Loc::Tr(LocId::Fps_Menu_OptionsDesc),    "F1",                                     true, false },
      { Loc::Tr(LocId::Fps_Menu_Exit),       Loc::Tr(LocId::Fps_Menu_ExitDesc),       "",                                       true, true  }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _MainMenuNav, *ioContext._MenuInput, _Theme, _Fonts, 56.0f);
    if ( ( activated >= 0 ) && ioContext._Audio && !ioContext._MenuInput->_Confirm )
      ioContext._Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

    if ( 0 == activated )
      SetMenuPage(FpsMenuPage::MapSelect);
    else if ( 1 == activated )
      ioContext._RequestStartGame = true;
    else if ( 2 == activated )
      ioContext._RequestEditor = true;
    else if ( 3 == activated )
      SetMenuPage(FpsMenuPage::Options);
    else if ( 4 == activated )
      ioContext._RequestExit = true;
  }
  GameMenuUI::EndMenuPanel();

  const std::string leftStatus = std::string(Loc::Tr(LocId::Option_ActiveRenderer)) + ": " + GetRendererModeLabel(ioContext._GameSettings->_RendererMode)
                               + "    " + Loc::Tr(LocId::Fps_Map_Title) + ": " + ioContext._MapName;
  GameMenuUI::DrawFooterBar(leftStatus.c_str(), Loc::Tr(LocId::Fps_Footer_Main), _Theme, _Fonts);
}

// ----------------------------------------------------------------------------
// DrawMapSelectPage
// ----------------------------------------------------------------------------
void FpsGameHud::DrawMapSelectPage( FpsGameHudContext & ioContext )
{
  if ( ioContext._MenuInput->_Back )
  {
    SetMenuPage(FpsMenuPage::MainMenu);
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.56f);

  if ( GameMenuUI::BeginMenuPanel("##FpsMapSelect", Loc::Tr(LocId::Fps_Map_Title), center, Vec2(820.0f, 0.0f), _Theme, _Fonts) )
  {
    const std::vector<GameSelectionCard> cards =
    {
      {
        Loc::Tr(LocId::Fps_Map_CityTitle),
        "ccity.fpsmap",
        Loc::Tr(LocId::Fps_Map_CityDesc),
        PathEndsWith(ioContext._MapPath, "ccity.fpsmap") ? Loc::Tr(LocId::Fps_Map_Active) : Loc::Tr(LocId::Fps_Map_Available),
        Vec4(0.96f, 0.72f, 0.18f, 1.0f)
      },
      {
        Loc::Tr(LocId::Fps_Map_DefaultTitle),
        "default.fpsmap",
        Loc::Tr(LocId::Fps_Map_DefaultDesc),
        PathEndsWith(ioContext._MapPath, "default.fpsmap") ? Loc::Tr(LocId::Fps_Map_Active) : Loc::Tr(LocId::Fps_Map_Available),
        Vec4(0.90f, 0.25f, 0.22f, 1.0f)
      },
      {
        Loc::Tr(LocId::Fps_Map_CustomTitle),
        Loc::Tr(LocId::Fps_Map_CustomSubtitle),
        Loc::Tr(LocId::Fps_Map_CustomDesc),
        Loc::Tr(LocId::Fps_Map_FileDialog),
        Vec4(0.25f, 0.95f, 0.55f, 1.0f)
      }
    };

    bool cardConfirmed = false;
    if ( GameMenuUI::DrawSelectionCards(cards, _SelectedMapIndex, *ioContext._MenuInput, _Theme, _Fonts, &cardConfirmed) )
    {
      if ( ioContext._Audio && !ioContext._MenuInput->_NavLeft && !ioContext._MenuInput->_NavRight )
        ioContext._Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Checkbox(Loc::Tr(LocId::Fps_Map_FreeLook), &ioContext._GameSettings->_FreeLook);
    ImGui::SameLine(0.0f, 32.0f);
    if ( ImGui::Checkbox(Loc::Tr(LocId::Fps_Map_ShowWeapon), &ioContext._GameSettings->_ShowViewWeapon) )
      ioContext._WeaponDirty = true;

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    const float availW = ImGui::GetContentRegionAvail().x;
    const float btnW = ( availW - 16.0f ) * 0.5f;

    if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Map_Back), Vec2(btnW, 46.0f), false, _Theme, _Fonts) )
    {
      if ( ioContext._Audio && !ioContext._MenuInput->_Back )
        ioContext._Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      SetMenuPage(FpsMenuPage::MainMenu);
    }

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Map_Deploy), Vec2(btnW, 46.0f), true, _Theme, _Fonts) || cardConfirmed )
    {
      if ( ioContext._Audio && !ioContext._MenuInput->_Confirm )
        ioContext._Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

      if ( _SelectedMapIndex < 2 )
      {
        ioContext._RequestMapPath = g_MapPaths[_SelectedMapIndex];
        ioContext._RequestStartGame = true;
      }
      else
      {
        ioContext._RequestBrowseMap = true;
        ioContext._RequestStartGame = true;
      }
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(Loc::Tr(LocId::Fps_Footer_MapSelectLeft), Loc::Tr(LocId::Fps_Footer_MapSelect), _Theme, _Fonts);
}

// ----------------------------------------------------------------------------
// DrawOptionsPage
// ----------------------------------------------------------------------------
void FpsGameHud::DrawOptionsPage( FpsGameHudContext & ioContext, bool & oBackRequested )
{
  oBackRequested = false;
  const GameMenuInput & menuInput = *ioContext._MenuInput;
  if ( menuInput._Back && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) )
  {
    oBackRequested = true;
    return;
  }

  if ( menuInput._PrevTab )
    _OptionsTabIndex = ( _OptionsTabIndex + 3 ) % 4;
  if ( menuInput._NextTab )
    _OptionsTabIndex = ( _OptionsTabIndex + 1 ) % 4;

  FpsGameSettings & game = *ioContext._GameSettings;

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.54f);

  if ( GameMenuUI::BeginMenuPanel("##FpsOptionsPanel", Loc::Tr(LocId::Fps_Opt_Title), center, Vec2(780.0f, 540.0f), _Theme, _Fonts) )
  {
    const std::vector<const char *> tabs =
    {
      Loc::Tr(LocId::Tab_Gameplay),
      Loc::Tr(LocId::Tab_Graphics),
      Loc::Tr(LocId::Tab_Audio),
      Loc::Tr(LocId::Tab_Controls)
    };
    if ( GameMenuUI::DrawTabBar(tabs, _OptionsTabIndex, _Theme, _Fonts) )
    {
      if ( ioContext._Audio && !ioContext._MenuInput->_PrevTab && !ioContext._MenuInput->_NextTab )
        ioContext._Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
    }

    ImGui::BeginChild("##FpsOptionsTabContent", ImVec2(0.0f, -56.0f), false);

    if ( 0 == _OptionsTabIndex )
    {
      int currentLang = static_cast<int>(Loc::GetLanguage());
      const char * langNames[] = { "English", "Français" };
      if ( ImGui::Combo(Loc::Tr(LocId::Option_Language), &currentLang, langNames, IM_ARRAYSIZE(langNames)) )
      {
        Loc::SetLanguage(static_cast<Language>(currentLang));
      }
      ImGui::Spacing();

      ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_Movement));
      ImGui::Separator();
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_MoveSpeed), &game._MoveSpeed, 1.f, 100.f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_SprintSpeed), &game._SprintSpeed, 1.f, 16.f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_MouseSensitivity), &game._MouseSensitivity, 0.01f, 0.25f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_FOV), &game._CameraFOV, 30.f, 140.f, "%.0f deg");
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_Gravity), &game._Gravity, 1.f, 35.f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_JumpSpeed), &game._JumpSpeed, 1.f, 12.f);
      ImGui::Checkbox(Loc::Tr(LocId::Fps_Opt_FreeLook), &game._FreeLook);

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_HeadBobHeader));
      ImGui::Separator();
      ImGui::Checkbox(Loc::Tr(LocId::Fps_Opt_HeadBobEnabled), &game._HeadBob._Enabled);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_BobAmplitude), &game._HeadBob._Amplitude, 0.f, 0.08f, "%.3f");
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_BobFrequency), &game._HeadBob._Frequency, 0.5f, 4.f, "%.2f");
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_BobSway), &game._HeadBob._Sway, 0.f, 0.05f, "%.3f");

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_ProjectilesHeader));
      ImGui::Separator();
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_ProjSpeed), &game._ProjectileSpeed, 1.f, 40.f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_ProjGravity), &game._ProjectileGravity, 0.f, 30.f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_ProjBounciness), &game._ProjectileBounciness, 0.f, 1.f);
      ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_ProjCooldown), &game._ProjectileCooldown, 0.02f, 1.f);
      int maxProjectiles = game._MaxProjectiles;
      if ( ImGui::SliderInt(Loc::Tr(LocId::Fps_Opt_MaxProjectiles), &maxProjectiles, 1, 128) )
      {
        game._MaxProjectiles = std::max(1, maxProjectiles);
        ioContext._RequestReloadScene = true;
      }
      if ( ImGui::Checkbox(Loc::Tr(LocId::Fps_Opt_ShowViewWeapon), &game._ShowViewWeapon) )
        ioContext._WeaponDirty = true;
    }
    else if ( 1 == _OptionsTabIndex )
    {
      ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_EngineHeader));
      ImGui::Separator();

      const char * rendererNames[] =
      {
        Loc::Tr(LocId::Fps_Renderer_Deferred),
        Loc::Tr(LocId::Fps_Renderer_Software)
      };
      int rendererIdx = static_cast<int>(game._RendererMode);
      bool reloadRenderer = false;
      if ( ioContext._Scene && ioContext._RenderSettings )
      {
        GameMenuUI::DrawSharedRendererSettings(*ioContext._Scene,
                                               ioContext._Renderer,
                                               *ioContext._RenderSettings,
                                               rendererIdx,
                                               rendererNames,
                                               IM_ARRAYSIZE(rendererNames),
                                               reloadRenderer,
                                               _StatsUIState,
                                               ioContext._FrameRate,
                                               ioContext._DeltaTime,
                                               ioContext._NbRenderedFrames);
      }
      if ( rendererIdx != static_cast<int>(game._RendererMode) )
        ioContext._RequestRendererMode = rendererIdx;

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_DevTools));
      ImGui::Separator();
      bool devPanels = ioContext._DevPanelsVisible;
      if ( ImGui::Checkbox(Loc::Tr(LocId::Fps_Opt_DevPanels), &devPanels) )
        ioContext._RequestToggleDevPanels = true;
    }
    else if ( 2 == _OptionsTabIndex )
    {
      if ( ioContext._Audio )
      {
        GameMenuUI::DrawAudioSettings(*ioContext._Audio, _Theme, _Fonts);
      }
      else
      {
        ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_AudioNotAvail));
      }
    }
    else
    {
      if ( ioContext._Gamepad )
      {
        ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Opt_GamepadHeader));
        ImGui::Separator();

        if ( ioContext._Gamepad->IsConnected() )
        {
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), Loc::Tr(LocId::Fps_Opt_GamepadConnected),
                             ioContext._Gamepad->GetName().c_str(), ioContext._Gamepad->GetJoystickId() + 1);
        }
        else
          ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "%s", Loc::Tr(LocId::Fps_Opt_GamepadNotFound));

        float deadzone = ioContext._Gamepad->GetStickDeadzone();
        ImGui::SetNextItemWidth(220.0f);
        if ( ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_Deadzone), &deadzone, 0.05f, 0.45f, "%.2f") )
          ioContext._Gamepad->SetStickDeadzone(deadzone);

        if ( ioContext._GamepadLookSensitivity )
        {
          ImGui::SetNextItemWidth(220.0f);
          ImGui::SliderFloat(Loc::Tr(LocId::Fps_Opt_LookSpeed), ioContext._GamepadLookSensitivity, 40.f, 360.f, "%.0f");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
      }

      const std::vector<GameKeybindEntry> bindings =
      {
        { Loc::Tr(LocId::Fps_Key_CategoryMovement), Loc::Tr(LocId::Fps_Key_Move),           "W / S",       "UP / DOWN",  "L-STICK UP / DOWN"  },
        { Loc::Tr(LocId::Fps_Key_CategoryMovement), Loc::Tr(LocId::Fps_Key_Strafe),         "A / D",       "",           "L-STICK LEFT / RIGHT" },
        { Loc::Tr(LocId::Fps_Key_CategoryMovement), Loc::Tr(LocId::Fps_Key_Look),           "MOUSE",       "",           "R-STICK"            },
        { Loc::Tr(LocId::Fps_Key_CategoryMovement), Loc::Tr(LocId::Fps_Key_Jump),           "SPACE",       "",           "A BUTTON"           },
        { Loc::Tr(LocId::Fps_Key_CategoryMovement), Loc::Tr(LocId::Fps_Key_Sprint),         "LEFT SHIFT",  "",           "L3 / LB"            },
        { Loc::Tr(LocId::Fps_Key_CategoryCombat),   Loc::Tr(LocId::Fps_Key_Fire),           "LEFT CLICK",  "",           "RT"                 },
        { Loc::Tr(LocId::Fps_Key_CategoryCombat),   Loc::Tr(LocId::Fps_Key_Respawn),        "R",           "",           "Y BUTTON"           },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_Pause),          "ESCAPE",      "P",          "START"              },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_Options),        "F1",          "",           ""                   },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_ToggleFreeLook),  "F2",          "",           ""                   },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_MapEditor),      "F3",          "",           ""                   },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_DevPanels),      "F4",          "",           ""                   },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_SwitchRenderer), "J / K",       "",           ""                   },
        { Loc::Tr(LocId::Fps_Key_CategorySystem),   Loc::Tr(LocId::Fps_Key_PhotoMode),      "L",           "F12",        ""                   }
      };
      GameMenuUI::DrawKeybindingTable(bindings, _Theme, _Fonts);
    }

    ImGui::EndChild();

    ImGui::Separator();
    if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Opt_BackBtn), Vec2(220.0f, 42.0f), true, _Theme, _Fonts) )
    {
      if ( ioContext._Audio && !ioContext._MenuInput->_Back )
        ioContext._Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      oBackRequested = true;
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(Loc::Tr(LocId::Option_Title), Loc::Tr(LocId::Fps_Footer_Options), _Theme, _Fonts);
}

// ----------------------------------------------------------------------------
// DrawTitleScreenMenus
// ----------------------------------------------------------------------------
void FpsGameHud::DrawTitleScreenMenus( FpsGameHudContext & ioContext )
{
  GameMenuUI::DrawBackgroundOverlay(_Theme, 0.78f);
  GameMenuUI::DrawTitleHeader(Loc::Tr(LocId::Fps_Title), Loc::Tr(LocId::Fps_Subtitle), _Theme, _Fonts, 28.0f);

  if ( FpsMenuPage::MainMenu == _MenuPage )
    DrawMainMenuPage(ioContext);
  else if ( FpsMenuPage::MapSelect == _MenuPage )
    DrawMapSelectPage(ioContext);
  else
  {
    bool backRequested = false;
    DrawOptionsPage(ioContext, backRequested);
    if ( backRequested )
      SetMenuPage(FpsMenuPage::MainMenu);
  }
}

// ----------------------------------------------------------------------------
// DrawInGamePauseModal
// ----------------------------------------------------------------------------
void FpsGameHud::DrawInGamePauseModal( FpsGameHudContext & ioContext )
{
  GameMenuUI::DrawBackgroundOverlay(_Theme, 0.85f);

  if ( _ShowInGameOptions )
  {
    bool backRequested = false;
    DrawOptionsPage(ioContext, backRequested);
    if ( backRequested )
      _ShowInGameOptions = false;
    return;
  }

  if ( ioContext._MenuInput->_Back )
  {
    ioContext._RequestResume = true;
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.52f);

  if ( GameMenuUI::BeginMenuPanel("##FpsPauseModal", Loc::Tr(LocId::Fps_Pause_Title), center, Vec2(500.0f, 0.0f), _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { Loc::Tr(LocId::Fps_Pause_Resume),      Loc::Tr(LocId::Fps_Pause_ResumeDesc),      "ESC / P", true, false },
      { Loc::Tr(LocId::Fps_Pause_PhotoMode),   Loc::Tr(LocId::Fps_Pause_PhotoModeDesc),   "L / F12", true, false },
      { Loc::Tr(LocId::Fps_Pause_Respawn),     Loc::Tr(LocId::Fps_Pause_RespawnDesc),     "R",       true, false },
      { Loc::Tr(LocId::Fps_Pause_SwitchMap),   Loc::Tr(LocId::Fps_Pause_SwitchMapDesc),   "MAPS",    true, false },
      { Loc::Tr(LocId::Fps_Pause_EnterEditor), Loc::Tr(LocId::Fps_Pause_EnterEditorDesc), "F3",      true, false },
      { Loc::Tr(LocId::Fps_Pause_Options),     Loc::Tr(LocId::Fps_Pause_OptionsDesc),     "F1",      true, false },
      { Loc::Tr(LocId::Fps_Pause_MainMenu),    Loc::Tr(LocId::Fps_Pause_MainMenuDesc),    "",        true, true  }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _PauseMenuNav, *ioContext._MenuInput, _Theme, _Fonts, 56.0f);
    if ( ( activated >= 0 ) && ioContext._Audio && !ioContext._MenuInput->_Confirm )
      ioContext._Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

    if ( 0 == activated )
      ioContext._RequestResume = true;
    else if ( 1 == activated )
      ioContext._RequestPhotoMode = true;
    else if ( 2 == activated )
      ioContext._RequestRespawn = true;
    else if ( 3 == activated )
      ioContext._RequestMapSelect = true;
    else if ( 4 == activated )
      ioContext._RequestEditor = true;
    else if ( 5 == activated )
      _ShowInGameOptions = true;
    else if ( 6 == activated )
      ioContext._RequestTitle = true;
  }
  GameMenuUI::EndMenuPanel();
}

// ----------------------------------------------------------------------------
// Draw
// ----------------------------------------------------------------------------
void FpsGameHud::Draw( FpsGameHudContext & ioContext )
{
  if ( ioContext._Audio && ioContext._MenuInput )
  {
    const bool inMenu = ( FpsAppState::TitleScreen == ioContext._AppState ) ||
                        ioContext._Paused || _ShowInGameOptions;
    if ( inMenu )
    {
      const GameMenuInput & menuInput = *ioContext._MenuInput;
      if ( menuInput._NavUp || menuInput._NavDown || menuInput._NavLeft || menuInput._NavRight || menuInput._PrevTab || menuInput._NextTab )
      {
        ioContext._Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
      }
      else if ( menuInput._Confirm )
      {
        ioContext._Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
      }
      else if ( menuInput._Back )
      {
        ioContext._Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      }
    }
  }

  if ( FpsAppState::TitleScreen == ioContext._AppState )
  {
    DrawTitleScreenMenus(ioContext);
    return;
  }

  if ( ioContext._EditorEnabled )
  {
    DrawCrosshair();
    return;
  }

  if ( FpsPhotoModeState::Inactive != ioContext._PhotoModeState )
  {
    DrawPhotoModeHud(ioContext);
    return;
  }

  DrawGameplayHud(ioContext);
  if ( ioContext._Paused )
    DrawInGamePauseModal(ioContext);
}

// ----------------------------------------------------------------------------
// DrawPhotoModeHud
// ----------------------------------------------------------------------------
void FpsGameHud::DrawPhotoModeHud( FpsGameHudContext & ioContext )
{
  const ImGuiIO & io = ImGui::GetIO();

  if ( FpsPhotoModeState::Setup == ioContext._PhotoModeState )
  {
    const Vec2 panelSize(380.0f, 0.0f);
    const Vec2 panelCenter(io.DisplaySize.x - panelSize.x * 0.5f - 28.0f, io.DisplaySize.y * 0.48f);

    if ( GameMenuUI::BeginMenuPanel("##FpsPhotoSetup", Loc::Tr(LocId::Fps_Photo_SetupTitle), panelCenter, panelSize, _Theme, _Fonts) )
    {
      ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Photo_CameraHeader));
      ImGui::Separator();

      if ( ioContext._GameSettings )
      {
        if ( ImGui::SliderFloat(Loc::Tr(LocId::Fps_Photo_FOV), &ioContext._GameSettings->_CameraFOV, 30.f, 140.f, "%.0f deg") )
          ioContext._CameraDirty = true;

        if ( ImGui::Checkbox(Loc::Tr(LocId::Fps_Photo_ShowWeapon), &ioContext._GameSettings->_ShowViewWeapon) )
          ioContext._WeaponDirty = true;

        ImGui::SameLine(0.0f, 16.0f);
        if ( ImGui::SmallButton(Loc::Tr(LocId::Fps_Photo_ResetCam)) )
          ioContext._RequestResetPhotoCamera = true;
      }

      if ( ioContext._RenderSettings )
      {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Photo_ToneMappingHeader));
        ImGui::Separator();

        if ( ImGui::Checkbox(Loc::Tr(LocId::Fps_Photo_ToneMapping), &ioContext._RenderSettings->_ToneMapping) )
          ioContext._RenderSettingsDirty = true;
        if ( ioContext._RenderSettings->_ToneMapping )
        {
          if ( ImGui::SliderFloat(Loc::Tr(LocId::Fps_Photo_Exposure), &ioContext._RenderSettings->_Exposure, 0.1f, 5.f, "%.2f") )
            ioContext._RenderSettingsDirty = true;
          if ( ImGui::SliderFloat(Loc::Tr(LocId::Fps_Photo_Gamma), &ioContext._RenderSettings->_Gamma, 0.5f, 3.f, "%.2f") )
            ioContext._RenderSettingsDirty = true;
        }
      }

      if ( ioContext._PhotoSettings )
      {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextDisabled("%s", Loc::Tr(LocId::Fps_Photo_PtHeader));
        ImGui::Separator();

        ImGui::SliderInt(Loc::Tr(LocId::Fps_Photo_Passes), &ioContext._PhotoSettings->_TargetPasses, 8, 512);
        ImGui::SliderInt(Loc::Tr(LocId::Fps_Photo_SPP), &ioContext._PhotoSettings->_SamplesPerPixel, 1, 8);
        ImGui::SliderInt(Loc::Tr(LocId::Fps_Photo_Bounces), &ioContext._PhotoSettings->_Bounces, 1, 8);
        ImGui::Checkbox(Loc::Tr(LocId::Fps_Photo_Denoise), &ioContext._PhotoSettings->_Denoise);

        if ( ioContext._PhotoSettings->_Denoise )
        {
#if defined(RTRT_HAS_OIDN)
          static const char * DENOISING_METHODS[] = { "Bilateral", "Wavelet", "Edge-aware (A-Trous)", "OIDN (AI)" };
          const int methodCount = 4;
#else
          static const char * DENOISING_METHODS[] = { "Bilateral", "Wavelet", "Edge-aware (A-Trous)" };
          const int methodCount = 3;
#endif
          int denoisingMethod = static_cast<int>(ioContext._PhotoSettings->_DenoisingMethod);
          if ( ImGui::Combo(Loc::Tr(LocId::Fps_Photo_DenoiserCombo), &denoisingMethod, DENOISING_METHODS, methodCount) )
            ioContext._PhotoSettings->_DenoisingMethod = static_cast<unsigned int>(denoisingMethod);
        }
      }

      ImGui::Dummy(ImVec2(0.0f, 8.0f));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 4.0f));

      const float availW = ImGui::GetContentRegionAvail().x;
      if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Photo_CaptureBtn), Vec2(availW, 44.0f), true, _Theme, _Fonts) )
        ioContext._RequestCapturePhoto = true;

      if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Photo_ExitBtn), Vec2(availW, 40.0f), false, _Theme, _Fonts) )
        ioContext._RequestExitPhotoPreview = true;
    }
    GameMenuUI::EndMenuPanel();

    GameMenuUI::DrawFooterBar(
      Loc::Tr(LocId::Fps_Photo_FooterPreviewTitle),
      Loc::Tr(LocId::Fps_Photo_FooterSetup),
      _Theme,
      _Fonts );
  }
  else if ( FpsPhotoModeState::Capturing == ioContext._PhotoModeState )
  {
    const Vec2 panelSize(460.0f, 0.0f);
    const Vec2 panelCenter(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 120.0f);

    if ( GameMenuUI::BeginMenuPanel("##FpsPhotoCapture", Loc::Tr(LocId::Fps_Photo_CapturingTitle), panelCenter, panelSize, _Theme, _Fonts) )
    {
      const int target = std::max(1, ioContext._PhotoTargetPasses);
      const int current = MathUtil::Clamp(ioContext._PhotoCurrentPass, 0, target);
      const float progress = static_cast<float>(current) / static_cast<float>(target);

      char overlay[64];
      std::snprintf(overlay, sizeof(overlay), Loc::Tr(LocId::Fps_Photo_Accumulating), current, target, progress * 100.0f);
      ImGui::ProgressBar(progress, ImVec2(-1.0f, 26.0f), overlay);

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      const float availW = ImGui::GetContentRegionAvail().x;
      if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Photo_CancelBtn), Vec2(availW, 40.0f), false, _Theme, _Fonts) )
        ioContext._RequestCancelPhotoCapture = true;
    }
    GameMenuUI::EndMenuPanel();
  }
  else if ( FpsPhotoModeState::Preview == ioContext._PhotoModeState )
  {
    const Vec2 panelSize(720.0f, 0.0f);
    const Vec2 panelCenter(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 110.0f);

    if ( GameMenuUI::BeginMenuPanel("##FpsPhotoPreview", Loc::Tr(LocId::Fps_Photo_CapturedTitle), panelCenter, panelSize, _Theme, _Fonts) )
    {
      if ( !ioContext._PhotoSavedPath.empty() )
      {
        ImGui::TextColored(ImVec4(0.30f, 0.88f, 0.45f, 1.0f), "%s", Loc::Tr(LocId::Fps_Photo_SavedTo));
        ImGui::SameLine();
        ImGui::TextWrapped("%s", ioContext._PhotoSavedPath.c_str());
      }
      else
      {
        ImGui::TextColored(ImVec4(0.95f, 0.24f, 0.24f, 1.0f), "%s", Loc::Tr(LocId::Fps_Photo_SaveFailed));
      }

      ImGui::Dummy(ImVec2(0.0f, 8.0f));

      const float availW = ImGui::GetContentRegionAvail().x;
      const float btnW = ( availW - 14.0f ) * 0.5f;

      if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Photo_ReframeBtn), Vec2(btnW, 42.0f), false, _Theme, _Fonts) )
        ioContext._RequestRetakePhoto = true;

      ImGui::SameLine(0.0f, 14.0f);

      if ( GameMenuUI::DrawActionButton(Loc::Tr(LocId::Fps_Photo_ExitPreviewBtn), Vec2(btnW, 42.0f), true, _Theme, _Fonts) )
        ioContext._RequestExitPhotoPreview = true;
    }
    GameMenuUI::EndMenuPanel();
  }
}

// ----------------------------------------------------------------------------
// DrawGameplayHud
// ----------------------------------------------------------------------------
void FpsGameHud::DrawGameplayHud( const FpsGameHudContext & iContext )
{
  const ImGuiIO & io = ImGui::GetIO();
  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  const FpsPlayer & player = iContext._GameWorld->GetPlayer();
  const FpsGameSettings & game = *iContext._GameSettings;

  const float margin = 24.f;
  const float barWidth = 180.f;
  const float barHeight = 16.f;
  const float lineHeight = 26.f;
  const ImVec2 origin(margin, io.DisplaySize.y - margin - lineHeight * 3.f);
  const ImU32 bgColor = IM_COL32(16, 16, 18, 210);
  const ImU32 textColor = IM_COL32(245, 245, 245, 255);

  const int activeSlot = MathUtil::Clamp(iContext._GameWorld->GetActiveWeaponSlot(), 0, (int)FpsWeaponId::Count - 1);
  const FpsWeaponSlotConfig & slotCfg = game._WeaponSlots[activeSlot];
  const char * ammoLabel = ( FpsWeaponFireMode::Hitscan == slotCfg._FireMode ) ? "AMMO" : Loc::Tr(LocId::Fps_Hud_Projectiles);

  const char * slotShortNames[(int)FpsWeaponId::Count] = { "[1] LAUNCHER", "[2] MARK 23", "[3] CARBINE" };
  float slotX = origin.x;
  const float slotY = origin.y - 44.f;
  for ( int s = 0; s < (int)FpsWeaponId::Count; ++s )
  {
    const bool active = ( s == activeSlot );
    const ImVec2 labelSize = ImGui::CalcTextSize(slotShortNames[s]);
    const float pillW = labelSize.x + 14.f;
    const float pillH = 20.f;
    const ImVec2 pMin(slotX, slotY);
    const ImVec2 pMax(slotX + pillW, slotY + pillH);
    drawList -> AddRectFilled(pMin, pMax, active ? IM_COL32(48, 42, 22, 230) : IM_COL32(16, 16, 18, 180), 3.f);
    drawList -> AddRect(pMin, pMax, active ? IM_COL32(235, 190, 48, 255) : IM_COL32(255, 255, 255, 70), 3.f);
    drawList -> AddText(ImVec2(slotX + 7.f, slotY + 3.f), active ? IM_COL32(255, 232, 130, 255) : IM_COL32(200, 200, 205, 180), slotShortNames[s]);
    slotX += pillW + 6.f;
  }

  std::string weaponStatus = slotCfg._Name ? slotCfg._Name : "";
  if ( iContext._GameWorld->IsWeaponReloading() )
    weaponStatus += "  |  RELOADING...";
  else if ( FpsWeaponFireMode::Hitscan == slotCfg._FireMode )
    weaponStatus += slotCfg._Automatic ? "  |  HITSCAN AUTO" : "  |  HITSCAN SEMI";
  else
    weaponStatus += "  |  PROJECTILE";
  drawList -> AddText(ImVec2(origin.x, origin.y - 20.f),
                      iContext._GameWorld->IsWeaponReloading() ? IM_COL32(255, 195, 72, 255) : IM_COL32(235, 235, 240, 220),
                      weaponStatus.c_str());

  struct HudBar
  {
    const char * _Label;
    int          _Value;
    int          _MaxValue;
    ImU32        _Color;
  };

  const HudBar bars[] =
  {
    { Loc::Tr(LocId::Fps_Hud_Health), player._Health, game._MaxHealth, IM_COL32(220, 42, 42, 235) },
    { Loc::Tr(LocId::Fps_Hud_Armor),  player._Armor,  game._MaxArmor,  IM_COL32(64, 142, 255, 235) },
    { ammoLabel, iContext._GameWorld->GetWeaponAmmo(activeSlot, game), iContext._GameWorld->GetWeaponMaxAmmo(activeSlot, game), IM_COL32(235, 190, 48, 235) }
  };

  for ( int i = 0; i < 3; ++i )
  {
    const HudBar & bar = bars[i];
    const float y = origin.y + lineHeight * i;
    const int maxValue = std::max(1, bar._MaxValue);
    const int value = MathUtil::Clamp(bar._Value, 0, maxValue);
    const float ratio = static_cast<float>(value) / static_cast<float>(maxValue);
    const ImVec2 barMin(origin.x, y + 4.f);
    const ImVec2 barMax(origin.x + barWidth, y + 4.f + barHeight);
    const ImVec2 fillMax(origin.x + barWidth * ratio, barMax.y);
    const std::string label = std::string(bar._Label) + " " + std::to_string(value) + " / " + std::to_string(maxValue);

    drawList -> AddRectFilled(barMin, barMax, bgColor, 2.f);
    drawList -> AddRectFilled(barMin, fillMax, bar._Color, 2.f);
    drawList -> AddRect(barMin, barMax, IM_COL32(255, 255, 255, 90), 2.f);
    drawList -> AddText(ImVec2(origin.x + 8.f, y + 5.f), textColor, label.c_str());
  }

  const char * hint = Loc::Tr(LocId::Fps_Hud_Hint);
  const ImVec2 hintSize = ImGui::CalcTextSize(hint);
  drawList -> AddText(ImVec2(io.DisplaySize.x - hintSize.x - margin, margin), IM_COL32(245, 245, 245, 140), hint);

  const char * weaponHint = "Scroll / 1-3: Weapon | R: Reload";
  const ImVec2 weaponHintSize = ImGui::CalcTextSize(weaponHint);
  drawList -> AddText(ImVec2(io.DisplaySize.x - weaponHintSize.x - margin, margin + 18.f), IM_COL32(245, 245, 245, 140), weaponHint);

  DrawCrosshair(iContext._GameWorld);
}

// ----------------------------------------------------------------------------
// DrawCrosshair
// ----------------------------------------------------------------------------
void FpsGameHud::DrawCrosshair( const FpsGameWorld * iGameWorld )
{
  const ImGuiIO & io = ImGui::GetIO();
  const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
  const float gap = 3.f;
  const float length = 10.f;
  const float thickness = 2.f;
  const ImU32 color = IM_COL32(255, 24, 24, 255);

  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  drawList -> AddLine(ImVec2(center.x - length, center.y), ImVec2(center.x - gap, center.y), color, thickness);
  drawList -> AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + length, center.y), color, thickness);
  drawList -> AddLine(ImVec2(center.x, center.y - length), ImVec2(center.x, center.y - gap), color, thickness);
  drawList -> AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + length), color, thickness);

  if ( iGameWorld && ( iGameWorld->GetHitMarkerTimer() > 0.f ) )
  {
    const float alphaRatio = MathUtil::Clamp(iGameWorld->GetHitMarkerTimer() / 0.16f, 0.f, 1.f);
    const int alpha = MathUtil::Clamp(static_cast<int>(alphaRatio * 255.f), 0, 255);
    const ImU32 hitColor = IM_COL32(255, 235, 140, alpha);
    const float inner = 5.f;
    const float outer = 12.f;
    drawList -> AddLine(ImVec2(center.x - outer, center.y - outer), ImVec2(center.x - inner, center.y - inner), hitColor, 2.f);
    drawList -> AddLine(ImVec2(center.x + inner, center.y - inner), ImVec2(center.x + outer, center.y - outer), hitColor, 2.f);
    drawList -> AddLine(ImVec2(center.x - outer, center.y + outer), ImVec2(center.x - inner, center.y + inner), hitColor, 2.f);
    drawList -> AddLine(ImVec2(center.x + inner, center.y + inner), ImVec2(center.x + outer, center.y + outer), hitColor, 2.f);
  }
}

}
