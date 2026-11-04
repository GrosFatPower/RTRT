#include "TetrisHud.h"

#include "PathTracer.h"
#include "Renderer.h"
#include "Scene.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace Tetris
{
using namespace RTRT;

namespace
{

// ----------------------------------------------------------------------------
// FormatTime
// ----------------------------------------------------------------------------
std::string FormatTime( double iSeconds )
{
  const int totalMs = static_cast<int>(std::max(0.0, iSeconds) * 100.0);
  const int mins = ( totalMs / 100 ) / 60;
  const int secs = ( totalMs / 100 ) % 60;
  const int hundredths = totalMs % 100;

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d:%02d.%02d", mins, secs, hundredths);
  return std::string(buf);
}

// ----------------------------------------------------------------------------
// GetRendererModeLabel
// ----------------------------------------------------------------------------
const char * GetRendererModeLabel( TetrisRendererMode iMode )
{
  switch ( iMode )
  {
    case TetrisRendererMode::PathTracer: return "Path Tracer (GPU Ray Tracing)";
    case TetrisRendererMode::Software:   return "Software Rasterizer (CPU SIMD)";
    default:                             return "Deferred Renderer (Real-Time PBR)";
  }
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// CTOR / DTOR
// ----------------------------------------------------------------------------
TetrisHud::TetrisHud()
: _Theme(GameUITheme::CreateArcadeTheme())
{
}

TetrisHud::~TetrisHud()
{
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
void TetrisHud::Initialize()
{
  _Theme = GameUITheme::CreateArcadeTheme();
  GameMenuUI::InitializeFonts(_Fonts);
}

// ----------------------------------------------------------------------------
// ToggleSettings / SetMenuPage
// ----------------------------------------------------------------------------
void TetrisHud::ToggleSettings()
{
  _ShowInGameOptions = !_ShowInGameOptions;
}

void TetrisHud::SetMenuPage( TetrisMenuPage iPage )
{
  _MenuPage = iPage;
  _MainMenuNav.Reset();
}

// ----------------------------------------------------------------------------
// DrawMainMenuPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawMainMenuPage( TetrisAppState & ioAppState,
                                  TetrisGameLogic & ioLogic,
                                  TetrisRendererMode iRendererMode,
                                  const GameMenuInput & iMenuInput,
                                  bool & oRequestExit )
{
  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.57f);
  const Vec2 panelSize(520.0f, 0.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisMainMenu", "MAIN MENU", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { "PLAY GAME",           "Choose Marathon, Sprint 40L, or Ultra 2m mode",     "3 MODES",  true, false },
      { "QUICK START",         "Jump straight into Marathon mode at Level 1",       "MARATHON", true, false },
      { "HIGH SCORES & STATS", "View personal best records and session statistics", "",         true, false },
      { "OPTIONS",             "Configure handling (DAS/ARR), renderer & controls", "F1",       true, false },
      { "EXIT TO DESKTOP",     "Close application",                                 "",         true, true  }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _MainMenuNav, iMenuInput, _Theme, _Fonts, 56.0f);
    if ( 0 == activated )
    {
      _SelectedModeIndex = static_cast<int>(ioLogic.GetGameMode());
      SetMenuPage(TetrisMenuPage::ModeSelect);
    }
    else if ( 1 == activated )
    {
      ioLogic.StartNewGame(TetrisGameMode::Marathon, 1, 0);
      ioAppState = TetrisAppState::InGame;
      _ShowInGameOptions = false;
    }
    else if ( 2 == activated )
    {
      SetMenuPage(TetrisMenuPage::HighScores);
    }
    else if ( 3 == activated )
    {
      SetMenuPage(TetrisMenuPage::Options);
    }
    else if ( 4 == activated )
    {
      oRequestExit = true;
    }
  }
  GameMenuUI::EndMenuPanel();

  const std::string leftStatus = std::string("ACTIVE BACKEND: ") + GetRendererModeLabel(iRendererMode);
  GameMenuUI::DrawFooterBar(
    leftStatus.c_str(),
    "[UP/DOWN] Navigate    [ENTER] Select    [F2/F3/F4] Switch Renderer",
    _Theme,
    _Fonts
  );
}

// ----------------------------------------------------------------------------
// DrawModeSelectPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawModeSelectPage( TetrisAppState & ioAppState,
                                    TetrisGameLogic & ioLogic,
                                    const GameMenuInput & iMenuInput )
{
  if ( iMenuInput._Back )
  {
    SetMenuPage(TetrisMenuPage::MainMenu);
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.55f);
  const Vec2 panelSize(760.0f, 0.0f);

  const TetrisSessionStats & stats = ioLogic.GetSessionStats();
  const std::string marathonBest = "Best Score: " + std::to_string(stats._BestMarathonScore);
  const std::string sprintBest   = ( stats._BestSprint40Time > 0.0 )
                                   ? ( "Best Time: " + FormatTime(stats._BestSprint40Time) )
                                   : "Best Time: --:--.--";
  const std::string ultraBest    = "Best Score: " + std::to_string(stats._BestUltraScore);

  if ( GameMenuUI::BeginMenuPanel("##TetrisModeSelect", "SELECT GAME MODE", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<GameSelectionCard> cards =
    {
      {
        "MARATHON",
        "15 LEVELS / 150 LINES",
        "Official Guideline progression through 15 accelerating gravity levels. Enable Endless to play past 150 lines.",
        marathonBest,
        Vec4(0.00f, 0.90f, 0.95f, 1.0f)
      },
      {
        "SPRINT",
        "40 LINES TIME ATTACK",
        "Pure high-speed execution. Clear 40 lines as fast as possible against the precision stopwatch.",
        sprintBest,
        Vec4(1.00f, 0.60f, 0.10f, 1.0f)
      },
      {
        "ULTRA",
        "2-MINUTE SCORE ATTACK",
        "Maximize your score with T-Spins, Back-to-Backs, and Tetrises before the 2:00 timer runs out.",
        ultraBest,
        Vec4(0.75f, 0.30f, 1.00f, 1.0f)
      }
    };

    bool cardConfirmed = false;
    GameMenuUI::DrawSelectionCards(cards, _SelectedModeIndex, iMenuInput, _Theme, _Fonts, &cardConfirmed);

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    int startLevel = ioLogic.GetStartLevel();
    ImGui::SetNextItemWidth(260.0f);
    if ( ImGui::SliderInt("Starting Level", &startLevel, 1, 15) )
      ioLogic.SetStartLevel(startLevel);

    if ( 0 == _SelectedModeIndex )
    {
      ImGui::SameLine(0.0f, 32.0f);
      bool endless = ioLogic.IsEndlessMarathon();
      if ( ImGui::Checkbox("Endless Marathon Mode", &endless) )
        ioLogic.SetEndlessMarathon(endless);
    }

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    const float availW = ImGui::GetContentRegionAvail().x;
    const float btnW = ( availW - 16.0f ) * 0.5f;

    if ( GameMenuUI::DrawActionButton("BACK (ESC)", Vec2(btnW, 46.0f), false, _Theme, _Fonts) )
    {
      SetMenuPage(TetrisMenuPage::MainMenu);
    }

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton("START GAME (ENTER)", Vec2(btnW, 46.0f), true, _Theme, _Fonts) || cardConfirmed )
    {
      const TetrisGameMode chosenMode = static_cast<TetrisGameMode>(_SelectedModeIndex);
      ioLogic.StartNewGame(chosenMode, ioLogic.GetStartLevel(), 0);
      ioAppState = TetrisAppState::InGame;
      _ShowInGameOptions = false;
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(
    "SELECT MODE & LAUNCH",
    "[LEFT/RIGHT] Select Mode    [ENTER] Launch Game    [ESC] Back",
    _Theme,
    _Fonts
  );
}

// ----------------------------------------------------------------------------
// DrawHighScoresPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawHighScoresPage( TetrisGameLogic & ioLogic,
                                    const GameMenuInput & iMenuInput )
{
  if ( iMenuInput._Back )
  {
    SetMenuPage(TetrisMenuPage::MainMenu);
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.55f);
  const Vec2 panelSize(720.0f, 0.0f);

  const TetrisSessionStats & stats = ioLogic.GetSessionStats();

  if ( GameMenuUI::BeginMenuPanel("##TetrisHighScores", "HIGH SCORES & SESSION STATISTICS", center, panelSize, _Theme, _Fonts) )
  {
    const std::string sprintTimeStr = ( stats._BestSprint40Time > 0.0 )
                                      ? FormatTime(stats._BestSprint40Time)
                                      : "--:--.--";

    const std::vector<GameStatCard> statCards =
    {
      {
        "MARATHON RECORD",
        std::to_string(stats._BestMarathonScore),
        "Best Lines: " + std::to_string(stats._BestMarathonLines),
        "Max Level: " + std::to_string(stats._BestMarathonLevel),
        Vec4(0.00f, 0.90f, 0.95f, 1.0f)
      },
      {
        "SPRINT (40L) RECORD",
        sprintTimeStr,
        "Best Score: " + std::to_string(stats._BestSprintScore),
        "Target: 40 Lines",
        Vec4(1.00f, 0.60f, 0.10f, 1.0f)
      },
      {
        "ULTRA (2:00) RECORD",
        std::to_string(stats._BestUltraScore),
        "Best Lines: " + std::to_string(stats._BestUltraLines),
        "Limit: 02:00.00",
        Vec4(0.75f, 0.30f, 1.00f, 1.0f)
      },
      {
        "TOTAL LINES CLEARED",
        std::to_string(stats._TotalLinesCleared),
        "Games Played: " + std::to_string(stats._TotalGamesPlayed),
        "",
        Vec4(0.25f, 0.95f, 0.55f, 1.0f)
      },
      {
        "TETRIS (4-LINE) CLEARS",
        std::to_string(stats._TotalTetrises),
        "Quadruple Line Clears",
        "",
        Vec4(1.00f, 0.85f, 0.15f, 1.0f)
      },
      {
        "T-SPIN CLEARS",
        std::to_string(stats._TotalTSpins),
        "3-Corner Guideline Spins",
        "",
        Vec4(0.95f, 0.35f, 0.75f, 1.0f)
      }
    };

    GameMenuUI::DrawStatCardsGrid(statCards, 2, _Theme, _Fonts);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    const float availW = ImGui::GetContentRegionAvail().x;
    const float btnW = ( availW - 16.0f ) * 0.5f;

    if ( GameMenuUI::DrawActionButton("BACK TO MENU (ESC)", Vec2(btnW, 44.0f), true, _Theme, _Fonts) )
    {
      SetMenuPage(TetrisMenuPage::MainMenu);
    }

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton("RESET SESSION STATS", Vec2(btnW, 44.0f), false, _Theme, _Fonts) )
    {
      ioLogic.ResetSessionStats();
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(
    "SESSION LEADERBOARD",
    "[ESC] Back to Main Menu",
    _Theme,
    _Fonts
  );
}

// ----------------------------------------------------------------------------
// DrawOptionsPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawOptionsPage( TetrisGameLogic & ioLogic,
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
                                 bool & oBackRequested )
{
  oBackRequested = false;
  if ( iMenuInput._Back && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) )
  {
    oBackRequested = true;
    return;
  }

  if ( iMenuInput._PrevTab )
    _OptionsTabIndex = ( _OptionsTabIndex + 2 ) % 3;
  if ( iMenuInput._NextTab )
    _OptionsTabIndex = ( _OptionsTabIndex + 1 ) % 3;

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.54f);
  const Vec2 panelSize(760.0f, 495.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisOptionsPanel", "OPTIONS & CONFIGURATION", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<const char *> tabs =
    {
      "GAMEPLAY & HANDLING",
      "3D GRAPHICS & RENDERER",
      "CONTROLS"
    };
    GameMenuUI::DrawTabBar(tabs, _OptionsTabIndex, _Theme, _Fonts);

    ImGui::BeginChild("##OptionsTabContent", ImVec2(0.0f, -56.0f), false);

    if ( 0 == _OptionsTabIndex )
    {
      ImGui::TextDisabled("Piece Handling & Guideline Tuning");
      ImGui::Separator();

      float dasMs = ioHandling._DAS * 1000.0f;
      if ( ImGui::SliderFloat("DAS - Delayed Auto Shift (ms)", &dasMs, 30.0f, 300.0f, "%.0f ms") )
        ioHandling._DAS = dasMs / 1000.0f;

      float arrMs = ioHandling._ARR * 1000.0f;
      if ( ImGui::SliderFloat("ARR - Auto Repeat Rate (ms)", &arrMs, 0.0f, 100.0f, "%.0f ms") )
        ioHandling._ARR = arrMs / 1000.0f;

      ImGui::SliderFloat("SDF - Soft Drop Factor", &ioHandling._SoftDropFactor, 1.0f, 40.0f, "%.1fx");

      float lockMs = ioHandling._LockDelay * 1000.0f;
      if ( ImGui::SliderFloat("Lock Delay Timer (ms)", &lockMs, 200.0f, 1000.0f, "%.0f ms") )
        ioHandling._LockDelay = lockMs / 1000.0f;

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      if ( ImGui::Button("Preset: Official Guideline (167ms / 33ms)", ImVec2(310.0f, 32.0f)) )
      {
        ioHandling._DAS = 0.167f;
        ioHandling._ARR = 0.033f;
        ioHandling._SoftDropFactor = 20.0f;
        ioHandling._LockDelay = 0.5f;
      }
      ImGui::SameLine();
      if ( ImGui::Button("Preset: Competitive Fast (100ms / 0ms)", ImVec2(310.0f, 32.0f)) )
      {
        ioHandling._DAS = 0.100f;
        ioHandling._ARR = 0.000f;
        ioHandling._SoftDropFactor = 40.0f;
        ioHandling._LockDelay = 0.5f;
      }

      ImGui::Dummy(ImVec2(0.0f, 8.0f));
      ImGui::TextDisabled("Gameplay Assists");
      ImGui::Separator();

      bool showGhost = ioSceneMgr.GetShowGhost();
      if ( ImGui::Checkbox("Show 3D Ghost Piece Landing Cage", &showGhost) )
        ioSceneMgr.SetShowGhost(showGhost);

      int startLvl = ioLogic.GetStartLevel();
      if ( ImGui::SliderInt("Default Starting Level", &startLvl, 1, 15) )
        ioLogic.SetStartLevel(startLvl);
    }
    else if ( 1 == _OptionsTabIndex )
    {
      ImGui::TextDisabled("3D Scene & Material Style");
      ImGui::Separator();

      int currentMatPreset = static_cast<int>(ioSceneMgr.GetMaterialPreset());
      const char * matPresets[] =
      {
        "Glossy Arcade Plastic",
        "Crystal Glass (Transmission / IOR)",
        "Cyberpunk Neon Glow",
        "Brushed Metallic"
      };
      if ( ImGui::Combo("Block Material Preset", &currentMatPreset, matPresets, IM_ARRAYSIZE(matPresets)) )
      {
        ioSceneMgr.SetMaterialPreset(ioScene, static_cast<TetrisMaterialPreset>(currentMatPreset));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }

      int currentCam = static_cast<int>(ioSceneMgr.GetCameraPreset());
      const char * camNames[] =
      {
        "Frontal 3D (Default)",
        "Isometric / Angled",
        "Free Orbit (Mouse Drag & Scroll)"
      };
      if ( ImGui::Combo("In-Game Camera Preset (V)", &currentCam, camNames, IM_ARRAYSIZE(camNames)) )
        ioSceneMgr.SetCameraPreset(static_cast<TetrisCameraPreset>(currentCam));

      int currentClearAnim = static_cast<int>(ioSceneMgr.GetLineClearAnimMode());
      const char * clearAnimNames[] =
      {
        "Dynamic (By Clear Type: 1=Blink, 2=Wave, 3=Cascade, 4=Burst)",
        "Classic Blink (Flash 3x & Vanish)",
        "3D Block Burst (Explosive Shatter)",
        "Edge Cascade (Peel & Fall Off Edges)",
        "Center-Out Wave (Ripple Spin & Shrink)",
        "Random (Cycle All Animations)"
      };
      if ( ImGui::Combo("Line Clear 3D Animation", &currentClearAnim, clearAnimNames, IM_ARRAYSIZE(clearAnimNames)) )
        ioSceneMgr.SetLineClearAnimMode(static_cast<TetrisLineClearAnimMode>(currentClearAnim));

      float backplateOpacity = ioSceneMgr.GetBackplateOpacity();
      if ( ImGui::SliderFloat("Playfield Backplane Opacity", &backplateOpacity, 0.0f, 1.0f, "%.2f") )
      {
        ioSceneMgr.SetBackplateOpacity(ioScene, backplateOpacity);
        if ( ioRenderer )
        {
          ioRenderer->Notify(DirtyState::SceneMaterials);
          ioRenderer->Notify(DirtyState::SceneInstances);
        }
      }

      bool dynLight = ioSceneMgr.GetDynamicPieceLighting();
      if ( ImGui::Checkbox("Active Piece Dynamic Point Light", &dynLight) )
        ioSceneMgr.SetDynamicPieceLighting(dynLight);

      ImGui::SameLine();
      bool camMotion = ioSceneMgr.GetInGameCameraMotion();
      if ( ImGui::Checkbox("In-Game Background Motion", &camMotion) )
        ioSceneMgr.SetInGameCameraMotion(camMotion);

      if ( camMotion )
      {
        float bgSpeed = ioSceneMgr.GetBackgroundMotionSpeed();
        if ( ImGui::SliderFloat("Background Motion Speed", &bgSpeed, 0.0f, 3.0f, "%.2fx") )
          ioSceneMgr.SetBackgroundMotionSpeed(bgSpeed);

        float boardSway = ioSceneMgr.GetBoardSwayAmplitude();
        if ( ImGui::SliderFloat("Board Camera Sway Amplitude", &boardSway, 0.0f, 1.0f, "%.2f") )
          ioSceneMgr.SetBoardSwayAmplitude(boardSway);
      }

      const std::vector<std::string> & bgNames = ioSceneMgr.GetBackgroundNames();
      if ( !bgNames.empty() )
      {
        std::vector<const char *> bgNamePtrs;
        bgNamePtrs.reserve(bgNames.size());
        for ( size_t i = 0; i < bgNames.size(); ++i )
          bgNamePtrs.push_back(bgNames[i].c_str());

        int currentBgIdx = ioSceneMgr.GetCurrentBackgroundIndex();
        if ( ImGui::Combo("HDR Background Map", &currentBgIdx, bgNamePtrs.data(), static_cast<int>(bgNamePtrs.size())) )
          ioSceneMgr.ApplyBackgroundByIndex(currentBgIdx, ioScene, ioSettings, ioRenderer);

        if ( ImGui::Button("Next Random Background", ImVec2(220.0f, 30.0f)) )
          ioSceneMgr.SelectNextRandomBackground(ioScene, ioSettings, ioRenderer);

        ImGui::SameLine();
        bool rotateOnLevel = ioSceneMgr.GetRotateBackgroundOnLevelUp();
        if ( ImGui::Checkbox("Auto-Rotate Background on Level Up", &rotateOnLevel) )
          ioSceneMgr.SetRotateBackgroundOnLevelUp(rotateOnLevel);

        if ( ImGui::SliderFloat("Skybox Rotation (deg)", &ioSettings._SkyBoxRotation, 0.0f, 360.0f, "%.1f deg") )
        {
          if ( ioRenderer )
            ioRenderer->Notify(DirtyState::RenderSettings);
        }
      }

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextDisabled("RTRTCore Engine & Renderer Settings");
      ImGui::Separator();

      const char * rendererNames[] =
      {
        "Deferred Renderer (Real-Time PBR)",
        "Path Tracer (GPU Ray Tracing)",
        "Software Rasterizer (CPU SIMD)"
      };
      int rendererIdx = static_cast<int>(ioRendererMode);
      GameMenuUI::DrawSharedRendererSettings(
        ioScene,
        ioRenderer,
        ioSettings,
        rendererIdx,
        rendererNames,
        IM_ARRAYSIZE(rendererNames),
        oReloadRenderer,
        _StatsUIState,
        iFrameRate,
        iDeltaTime,
        iNbRenderedFrames
      );
      ioRendererMode = static_cast<TetrisRendererMode>(rendererIdx);
    }
    else if ( 2 == _OptionsTabIndex )
    {
      if ( _Gamepad )
      {
        ImGui::TextDisabled("Xbox / XInput Controller Status & Tuning");
        ImGui::Separator();

        if ( _Gamepad->IsConnected() )
        {
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "Controller Connected: %s (Slot %d)",
                             _Gamepad->GetName().c_str(), _Gamepad->GetJoystickId() + 1);
        }
        else
        {
          ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "No Gamepad Detected (Connect an Xbox Controller to play)");
        }

        bool stickHardDrop = _Gamepad->GetAllowStickHardDrop();
        if ( ImGui::Checkbox("Allow Left Stick Up for Hard Drop (Strict Vertical Gate)", &stickHardDrop) )
          _Gamepad->SetAllowStickHardDrop(stickHardDrop);

        ImGui::SameLine(0.0f, 24.0f);
        float deadzone = _Gamepad->GetStickDeadzone();
        ImGui::SetNextItemWidth(160.0f);
        if ( ImGui::SliderFloat("Stick Deadzone", &deadzone, 0.05f, 0.45f, "%.2f") )
          _Gamepad->SetStickDeadzone(deadzone);

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
      }

      const std::vector<GameKeybindEntry> bindings =
      {
        { "PIECE MOVEMENT",     "Move Left",                   "LEFT ARROW",  "A",          "D-PAD / L-STICK LEFT"  },
        { "PIECE MOVEMENT",     "Move Right",                  "RIGHT ARROW", "D",          "D-PAD / L-STICK RIGHT" },
        { "PIECE MOVEMENT",     "Soft Drop",                   "DOWN ARROW",  "S",          "D-PAD / L-STICK DOWN"  },
        { "PIECE MOVEMENT",     "Hard Drop (Instant Lock)",    "SPACE",       "",           "D-PAD UP / L-STICK UP" },
        { "ROTATION & HOLD",    "Rotate Clockwise (90 CW)",    "UP ARROW",    "X",          "B BUTTON"              },
        { "ROTATION & HOLD",    "Rotate Counter-Clockwise",    "Z",           "LEFT CTRL",  "A BUTTON"              },
        { "ROTATION & HOLD",    "Rotate 180 Degrees",          "W",           "",           "X / Y BUTTON"          },
        { "ROTATION & HOLD",    "Hold Active Piece",           "LEFT SHIFT",  "C",          "LB / RB / LT / RT"     },
        { "SYSTEM & RENDERERS", "Pause / Back",                "ESCAPE",      "P",          "START (MENU) / B"      },
        { "SYSTEM & RENDERERS", "Quick Restart Match",         "R",           "",           "BACK (VIEW)"           },
        { "SYSTEM & RENDERERS", "Options Menu",                "F1",          "",           "START -> OPTIONS"      },
        { "SYSTEM & RENDERERS", "Switch to Deferred Renderer", "F2",          "",           ""                      },
        { "SYSTEM & RENDERERS", "Switch to Path Tracer",       "F3",          "",           ""                      },
        { "SYSTEM & RENDERERS", "Switch to Software Raster",   "F4",          "",           ""                      },
        { "SYSTEM & RENDERERS", "Cycle / Orbit 3D Camera",     "V / MOUSE",   "",           "R3 / RIGHT STICK"      }
      };

      GameMenuUI::DrawKeybindingTable(bindings, _Theme, _Fonts);
    }

    ImGui::EndChild();

    ImGui::Separator();
    if ( GameMenuUI::DrawActionButton("BACK (ESC / B)", Vec2(220.0f, 42.0f), true, _Theme, _Fonts) )
      oBackRequested = true;
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(
    "CONFIGURATION",
    "[ESC / B] Back    [LB / RB] Switch Tab    [F2/F3/F4] Hot-Swap Renderer",
    _Theme,
    _Fonts
  );
}

// ----------------------------------------------------------------------------
// DrawTitleScreenMenus
// ----------------------------------------------------------------------------
void TetrisHud::DrawTitleScreenMenus( TetrisAppState & ioAppState,
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
                                      bool & oRequestExit )
{
  GameMenuUI::DrawBackgroundOverlay(_Theme, 0.78f);
  GameMenuUI::DrawTitleHeader(
    "TETRIS 3D",
    "RTRT MULTI-RENDERER GUIDELINE EDITION",
    _Theme,
    _Fonts,
    28.0f
  );

  if ( TetrisMenuPage::MainMenu == _MenuPage )
  {
    DrawMainMenuPage(ioAppState, ioLogic, ioRendererMode, iMenuInput, oRequestExit);
  }
  else if ( TetrisMenuPage::ModeSelect == _MenuPage )
  {
    DrawModeSelectPage(ioAppState, ioLogic, iMenuInput);
  }
  else if ( TetrisMenuPage::HighScores == _MenuPage )
  {
    DrawHighScoresPage(ioLogic, iMenuInput);
  }
  else if ( TetrisMenuPage::Options == _MenuPage )
  {
    bool backRequested = false;
    DrawOptionsPage(
      ioLogic,
      ioSceneMgr,
      ioScene,
      ioRenderer,
      ioSettings,
      ioRendererMode,
      ioHandling,
      iMenuInput,
      iFrameRate,
      iDeltaTime,
      iNbRenderedFrames,
      oReloadRenderer,
      backRequested
    );
    if ( backRequested )
      SetMenuPage(TetrisMenuPage::MainMenu);
  }
}

// ----------------------------------------------------------------------------
// DrawFrameLabel3D
// ----------------------------------------------------------------------------
static void DrawFrameLabel3D( Scene & ioScene,
                              const Vec3 & iWorldPos,
                              const char * iLabel,
                              ImFont * iFont,
                              const GameUITheme & iTheme )
{
  const ImGuiIO & io = ImGui::GetIO();
  if ( ( io.DisplaySize.x <= 0.0f ) || ( io.DisplaySize.y <= 0.0f ) )
    return;

  Camera & cam = ioScene.GetCamera();
  Mat4x4 viewM(1.0f);
  Mat4x4 projM(1.0f);
  cam.ComputeLookAtMatrix(viewM);
  cam.ComputeProjMatrix(io.DisplaySize.x / io.DisplaySize.y, projM);

  const Vec4 clip = projM * viewM * Vec4(iWorldPos, 1.0f);
  if ( clip.w <= 0.01f )
    return;

  const Vec3 ndc = Vec3(clip) / clip.w;
  const float sx = ( ndc.x * 0.5f + 0.5f ) * io.DisplaySize.x;
  const float sy = ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * io.DisplaySize.y;

  ImDrawList * bgList = ImGui::GetBackgroundDrawList();
  const float fontSize = iFont ? iFont->LegacySize : 22.0f;
  const ImVec2 textSize = iFont ? iFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iLabel)
                                : ImGui::CalcTextSize(iLabel);

  const float padX = 16.0f;
  const float padY = 5.0f;
  const ImVec2 boxMin(sx - textSize.x * 0.5f - padX, sy - textSize.y * 0.5f - padY);
  const ImVec2 boxMax(sx + textSize.x * 0.5f + padX, sy + textSize.y * 0.5f + padY);
  const ImVec2 textPos(sx - textSize.x * 0.5f, sy - textSize.y * 0.5f);

  bgList->AddRectFilled(boxMin, boxMax, IM_COL32(10, 14, 22, 215), 5.0f);
  bgList->AddRect(boxMin, boxMax, IM_COL32(0, 228, 245, 165), 5.0f, 0, 1.5f);

  if ( iFont )
  {
    bgList->AddText(iFont, fontSize, ImVec2(textPos.x + 1.5f, textPos.y + 1.5f), IM_COL32(0, 0, 0, 220), iLabel);
    bgList->AddText(iFont, fontSize, textPos, IM_COL32(240, 248, 255, 255), iLabel);
  }
  else
  {
    bgList->AddText(textPos, IM_COL32(240, 248, 255, 255), iLabel);
  }

  (void)iTheme;
}

// ----------------------------------------------------------------------------
// DrawGameplayOverlay
// ----------------------------------------------------------------------------
void TetrisHud::DrawGameplayOverlay( const TetrisGameLogic & iLogic, Scene & ioScene )
{
  // Draw 3D-anchored "HOLD" and "NEXT" headers above the Hold and Next frames
  DrawFrameLabel3D(ioScene, Vec3(-8.0f, 20.85f, -0.2f), "HOLD", _Fonts._HeaderFont, _Theme);
  DrawFrameLabel3D(ioScene, Vec3( 8.0f, 20.85f, -0.2f), "NEXT", _Fonts._HeaderFont, _Theme);

  ImGuiIO & io = ImGui::GetIO();
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                               | ImGuiWindowFlags_AlwaysAutoResize
                               | ImGuiWindowFlags_NoSavedSettings
                               | ImGuiWindowFlags_NoFocusOnAppearing
                               | ImGuiWindowFlags_NoNav
                               | ImGuiWindowFlags_NoMove;

  GameMenuUI::PushThemeStyle(_Theme);

  // 1. Left Telemetry Panel
  const float pad = 18.0f;
  ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(235.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.78f);

  if ( ImGui::Begin("##TetrisLeftTelemetry", nullptr, flags) )
  {
    const char * modeLabel = "MARATHON";
    if ( TetrisGameMode::Sprint == iLogic.GetGameMode() ) modeLabel = "SPRINT (40L)";
    else if ( TetrisGameMode::Ultra == iLogic.GetGameMode() ) modeLabel = "ULTRA (2:00)";

    if ( _Fonts._HeaderFont ) ImGui::PushFont(_Fonts._HeaderFont);
    ImGui::TextColored(ImVec4(0.0f, 0.90f, 0.96f, 1.0f), "%s", modeLabel);
    if ( _Fonts._HeaderFont ) ImGui::PopFont();
    ImGui::Separator();

    ImGui::TextDisabled("SCORE");
    if ( _Fonts._MonoFont ) ImGui::PushFont(_Fonts._MonoFont);
    ImGui::Text("%lld", iLogic.GetScore());
    if ( _Fonts._MonoFont ) ImGui::PopFont();

    ImGui::TextDisabled("HIGH SCORE");
    if ( _Fonts._MonoFont ) ImGui::PushFont(_Fonts._MonoFont);
    ImGui::Text("%lld", iLogic.GetHighScore());
    if ( _Fonts._MonoFont ) ImGui::PopFont();

    ImGui::Separator();
    ImGui::Text("LEVEL : %d", iLogic.GetLevel());
    if ( TetrisGameMode::Sprint == iLogic.GetGameMode() )
      ImGui::Text("LINES : %d / 40", iLogic.GetLinesCleared());
    else if ( ( TetrisGameMode::Marathon == iLogic.GetGameMode() ) && !iLogic.IsEndlessMarathon() )
      ImGui::Text("LINES : %d / 150", iLogic.GetLinesCleared());
    else
      ImGui::Text("LINES : %d", iLogic.GetLinesCleared());

    if ( iLogic.GetComboCount() > 0 )
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "COMBO : %dx", iLogic.GetComboCount());

    if ( iLogic.IsBackToBackActive() )
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.95f, 1.0f), "[BACK-TO-BACK]");

    ImGui::Dummy(ImVec2(0.f, 6.f));
    ImGui::TextDisabled("[ESC / P] Pause Menu");
    ImGui::TextDisabled("[F1] Options   [F2-F4] Renderer");
  }
  ImGui::End();

  // 2. Right Telemetry Panel
  const float rightPanelWidth = 205.0f;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - rightPanelWidth - pad, pad), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.78f);

  if ( ImGui::Begin("##TetrisRightTelemetry", nullptr, flags) )
  {
    if ( TetrisGameMode::Ultra == iLogic.GetGameMode() )
    {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "TIME REMAINING");
      if ( _Fonts._MonoFont ) ImGui::PushFont(_Fonts._MonoFont);
      ImGui::Text("%s", FormatTime(iLogic.GetRemainingTime()).c_str());
      if ( _Fonts._MonoFont ) ImGui::PopFont();
    }
    else
    {
      ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "ELAPSED TIME");
      if ( _Fonts._MonoFont ) ImGui::PushFont(_Fonts._MonoFont);
      ImGui::Text("%s", FormatTime(iLogic.GetElapsedTime()).c_str());
      if ( _Fonts._MonoFont ) ImGui::PopFont();
    }

    ImGui::Separator();
    ImGui::TextDisabled("NEXT QUEUE");
    for ( int i = 0; i < 5; ++i )
    {
      const TetrisPieceType piece = iLogic.PeekNextPiece(i);
      const Vec3 col = TetrisPiece::GetColor(piece);
      ImGui::TextColored(ImVec4(col.x, col.y, col.z, 1.0f), "%d. [%s]", i + 1, TetrisPiece::GetName(piece));
    }
  }
  ImGui::End();

  GameMenuUI::PopThemeStyle();
}

// ----------------------------------------------------------------------------
// DrawActionBanner
// ----------------------------------------------------------------------------
void TetrisHud::DrawActionBanner( const TetrisGameLogic & iLogic )
{
  const float alpha = iLogic.GetActionBannerAlpha();
  if ( ( alpha <= 0.01f ) || iLogic.GetActionBannerText().empty() )
    return;

  ImGuiIO & io = ImGui::GetIO();
  const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.25f);

  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  const std::string & text = iLogic.GetActionBannerText();

  ImFont * font = _Fonts._TitleFont ? _Fonts._TitleFont : ImGui::GetFont();
  const float fontSize = font ? ( font->LegacySize * 0.78f ) : 32.0f;
  const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
  const ImVec2 textPos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);

  const ImU32 shadowCol = IM_COL32(0, 0, 0, static_cast<int>(220.f * alpha));
  const ImU32 mainCol   = IM_COL32(255, 240, 95, static_cast<int>(255.f * alpha));

  drawList->AddText(font, fontSize, ImVec2(textPos.x + 3.0f, textPos.y + 3.0f), shadowCol, text.c_str());
  drawList->AddText(font, fontSize, textPos, mainCol, text.c_str());
}

// ----------------------------------------------------------------------------
// DrawInGamePauseOrEndModal
// ----------------------------------------------------------------------------
void TetrisHud::DrawInGamePauseOrEndModal( TetrisAppState & ioAppState,
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
                                           bool & oReloadRenderer )
{
  GameMenuUI::DrawBackgroundOverlay(_Theme, 0.85f);

  if ( _ShowInGameOptions )
  {
    bool backRequested = false;
    DrawOptionsPage(
      ioLogic,
      ioSceneMgr,
      ioScene,
      ioRenderer,
      ioSettings,
      ioRendererMode,
      ioHandling,
      iMenuInput,
      iFrameRate,
      iDeltaTime,
      iNbRenderedFrames,
      oReloadRenderer,
      backRequested
    );
    if ( backRequested )
      _ShowInGameOptions = false;
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.52f);

  // 1. Pause Menu
  if ( ioLogic.IsPaused() && !ioLogic.IsGameOver() && !ioLogic.IsVictory() )
  {
    if ( GameMenuUI::BeginMenuPanel("##TetrisPauseModal", "GAME PAUSED", center, Vec2(480.0f, 0.0f), _Theme, _Fonts) )
    {
      const std::vector<GameMenuItem> pauseItems =
      {
        { "RESUME GAME",            "Return to active match",                 "ESC / P", true, false },
        { "RESTART MATCH",          "Start a fresh board in current mode",    "R",       true, false },
        { "OPTIONS",                "Configure handling, renderer & visuals", "F1",      true, false },
        { "RETURN TO TITLE SCREEN", "Exit match and return to Main Menu",     "",        true, true  }
      };

      const int act = GameMenuUI::DrawMenuButtonList(pauseItems, _PauseMenuNav, iMenuInput, _Theme, _Fonts, 56.0f);
      if ( 0 == act )
      {
        ioLogic.SetPaused(false);
      }
      else if ( 1 == act )
      {
        ioLogic.Reset();
      }
      else if ( 2 == act )
      {
        _ShowInGameOptions = true;
      }
      else if ( 3 == act )
      {
        ioLogic.SetPaused(false);
        SetMenuPage(TetrisMenuPage::MainMenu);
        ioAppState = TetrisAppState::TitleScreen;
      }
    }
    GameMenuUI::EndMenuPanel();
    return;
  }

  // 2. Game Over / Victory Summary Modal
  if ( ioLogic.IsGameOver() || ioLogic.IsVictory() )
  {
    const char * headerTitle = ioLogic.IsVictory() ? "OBJECTIVE COMPLETE - VICTORY!" : "GAME OVER";
    if ( GameMenuUI::BeginMenuPanel("##TetrisEndModal", headerTitle, center, Vec2(520.0f, 0.0f), _Theme, _Fonts) )
    {
      if ( ioLogic.IsGameOver() && !ioLogic.GetGameOverReason().empty() )
      {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Cause: %s", ioLogic.GetGameOverReason().c_str());
      }

      const std::vector<GameStatCard> summaryCards =
      {
        {
          "FINAL SCORE",
          std::to_string(ioLogic.GetScore()),
          "Level Reached: " + std::to_string(ioLogic.GetLevel()),
          "",
          Vec4(0.00f, 0.90f, 0.95f, 1.0f)
        },
        {
          "LINES & TIME",
          FormatTime(ioLogic.GetElapsedTime()),
          "Lines Cleared: " + std::to_string(ioLogic.GetLinesCleared()),
          "",
          Vec4(0.25f, 0.95f, 0.55f, 1.0f)
        }
      };
      GameMenuUI::DrawStatCardsGrid(summaryCards, 2, _Theme, _Fonts);

      ImGui::Dummy(ImVec2(0.0f, 6.0f));

      const std::vector<GameMenuItem> endItems =
      {
        { "PLAY AGAIN",             "Restart immediately in current mode", "R", true, false },
        { "CHANGE GAME MODE",       "Return to Mode Selection screen",     "",  true, false },
        { "RETURN TO TITLE SCREEN", "Exit to Main Menu",                   "",  true, true  }
      };

      const int act = GameMenuUI::DrawMenuButtonList(endItems, _GameOverMenuNav, iMenuInput, _Theme, _Fonts, 56.0f);
      if ( 0 == act )
      {
        ioLogic.Reset();
      }
      else if ( 1 == act )
      {
        SetMenuPage(TetrisMenuPage::ModeSelect);
        ioAppState = TetrisAppState::TitleScreen;
      }
      else if ( 2 == act )
      {
        SetMenuPage(TetrisMenuPage::MainMenu);
        ioAppState = TetrisAppState::TitleScreen;
      }
    }
    GameMenuUI::EndMenuPanel();
  }
}

// ----------------------------------------------------------------------------
// Draw
// ----------------------------------------------------------------------------
int TetrisHud::Draw( TetrisAppState & ioAppState,
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
                     bool & oRequestExit )
{
  oRequestExit = false;

  if ( TetrisAppState::TitleScreen == ioAppState )
  {
    DrawTitleScreenMenus(
      ioAppState,
      ioLogic,
      ioSceneMgr,
      ioScene,
      ioRenderer,
      ioSettings,
      ioRendererMode,
      ioHandling,
      iMenuInput,
      iFrameRate,
      iDeltaTime,
      iNbRenderedFrames,
      oReloadRenderer,
      oRequestExit
    );
  }
  else
  {
    DrawGameplayOverlay(ioLogic, ioScene);
    DrawActionBanner(ioLogic);

    if ( ioLogic.IsPaused() || ioLogic.IsGameOver() || ioLogic.IsVictory() || _ShowInGameOptions )
    {
      DrawInGamePauseOrEndModal(
        ioAppState,
        ioLogic,
        ioSceneMgr,
        ioScene,
        ioRenderer,
        ioSettings,
        ioRendererMode,
        ioHandling,
        iMenuInput,
        iFrameRate,
        iDeltaTime,
        iNbRenderedFrames,
        oReloadRenderer
      );
    }
  }

  return 0;
}

} // namespace Tetris
