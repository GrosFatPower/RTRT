#include "FpsGameHud.h"

#include "Renderer.h"
#include "Scene.h"

#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>

namespace RTRT
{

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
    case FpsRendererMode::Software:        return "Software Rasterizer (CPU SIMD)";
    case FpsRendererMode::PhotoPathTracer: return "Photo Path Tracer (GPU Ray Tracing)";
    default:                               return "Deferred Renderer (Real-Time PBR)";
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

  if ( GameMenuUI::BeginMenuPanel("##FpsMainMenu", "MAIN MENU", center, Vec2(520.0f, 0.0f), _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { "DEPLOY / SELECT MAP", "Choose an arena or load a custom .fpsmap file",         "MAPS", true, false },
      { "QUICK START",         "Deploy immediately into the active arena",              "PLAY", true, false },
      { "MAP EDITOR",          "Open the interactive scene, prop, light & material editor", "F3", true, false },
      { "OPTIONS",             "Configure gameplay, renderer backend & controls",       "F1",   true, false },
      { "EXIT TO DESKTOP",     "Close application",                                     "",     true, true  }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _MainMenuNav, *ioContext._MenuInput, _Theme, _Fonts, 56.0f);
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

  const std::string leftStatus = std::string("BACKEND: ") + GetRendererModeLabel(ioContext._GameSettings->_RendererMode)
                               + "    MAP: " + ioContext._MapName;
  GameMenuUI::DrawFooterBar(leftStatus.c_str(), "[UP/DOWN] Navigate    [ENTER] Select    [F1] Options    [F3] Editor", _Theme, _Fonts);
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

  if ( GameMenuUI::BeginMenuPanel("##FpsMapSelect", "SELECT MAP", center, Vec2(820.0f, 0.0f), _Theme, _Fonts) )
  {
    const std::vector<GameSelectionCard> cards =
    {
      {
        "CYBER CITY",
        "ccity.fpsmap",
        "Large-scale urban sector with props, dynamic lights and boid flocks.",
        PathEndsWith(ioContext._MapPath, "ccity.fpsmap") ? "ACTIVE MAP" : "AVAILABLE",
        Vec4(0.96f, 0.72f, 0.18f, 1.0f)
      },
      {
        "DEFAULT ARENA",
        "default.fpsmap",
        "Compact tactical arena with pillars, target crates and clean sightlines.",
        PathEndsWith(ioContext._MapPath, "default.fpsmap") ? "ACTIVE MAP" : "AVAILABLE",
        Vec4(0.90f, 0.25f, 0.22f, 1.0f)
      },
      {
        "CUSTOM MAP",
        "Browse .fpsmap...",
        "Load any user-authored .fpsmap file from disk using the native file dialog.",
        "FILE DIALOG",
        Vec4(0.25f, 0.95f, 0.55f, 1.0f)
      }
    };

    bool cardConfirmed = false;
    GameMenuUI::DrawSelectionCards(cards, _SelectedMapIndex, *ioContext._MenuInput, _Theme, _Fonts, &cardConfirmed);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::Checkbox("Free Look Mode", &ioContext._GameSettings->_FreeLook);
    ImGui::SameLine(0.0f, 32.0f);
    if ( ImGui::Checkbox("Show View Weapon", &ioContext._GameSettings->_ShowViewWeapon) )
      ioContext._WeaponDirty = true;

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    const float availW = ImGui::GetContentRegionAvail().x;
    const float btnW = ( availW - 16.0f ) * 0.5f;

    if ( GameMenuUI::DrawActionButton("BACK (ESC)", Vec2(btnW, 46.0f), false, _Theme, _Fonts) )
      SetMenuPage(FpsMenuPage::MainMenu);

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton("DEPLOY (ENTER)", Vec2(btnW, 46.0f), true, _Theme, _Fonts) || cardConfirmed )
    {
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

  GameMenuUI::DrawFooterBar("SELECT MAP & DEPLOY", "[LEFT/RIGHT] Select Map    [ENTER] Deploy    [ESC] Back", _Theme, _Fonts);
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
    _OptionsTabIndex = ( _OptionsTabIndex + 2 ) % 3;
  if ( menuInput._NextTab )
    _OptionsTabIndex = ( _OptionsTabIndex + 1 ) % 3;

  FpsGameSettings & game = *ioContext._GameSettings;

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.54f);

  if ( GameMenuUI::BeginMenuPanel("##FpsOptionsPanel", "OPTIONS & CONFIGURATION", center, Vec2(780.0f, 540.0f), _Theme, _Fonts) )
  {
    const std::vector<const char *> tabs = { "GAMEPLAY & WEAPON", "GRAPHICS & RENDERER", "CONTROLS" };
    GameMenuUI::DrawTabBar(tabs, _OptionsTabIndex, _Theme, _Fonts);

    ImGui::BeginChild("##FpsOptionsTabContent", ImVec2(0.0f, -56.0f), false);

    if ( 0 == _OptionsTabIndex )
    {
      ImGui::TextDisabled("Movement & Camera");
      ImGui::Separator();
      ImGui::SliderFloat("Move speed", &game._MoveSpeed, 1.f, 100.f);
      ImGui::SliderFloat("Sprint speed", &game._SprintSpeed, 1.f, 16.f);
      ImGui::SliderFloat("Mouse sensitivity", &game._MouseSensitivity, 0.01f, 0.25f);
      ImGui::SliderFloat("Field of view", &game._CameraFOV, 30.f, 140.f, "%.0f deg");
      ImGui::SliderFloat("Gravity", &game._Gravity, 1.f, 35.f);
      ImGui::SliderFloat("Jump speed", &game._JumpSpeed, 1.f, 12.f);
      ImGui::Checkbox("Free look (no physics)", &game._FreeLook);

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextDisabled("Head Bob");
      ImGui::Separator();
      ImGui::Checkbox("Head bob enabled", &game._HeadBob._Enabled);
      ImGui::SliderFloat("Bob amplitude", &game._HeadBob._Amplitude, 0.f, 0.08f, "%.3f");
      ImGui::SliderFloat("Bob frequency", &game._HeadBob._Frequency, 0.5f, 4.f, "%.2f");
      ImGui::SliderFloat("Bob sway", &game._HeadBob._Sway, 0.f, 0.05f, "%.3f");

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextDisabled("Projectiles & Weapon");
      ImGui::Separator();
      ImGui::SliderFloat("Projectile speed", &game._ProjectileSpeed, 1.f, 40.f);
      ImGui::SliderFloat("Projectile gravity", &game._ProjectileGravity, 0.f, 30.f);
      ImGui::SliderFloat("Projectile bounciness", &game._ProjectileBounciness, 0.f, 1.f);
      ImGui::SliderFloat("Projectile cooldown", &game._ProjectileCooldown, 0.02f, 1.f);
      int maxProjectiles = game._MaxProjectiles;
      if ( ImGui::SliderInt("Max projectiles", &maxProjectiles, 1, 128) )
      {
        game._MaxProjectiles = std::max(1, maxProjectiles);
        ioContext._RequestReloadScene = true;
      }
      if ( ImGui::Checkbox("Show view weapon", &game._ShowViewWeapon) )
        ioContext._WeaponDirty = true;
    }
    else if ( 1 == _OptionsTabIndex )
    {
      ImGui::TextDisabled("RTRTCore Engine & Renderer Settings");
      ImGui::Separator();

      const char * rendererNames[] =
      {
        "Deferred Renderer (Real-Time PBR)",
        "Software Rasterizer (CPU SIMD)",
        "Photo Path Tracer (GPU Ray Tracing)"
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
      ImGui::TextDisabled("Developer Tools");
      ImGui::Separator();
      bool devPanels = ioContext._DevPanelsVisible;
      if ( ImGui::Checkbox("Show developer panels (F4)", &devPanels) )
        ioContext._RequestToggleDevPanels = true;
    }
    else
    {
      if ( ioContext._Gamepad )
      {
        ImGui::TextDisabled("Xbox / XInput Controller Status & Tuning");
        ImGui::Separator();

        if ( ioContext._Gamepad->IsConnected() )
        {
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "Controller Connected: %s (Slot %d)",
                             ioContext._Gamepad->GetName().c_str(), ioContext._Gamepad->GetJoystickId() + 1);
        }
        else
          ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "No Gamepad Detected (Connect an Xbox Controller to play)");

        float deadzone = ioContext._Gamepad->GetStickDeadzone();
        ImGui::SetNextItemWidth(220.0f);
        if ( ImGui::SliderFloat("Stick deadzone", &deadzone, 0.05f, 0.45f, "%.2f") )
          ioContext._Gamepad->SetStickDeadzone(deadzone);

        if ( ioContext._GamepadLookSensitivity )
        {
          ImGui::SetNextItemWidth(220.0f);
          ImGui::SliderFloat("Look speed (deg/s)", ioContext._GamepadLookSensitivity, 40.f, 360.f, "%.0f");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
      }

      const std::vector<GameKeybindEntry> bindings =
      {
        { "MOVEMENT",        "Move Forward / Back",     "W / S",       "UP / DOWN",  "L-STICK UP / DOWN"  },
        { "MOVEMENT",        "Strafe Left / Right",     "A / D",       "",           "L-STICK LEFT / RIGHT" },
        { "MOVEMENT",        "Look Around",             "MOUSE",       "",           "R-STICK"            },
        { "MOVEMENT",        "Jump",                    "SPACE",       "",           "A BUTTON"           },
        { "MOVEMENT",        "Sprint",                  "LEFT SHIFT",  "",           "L3 / LB"            },
        { "COMBAT",          "Fire Projectile",         "LEFT CLICK",  "",           "RT"                 },
        { "COMBAT",          "Respawn Player",          "R",           "",           "Y BUTTON"           },
        { "SYSTEM",          "Pause Menu",              "ESCAPE",      "P",          "START"              },
        { "SYSTEM",          "Options Menu",            "F1",          "",           ""                   },
        { "SYSTEM",          "Toggle Free Look",        "F2",          "",           ""                   },
        { "SYSTEM",          "Map Editor",              "F3",          "",           ""                   },
        { "SYSTEM",          "Developer Panels",        "F4",          "",           ""                   },
        { "SYSTEM",          "Deferred / Software / Photo", "J / K / L", "",          ""                   }
      };
      GameMenuUI::DrawKeybindingTable(bindings, _Theme, _Fonts);
    }

    ImGui::EndChild();

    ImGui::Separator();
    if ( GameMenuUI::DrawActionButton("BACK (ESC / B)", Vec2(220.0f, 42.0f), true, _Theme, _Fonts) )
      oBackRequested = true;
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar("CONFIGURATION", "[ESC / B] Back    [LB / RB] Switch Tab", _Theme, _Fonts);
}

// ----------------------------------------------------------------------------
// DrawTitleScreenMenus
// ----------------------------------------------------------------------------
void FpsGameHud::DrawTitleScreenMenus( FpsGameHudContext & ioContext )
{
  GameMenuUI::DrawBackgroundOverlay(_Theme, 0.78f);
  GameMenuUI::DrawTitleHeader("RTRT TACTICAL FPS", "MULTI-RENDERER FIRST-PERSON SANDBOX", _Theme, _Fonts, 28.0f);

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

  if ( GameMenuUI::BeginMenuPanel("##FpsPauseModal", "GAME PAUSED", center, Vec2(500.0f, 0.0f), _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { "RESUME GAME",            "Return to active combat",                    "ESC / P", true, false },
      { "RESPAWN PLAYER",         "Reset position, velocity & clear projectiles", "R",     true, false },
      { "SWITCH MAP",             "Open the map selection screen",              "MAPS",    true, false },
      { "ENTER MAP EDITOR",       "Switch to the interactive 3D map editor",    "F3",      true, false },
      { "OPTIONS",                "Configure gameplay, renderer & controls",    "F1",      true, false },
      { "RETURN TO TITLE SCREEN", "Exit match and return to Main Menu",         "",        true, true  }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _PauseMenuNav, *ioContext._MenuInput, _Theme, _Fonts, 56.0f);
    if ( 0 == activated )
      ioContext._RequestResume = true;
    else if ( 1 == activated )
      ioContext._RequestRespawn = true;
    else if ( 2 == activated )
      ioContext._RequestMapSelect = true;
    else if ( 3 == activated )
      ioContext._RequestEditor = true;
    else if ( 4 == activated )
      _ShowInGameOptions = true;
    else if ( 5 == activated )
      ioContext._RequestTitle = true;
  }
  GameMenuUI::EndMenuPanel();
}

// ----------------------------------------------------------------------------
// Draw
// ----------------------------------------------------------------------------
void FpsGameHud::Draw( FpsGameHudContext & ioContext )
{
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

  DrawGameplayHud(ioContext);
  if ( ioContext._Paused )
    DrawInGamePauseModal(ioContext);
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

  struct HudBar
  {
    const char * _Label;
    int          _Value;
    int          _MaxValue;
    ImU32        _Color;
  };

  const HudBar bars[] =
  {
    { "HEALTH", player._Health, game._MaxHealth, IM_COL32(220, 42, 42, 235) },
    { "ARMOR", player._Armor, game._MaxArmor, IM_COL32(64, 142, 255, 235) },
    { "PROJECTILES", iContext._GameWorld->GetProjectileAmmo(), game._MaxProjectileAmmo, IM_COL32(235, 190, 48, 235) }
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

  const char * hint = "[ESC] Pause   [F1] Options   [F3] Editor";
  const ImVec2 hintSize = ImGui::CalcTextSize(hint);
  drawList -> AddText(ImVec2(io.DisplaySize.x - hintSize.x - margin, margin), IM_COL32(245, 245, 245, 140), hint);

  DrawCrosshair();
}

// ----------------------------------------------------------------------------
// DrawCrosshair
// ----------------------------------------------------------------------------
void FpsGameHud::DrawCrosshair()
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
}

}
