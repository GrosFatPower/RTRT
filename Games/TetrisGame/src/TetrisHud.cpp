#include "TetrisHud.h"
#include "AudioEngine.h"

#include "PathTracer.h"
#include "Renderer.h"
#include "RoomCode.h"
#include "Scene.h"
#include "TetrisNetProtocol.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

// ----------------------------------------------------------------------------
// ProjectWorldToScreen
// ----------------------------------------------------------------------------
bool ProjectWorldToScreen( Scene & ioScene, const Vec3 & iWorldPos, ImVec2 & oScreenPos )
{
  const ImGuiIO & io = ImGui::GetIO();
  if ( ( io.DisplaySize.x <= 0.0f ) || ( io.DisplaySize.y <= 0.0f ) )
    return false;

  Camera & cam = ioScene.GetCamera();
  Mat4x4 viewM(1.0f);
  Mat4x4 projM(1.0f);
  cam.ComputeLookAtMatrix(viewM);
  cam.ComputeProjMatrix(io.DisplaySize.x / io.DisplaySize.y, projM);

  const Vec4 clip = projM * viewM * Vec4(iWorldPos, 1.0f);
  if ( clip.w <= 0.01f )
    return false;

  const Vec3 ndc = Vec3(clip) / clip.w;
  oScreenPos.x = ( ndc.x * 0.5f + 0.5f ) * io.DisplaySize.x;
  oScreenPos.y = ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * io.DisplaySize.y;
  return true;
}

// ----------------------------------------------------------------------------
// DrawFrameLabel3DColored
// ----------------------------------------------------------------------------
void DrawFrameLabel3DColored( Scene & ioScene,
                              const Vec3 & iWorldPos,
                              const char * iLabel,
                              ImFont * iFont,
                              ImU32 iBorderCol,
                              ImU32 iTextCol = IM_COL32(240, 248, 255, 255) )
{
  ImVec2 screenPos;
  if ( !ProjectWorldToScreen(ioScene, iWorldPos, screenPos) )
    return;

  ImDrawList * bgList = ImGui::GetBackgroundDrawList();
  const float fontSize = iFont ? iFont->LegacySize : 22.0f;
  const ImVec2 textSize = iFont ? iFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iLabel)
                                : ImGui::CalcTextSize(iLabel);

  const float padX = 16.0f;
  const float padY = 5.0f;
  const ImVec2 boxMin(screenPos.x - textSize.x * 0.5f - padX, screenPos.y - textSize.y * 0.5f - padY);
  const ImVec2 boxMax(screenPos.x + textSize.x * 0.5f + padX, screenPos.y + textSize.y * 0.5f + padY);
  const ImVec2 textPos(screenPos.x - textSize.x * 0.5f, screenPos.y - textSize.y * 0.5f);

  bgList->AddRectFilled(boxMin, boxMax, IM_COL32(10, 14, 22, 215), 5.0f);
  bgList->AddRect(boxMin, boxMax, iBorderCol, 5.0f, 0, 1.5f);

  if ( iFont )
  {
    bgList->AddText(iFont, fontSize, ImVec2(textPos.x + 1.5f, textPos.y + 1.5f), IM_COL32(0, 0, 0, 220), iLabel);
    bgList->AddText(iFont, fontSize, textPos, iTextCol, iLabel);
  }
  else
  {
    bgList->AddText(textPos, iTextCol, iLabel);
  }
}

// ----------------------------------------------------------------------------
// DrawFrameLabel3D
// ----------------------------------------------------------------------------
void DrawFrameLabel3D( Scene & ioScene,
                       const Vec3 & iWorldPos,
                       const char * iLabel,
                       ImFont * iFont,
                       const GameUITheme & iTheme )
{
  (void)iTheme;
  DrawFrameLabel3DColored(ioScene, iWorldPos, iLabel, iFont, IM_COL32(0, 228, 245, 165));
}

// ----------------------------------------------------------------------------
// DrawGarbageMeter3D
// ----------------------------------------------------------------------------
void DrawGarbageMeter3D( Scene & ioScene,
                         const Vec3 & iBoardOrigin,
                         int iPendingGarbage,
                         ImFont * iFont )
{
  ImVec2 botPos, topPos;
  if ( !ProjectWorldToScreen(ioScene, iBoardOrigin + Vec3(-5.55f, 0.0f, 0.2f), botPos) ||
       !ProjectWorldToScreen(ioScene, iBoardOrigin + Vec3(-5.55f, 20.0f, 0.2f), topPos) )
  {
    return;
  }

  ImDrawList * bgList = ImGui::GetBackgroundDrawList();
  const float barHalfW = std::max(4.5f, std::fabs(botPos.y - topPos.y) * 0.016f);
  const float minX = botPos.x - barHalfW;
  const float maxX = botPos.x + barHalfW;
  const float topY = std::min(topPos.y, botPos.y);
  const float botY = std::max(topPos.y, botPos.y);
  const float totalH = botY - topY;
  if ( totalH <= 10.0f )
    return;

  // Outer dark track
  bgList->AddRectFilled(ImVec2(minX - 2.0f, topY - 2.0f), ImVec2(maxX + 2.0f, botY + 2.0f), IM_COL32(8, 12, 18, 210), 4.0f);
  bgList->AddRect(ImVec2(minX - 2.0f, topY - 2.0f), ImVec2(maxX + 2.0f, botY + 2.0f),
                  ( iPendingGarbage > 0 ) ? IM_COL32(255, 75, 65, 220) : IM_COL32(70, 85, 105, 140),
                  4.0f, 0, 1.5f);

  if ( iPendingGarbage <= 0 )
    return;

  const int clampedLines = std::min(20, iPendingGarbage);
  const float segH = totalH / 20.0f;

  for ( int i = 0; i < clampedLines; ++i )
  {
    const float segBot = botY - static_cast<float>(i) * segH;
    const float segTop = segBot - segH + 1.5f;
    const ImU32 segCol = ( i >= 8 )
                         ? IM_COL32(255, 45, 45, 245)
                         : IM_COL32(255, 135, 35, 240);
    bgList->AddRectFilled(ImVec2(minX, segTop), ImVec2(maxX, segBot), segCol, 2.0f);
  }

  char badgeBuf[16];
  std::snprintf(badgeBuf, sizeof(badgeBuf), "! +%d", iPendingGarbage);
  const float fontSize = iFont ? ( iFont->LegacySize * 0.85f ) : 18.0f;
  const ImVec2 txtSize = iFont ? iFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, badgeBuf)
                               : ImGui::CalcTextSize(badgeBuf);
  const ImVec2 badgePos(botPos.x - txtSize.x * 0.5f, botY + 6.0f);
  bgList->AddRectFilled(ImVec2(badgePos.x - 6.0f, badgePos.y - 2.0f),
                        ImVec2(badgePos.x + txtSize.x + 6.0f, badgePos.y + txtSize.y + 2.0f),
                        IM_COL32(45, 10, 10, 230), 4.0f);
  if ( iFont )
    bgList->AddText(iFont, fontSize, badgePos, IM_COL32(255, 90, 80, 255), badgeBuf);
  else
    bgList->AddText(badgePos, IM_COL32(255, 90, 80, 255), badgeBuf);
}

// ----------------------------------------------------------------------------
// DrawBoardActionBanner3D
// ----------------------------------------------------------------------------
void DrawBoardActionBanner3D( Scene & ioScene,
                              const Vec3 & iBoardOrigin,
                              const TetrisGameLogic & iLogic,
                              ImFont * iTitleFont )
{
  const float alpha = iLogic.GetActionBannerAlpha();
  if ( ( alpha <= 0.01f ) || iLogic.GetActionBannerText().empty() )
    return;

  ImVec2 center;
  if ( !ProjectWorldToScreen(ioScene, iBoardOrigin + Vec3(0.0f, 12.5f, 0.5f), center) )
    return;

  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  std::string text = iLogic.GetActionBannerText();
  if ( ( TetrisGameMode::Versus == iLogic.GetGameMode() ) && ( iLogic.GetLastAttackLinesSent() > 0 ) )
  {
    text += " (+" + std::to_string(iLogic.GetLastAttackLinesSent()) + " ATK)";
  }

  ImFont * font = iTitleFont ? iTitleFont : ImGui::GetFont();
  const float fontSize = font ? ( font->LegacySize * 0.62f ) : 26.0f;
  const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.c_str());
  const ImVec2 textPos(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);

  const ImU32 shadowCol = IM_COL32(0, 0, 0, static_cast<int>(220.f * alpha));
  const ImU32 mainCol   = IM_COL32(255, 240, 95, static_cast<int>(255.f * alpha));

  drawList->AddText(font, fontSize, ImVec2(textPos.x + 2.5f, textPos.y + 2.5f), shadowCol, text.c_str());
  drawList->AddText(font, fontSize, textPos, mainCol, text.c_str());
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
// GetControlDeviceName / GetControlDeviceShortBadge
// ----------------------------------------------------------------------------
const char * TetrisHud::GetControlDeviceName( TetrisControlDevice iDevice )
{
  switch ( iDevice )
  {
    case TetrisControlDevice::KeyboardFull:  return "Full Keyboard (Arrows + WASD)";
    case TetrisControlDevice::KeyboardLeft:  return "Left Keyboard (A/D/S, Space=Drop, G/H/V=Rot, LShift=Hold)";
    case TetrisControlDevice::KeyboardRight: return "Right Keyboard (Arrows, RShift/Enter=Drop, Up/Num1/2=Rot, RCtrl=Hold)";
    case TetrisControlDevice::Gamepad1:      return "Xbox Controller #1 (D-Pad / L-Stick, A/B=Rot, LB/RB=Hold)";
    case TetrisControlDevice::Gamepad2:      return "Xbox Controller #2 (D-Pad / L-Stick, A/B=Rot, LB/RB=Hold)";
    default:                                 return "Unassigned (Connect Controller or Switch Preset)";
  }
}

const char * TetrisHud::GetControlDeviceShortBadge( TetrisControlDevice iDevice )
{
  switch ( iDevice )
  {
    case TetrisControlDevice::KeyboardFull:  return "KEYBOARD";
    case TetrisControlDevice::KeyboardLeft:  return "KB LEFT (WASD)";
    case TetrisControlDevice::KeyboardRight: return "KB RIGHT (ARROWS)";
    case TetrisControlDevice::Gamepad1:      return "GAMEPAD 1";
    case TetrisControlDevice::Gamepad2:      return "GAMEPAD 2";
    default:                                 return "NO DEVICE";
  }
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
  _MpTypeMenuNav.Reset();
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
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.58f);
  const Vec2 panelSize(560.0f, 0.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisMainMenu", "MAIN MENU", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { "SINGLE PLAYER (1P)",   "Marathon 150L, Sprint 40L, or Ultra 2m solo modes",      "1P SOLO",   true, false },
      { "MULTIPLAYER (2P)",     "Play 2-Player Local Split-Screen or Online (LAN / WAN)", "2 PLAYERS", true, false },
      { "QUICK START (1P)",     "Jump straight into solo Marathon mode at Level 1",       "MARATHON",  true, false },
      { "HIGH SCORES & STATS",  "View personal best records and session statistics",      "",          true, false },
      { "OPTIONS",              "Configure handling (DAS/ARR), renderer, audio & inputs", "F1",        true, false },
      { "EXIT TO DESKTOP",      "Close application",                                      "",          true, true  }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _MainMenuNav, iMenuInput, _Theme, _Fonts, 52.0f);
    if ( ( activated >= 0 ) && _Audio && !iMenuInput._Confirm )
      _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

    if ( 0 == activated )
    {
      if ( _SelectedModeIndex > 2 )
        _SelectedModeIndex = 0;
      SetMenuPage(TetrisMenuPage::ModeSelect);
    }
    else if ( 1 == activated )
    {
      SetMenuPage(TetrisMenuPage::MultiplayerTypeSelect);
    }
    else if ( 2 == activated )
    {
      if ( _MpState )
      {
        _MpState->_NumPlayers      = 1;
        _MpState->_IsOnlineMatch   = false;
        _MpState->_Requested1PMode = TetrisGameMode::Marathon;
        _MpState->_StartLevel      = 1;
        _MpState->_RequestStart1P  = true;
      }
      else
      {
        ioLogic.StartNewGame(TetrisGameMode::Marathon, 1, 0);
      }
      ioAppState = TetrisAppState::InGame;
      _ShowInGameOptions = false;
    }
    else if ( 3 == activated )
    {
      SetMenuPage(TetrisMenuPage::HighScores);
    }
    else if ( 4 == activated )
    {
      SetMenuPage(TetrisMenuPage::Options);
    }
    else if ( 5 == activated )
    {
      oRequestExit = true;
    }
  }
  GameMenuUI::EndMenuPanel();

  const std::string leftStatus = std::string("ACTIVE BACKEND: ") + GetRendererModeLabel(iRendererMode) + " | v" + kTetrisGameVersionStr;
  GameMenuUI::DrawFooterBar(
    leftStatus.c_str(),
    "[UP/DOWN] Navigate    [ENTER] Select    [F2/F3/F4] Switch Renderer",
    _Theme,
    _Fonts
  );
}

// ----------------------------------------------------------------------------
// DrawMultiplayerTypeSelectPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawMultiplayerTypeSelectPage( const GameMenuInput & iMenuInput )
{
  if ( iMenuInput._Back )
  {
    SetMenuPage(TetrisMenuPage::MainMenu);
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.56f);
  const Vec2 panelSize(560.0f, 0.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisMpTypeSelect", "2-PLAYER MULTIPLAYER - SELECT MODE", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<GameMenuItem> items =
    {
      { "LOCAL MULTIPLAYER",    "Same-screen 2P split board (Keyboard / Gamepad)",      "SAME PC",   true, false },
      { "ONLINE MULTIPLAYER",   "Host or join a 2P match over LAN or Internet",         "LAN / WAN", true, false },
      { "BACK TO MAIN MENU",    "Return to main title screen",                          "ESC",       true, false }
    };

    const int activated = GameMenuUI::DrawMenuButtonList(items, _MpTypeMenuNav, iMenuInput, _Theme, _Fonts, 56.0f);
    if ( ( activated >= 0 ) && _Audio && !iMenuInput._Confirm )
      _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

    if ( 0 == activated )
    {
      SetMenuPage(TetrisMenuPage::MultiplayerLobby);
    }
    else if ( 1 == activated )
    {
      SetMenuPage(TetrisMenuPage::OnlineLobby);
    }
    else if ( 2 == activated )
    {
      SetMenuPage(TetrisMenuPage::MainMenu);
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(
    "SELECT MULTIPLAYER CONNECTION TYPE",
    "[UP/DOWN] Navigate    [ENTER] Select    [ESC] Back to Main Menu",
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

  if ( GameMenuUI::BeginMenuPanel("##TetrisModeSelect", "SINGLE PLAYER - SELECT MODE", center, panelSize, _Theme, _Fonts) )
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
    if ( GameMenuUI::DrawSelectionCards(cards, _SelectedModeIndex, iMenuInput, _Theme, _Fonts, &cardConfirmed) )
    {
      if ( _Audio && !iMenuInput._NavLeft && !iMenuInput._NavRight )
        _Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
    }

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
      if ( _Audio && !iMenuInput._Back )
        _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      SetMenuPage(TetrisMenuPage::MainMenu);
    }

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton("START GAME (ENTER)", Vec2(btnW, 46.0f), true, _Theme, _Fonts) || cardConfirmed )
    {
      if ( _Audio && !iMenuInput._Confirm )
        _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

      const TetrisGameMode chosenMode = static_cast<TetrisGameMode>(_SelectedModeIndex);
      if ( _MpState )
      {
        _MpState->_NumPlayers      = 1;
        _MpState->_IsOnlineMatch   = false;
        _MpState->_Requested1PMode = chosenMode;
        _MpState->_StartLevel      = ioLogic.GetStartLevel();
        _MpState->_RequestStart1P  = true;
      }
      else
      {
        ioLogic.StartNewGame(chosenMode, ioLogic.GetStartLevel(), 0);
      }
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
// DrawMultiplayerLobbyPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawMultiplayerLobbyPage( TetrisAppState & ioAppState,
                                          const GameMenuInput & iMenuInput )
{
  if ( iMenuInput._Back && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) )
  {
    SetMenuPage(TetrisMenuPage::MultiplayerTypeSelect);
    return;
  }

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.55f);
  const Vec2 panelSize(920.0f, 0.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisMpLobby", "LOCAL MULTIPLAYER LOBBY (2 PLAYERS)", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<GameSelectionCard> cards =
    {
      {
        "VS BATTLE",
        "GUIDELINE GARBAGE WARS",
        "Clear Doubles, Triples, Tetrises, and T-Spins to send garbage lines to your opponent. Counter incoming attacks before locking!",
        "Win: Top Out Opponent",
        Vec4(1.00f, 0.35f, 0.30f, 1.0f)
      },
      {
        "SPRINT RACE",
        "40 LINES HEAD-TO-HEAD",
        "Both players receive the exact same 7-Bag piece sequence. Race to clear 40 lines first without topping out!",
        "Win: First to 40 Lines",
        Vec4(1.00f, 0.65f, 0.10f, 1.0f)
      },
      {
        "SCORE ATTACK",
        "2-MINUTE ULTRA DUEL",
        "Two minutes on the clock with identical piece bags. Outscore your rival using T-Spins, Back-to-Backs, and high combos.",
        "Win: Highest Score in 2:00",
        Vec4(0.75f, 0.35f, 1.00f, 1.0f)
      },
      {
        "MARATHON DUEL",
        "150 LINES ENDURANCE",
        "Parallel 15-level Guideline progression on side-by-side boards. Survive accelerating gravity and reach 150 lines first.",
        "Win: 150 Lines / Survival",
        Vec4(0.00f, 0.90f, 0.95f, 1.0f)
      }
    };

    bool cardConfirmed = false;
    if ( GameMenuUI::DrawSelectionCards(cards, _SelectedMpModeIndex, iMenuInput, _Theme, _Fonts, &cardConfirmed) )
    {
      if ( _Audio && !iMenuInput._NavLeft && !iMenuInput._NavRight )
        _Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
    }

    if ( _MpState )
    {
      if ( 0 == _SelectedMpModeIndex )      _MpState->_MpMode = TetrisGameMode::Versus;
      else if ( 1 == _SelectedMpModeIndex ) _MpState->_MpMode = TetrisGameMode::Sprint;
      else if ( 2 == _SelectedMpModeIndex ) _MpState->_MpMode = TetrisGameMode::Ultra;
      else                                  _MpState->_MpMode = TetrisGameMode::Marathon;
    }

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if ( _MpState )
    {
      ImGui::TextDisabled("Controller & Keyboard Assignment");
      int presetIdx = static_cast<int>(_MpState->_Preset);
      const char * presetNames[] =
      {
        "Auto-Detect (Smart P1 / P2 Assignment)",
        "Gamepad 1 (P1)  +  Gamepad 2 (P2)",
        "Gamepad 1 (P1)  +  Keyboard (P2)",
        "Keyboard (P1)   +  Gamepad 1 (P2)",
        "Split Keyboard: Left WASD (P1) + Right Arrows (P2)"
      };
      ImGui::SetNextItemWidth(420.0f);
      if ( ImGui::Combo("Control Preset", &presetIdx, presetNames, IM_ARRAYSIZE(presetNames)) )
      {
        _MpState->_Preset = static_cast<TetrisMultiplayerPreset>(presetIdx);
      }

      ImGui::SameLine(0.0f, 28.0f);
      const bool gp1Conn = _Gamepad && _Gamepad->IsConnected();
      const bool gp2Conn = _Gamepad2 && _Gamepad2->IsConnected();
      ImGui::TextColored(gp1Conn ? ImVec4(0.25f, 0.95f, 0.55f, 1.0f) : ImVec4(0.55f, 0.60f, 0.68f, 1.0f),
                         "GP1: %s", gp1Conn ? "Connected" : "None");
      ImGui::SameLine(0.0f, 16.0f);
      ImGui::TextColored(gp2Conn ? ImVec4(0.25f, 0.95f, 0.55f, 1.0f) : ImVec4(0.55f, 0.60f, 0.68f, 1.0f),
                         "GP2: %s", gp2Conn ? "Connected" : "None");

      ImGui::Dummy(ImVec2(0.0f, 3.0f));
      ImGui::TextColored(ImVec4(0.00f, 0.90f, 0.96f, 1.0f), "PLAYER 1 (Left Board)  :");
      ImGui::SameLine();
      ImGui::Text("%s", GetControlDeviceName(_MpState->_PlayerDevices[0]));

      ImGui::TextColored(ImVec4(1.00f, 0.60f, 0.20f, 1.0f), "PLAYER 2 (Right Board) :");
      ImGui::SameLine();
      ImGui::Text("%s", GetControlDeviceName(_MpState->_PlayerDevices[1]));

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 4.0f));

      ImGui::SetNextItemWidth(220.0f);
      ImGui::SliderInt("Series Target (Wins)", &_MpState->_TargetRoundWins, 1, 5, "First to %d Wins");

      ImGui::SameLine(0.0f, 32.0f);
      ImGui::SetNextItemWidth(220.0f);
      ImGui::SliderInt("Starting Level", &_MpState->_StartLevel, 1, 15);
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    const float availW = ImGui::GetContentRegionAvail().x;
    const float btnW = ( availW - 16.0f ) * 0.5f;

    if ( GameMenuUI::DrawActionButton("BACK (ESC)", Vec2(btnW, 46.0f), false, _Theme, _Fonts) )
    {
      if ( _Audio && !iMenuInput._Back )
        _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      SetMenuPage(TetrisMenuPage::MultiplayerTypeSelect);
    }

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton("START 2P MATCH (ENTER)", Vec2(btnW, 46.0f), true, _Theme, _Fonts) || cardConfirmed )
    {
      if ( _Audio && !iMenuInput._Confirm )
        _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

      if ( _MpState )
      {
        _MpState->_NumPlayers     = 2;
        _MpState->_IsOnlineMatch  = false;
        _MpState->_RequestStart2P = true;
      }
      ioAppState = TetrisAppState::InGame;
      _ShowInGameOptions = false;
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(
    "2-PLAYER LOCAL SPLIT-SCREEN",
    "[LEFT/RIGHT] Select Mode    [ENTER / START] Launch Match    [ESC / B] Back",
    _Theme,
    _Fonts
  );
}

// ----------------------------------------------------------------------------
// DrawOnlineLobbyPage
// ----------------------------------------------------------------------------
void TetrisHud::DrawOnlineLobbyPage( TetrisAppState & ioAppState,
                                     const GameMenuInput & iMenuInput )
{
  (void)ioAppState;
  if ( !_MpState )
    return;

  if ( iMenuInput._Back && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId) && !ImGui::IsAnyItemActive() )
  {
    SetMenuPage(TetrisMenuPage::MultiplayerTypeSelect);
    return;
  }

  if ( iMenuInput._PrevTab && !ImGui::IsAnyItemActive() )
    _MpState->_OnlineLobbyTab = 0;
  if ( iMenuInput._NextTab && !ImGui::IsAnyItemActive() )
    _MpState->_OnlineLobbyTab = 1;

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.55f);
  const Vec2 panelSize(920.0f, 0.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisOnlineLobby", "ONLINE 2-PLAYER MULTIPLAYER (LAN & INTERNET)", center, panelSize, _Theme, _Fonts) )
  {
    // 1. Top Identity & Live Connection Bar
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("Your Nickname", _MpState->_LocalPlayerName, sizeof(_MpState->_LocalPlayerName));
    if ( _MpState->_LocalPlayerName[0] == '\0' )
      std::snprintf(_MpState->_LocalPlayerName, sizeof(_MpState->_LocalPlayerName), "PLAYER 1");

    ImGui::SameLine(0.0f, 20.0f);
    const NetState netState = _NetSession ? _NetSession->GetState() : NetState::Disconnected;
    if ( NetState::Connected == netState )
    {
      const std::string remoteVer = _MpState->_RemoteGameVersion.empty() ? "" : ( " [v" + _MpState->_RemoteGameVersion + "]" );
      ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f),
                         "STATUS: Connected to %s%s (%s)  |  PING: %u ms",
                         _MpState->_RemotePlayerName.c_str(),
                         remoteVer.c_str(),
                         _NetSession->GetPeerAddressString().c_str(),
                         _NetSession->GetPingMs());
    }
    else if ( NetState::Hosting == netState )
    {
      ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f),
                         "STATUS: Hosting on %s:%u  (Waiting for Challenger...)",
                         _NetSession ? _NetSession->GetLocalIPv4Address().c_str() : "127.0.0.1",
                         _NetSession ? _NetSession->GetBoundPort() : static_cast<unsigned int>(_MpState->_OnlinePort));
    }
    else if ( NetState::Connecting == netState )
    {
      ImGui::TextColored(ImVec4(0.25f, 0.85f, 1.00f, 1.0f),
                         "STATUS: Connecting to %s:%d...",
                         _MpState->_JoinAddressBuf, _MpState->_OnlinePort);
    }
    else
    {
      ImGui::TextDisabled("STATUS: Idle (Host a Room or Join a Room below)");
    }

    // Version Tag on far right
    const std::string verTag = std::string("v") + kTetrisGameVersionStr + " (NetProto " + std::to_string(kTetrisProtocolVersion) + ")";
    const float verTagW = ImGui::CalcTextSize(verTag.c_str()).x;
    ImGui::SameLine(std::max(0.0f, ImGui::GetWindowWidth() - verTagW - 24.0f));
    ImGui::TextDisabled("%s", verTag.c_str());

    // Version Incompatibility Alert Banner
    if ( _MpState->_VersionMismatchError )
    {
      ImGui::Dummy(ImVec2(0.0f, 4.0f));
      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.35f, 0.08f, 0.08f, 0.95f));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.00f, 0.35f, 0.30f, 1.0f));
      ImGui::BeginChild("##VersionMismatchAlert", ImVec2(0.0f, 52.0f), true, ImGuiWindowFlags_NoScrollbar);
      {
        ImGui::SetCursorPos(ImVec2(14.0f, 8.0f));
        ImGui::TextColored(ImVec4(1.00f, 0.35f, 0.30f, 1.0f), "VERSION INCOMPATIBILITY DETECTED");
        ImGui::SetCursorPos(ImVec2(14.0f, 26.0f));
        ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.85f, 1.0f), "%s", _MpState->_VersionMismatchMessage.c_str());

        const float bannerW = ImGui::GetWindowWidth();
        ImGui::SetCursorPos(ImVec2(std::max(14.0f, bannerW - 100.0f), 12.0f));
        if ( ImGui::Button("DISMISS", ImVec2(86.0f, 28.0f)) )
        {
          _MpState->_VersionMismatchError = false;
          _MpState->_VersionMismatchMessage.clear();
        }
      }
      ImGui::EndChild();
      ImGui::PopStyleColor(2);
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    const std::vector<const char *> lobbyTabs = { "HOST MATCH", "JOIN MATCH (LAN & DIRECT IP)" };
    if ( GameMenuUI::DrawTabBar(lobbyTabs, _MpState->_OnlineLobbyTab, _Theme, _Fonts) )
    {
      if ( _Audio )
        _Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    if ( 0 == _MpState->_OnlineLobbyTab )
    {
      // --- HOST MATCH TAB ---
      const std::vector<GameSelectionCard> cards =
      {
        {
          "VS BATTLE",
          "GUIDELINE GARBAGE WARS",
          "Clear Doubles, Triples, Tetrises, and T-Spins to send garbage lines to your opponent. Counter incoming attacks before locking!",
          "Win: Top Out Opponent",
          Vec4(1.00f, 0.35f, 0.30f, 1.0f)
        },
        {
          "SPRINT RACE",
          "40 LINES HEAD-TO-HEAD",
          "Both players receive the exact same 7-Bag piece sequence. Race to clear 40 lines first without topping out!",
          "Win: First to 40 Lines",
          Vec4(1.00f, 0.65f, 0.10f, 1.0f)
        },
        {
          "SCORE ATTACK",
          "2-MINUTE ULTRA DUEL",
          "Two minutes on the clock with identical piece bags. Outscore your rival using T-Spins, Back-to-Backs, and high combos.",
          "Win: Highest Score in 2:00",
          Vec4(0.75f, 0.35f, 1.00f, 1.0f)
        },
        {
          "MARATHON DUEL",
          "150 LINES ENDURANCE",
          "Parallel 15-level Guideline progression on side-by-side boards. Survive accelerating gravity and reach 150 lines first.",
          "Win: 150 Lines / Survival",
          Vec4(0.00f, 0.90f, 0.95f, 1.0f)
        }
      };

      bool cardConfirmed = false;
      GameMenuInput navInput = iMenuInput;
      if ( ImGui::IsAnyItemActive() )
      {
        navInput._NavLeft  = false;
        navInput._NavRight = false;
        navInput._Confirm  = false;
      }

      if ( GameMenuUI::DrawSelectionCards(cards, _SelectedMpModeIndex, navInput, _Theme, _Fonts, &cardConfirmed) )
      {
        if ( _Audio && !navInput._NavLeft && !navInput._NavRight )
          _Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
      }

      if ( 0 == _SelectedMpModeIndex )      _MpState->_MpMode = TetrisGameMode::Versus;
      else if ( 1 == _SelectedMpModeIndex ) _MpState->_MpMode = TetrisGameMode::Sprint;
      else if ( 2 == _SelectedMpModeIndex ) _MpState->_MpMode = TetrisGameMode::Ultra;
      else                                  _MpState->_MpMode = TetrisGameMode::Marathon;

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 4.0f));

      ImGui::SetNextItemWidth(200.0f);
      ImGui::SliderInt("Series Target", &_MpState->_TargetRoundWins, 1, 5, "First to %d Wins");

      ImGui::SameLine(0.0f, 24.0f);
      ImGui::SetNextItemWidth(180.0f);
      ImGui::SliderInt("Start Level", &_MpState->_StartLevel, 1, 15);

      ImGui::SameLine(0.0f, 24.0f);
      ImGui::SetNextItemWidth(110.0f);
      if ( ImGui::InputInt("UDP Port", &_MpState->_OnlinePort, 0, 0) )
        _MpState->_OnlinePort = MathUtil::Clamp(_MpState->_OnlinePort, 1024, 65535);

      ImGui::Dummy(ImVec2(0.0f, 6.0f));

      const bool isHostRole = _NetSession && ( _NetSession->GetRole() == NetRole::Host );
      const bool peerReady  = isHostRole && ( NetState::Connected == netState );

      if ( !isHostRole )
      {
        ImGui::Checkbox("Automatic port forwarding (UPnP)", &_MpState->_EnableUpnp);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        if ( GameMenuUI::DrawActionButton("OPEN ROOM & START HOSTING", Vec2(320.0f, 42.0f), true, _Theme, _Fonts) )
        {
          if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
          _MpState->_RequestHostOnline = true;
        }
        ImGui::SameLine(0.0f, 18.0f);
        ImGui::TextDisabled("Broadcasts automatically on LAN & accepts direct IP connections.");
      }
      else
      {
        if ( GameMenuUI::DrawActionButton("STOP HOSTING", Vec2(220.0f, 42.0f), false, _Theme, _Fonts) )
        {
          if ( _Audio ) _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
          _MpState->_RequestStopHost = true;
        }
        ImGui::SameLine(0.0f, 18.0f);
        if ( peerReady )
        {
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f),
                             "CHALLENGER READY: %s  (Ping: %u ms) - Ready to Launch!",
                             _MpState->_RemotePlayerName.c_str(),
                             _NetSession->GetPingMs());
        }
        else
        {
          ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f),
                             "Broadcasting LAN Room on %s:%u — waiting for opponent...",
                             _NetSession->GetLocalIPv4Address().c_str(),
                             _NetSession->GetBoundPort());
        }

        // --- Prominent Room Code Card ---
        const UpnpStatusInfo & upnp = _MpState->_UpnpInfo;
        const bool hasPublicIp = RoomCode::IsPublicInternetIP( upnp._ExternalIP ) && ( upnp._RoomCode.size() >= 8 );
        const std::string lanIp = _NetSession->GetLocalIPv4Address();
        const std::string lanRoomCode = RoomCode::Encode( lanIp, static_cast<uint16_t>( _NetSession->GetBoundPort() ) );

        std::string activeIp;
        std::string activeRoomCode;
        bool isInternetCode = false;
        bool isResolving = false;

        if ( hasPublicIp )
        {
          activeIp       = upnp._ExternalIP;
          activeRoomCode = upnp._RoomCode;
          isInternetCode = true;
        }
        else if ( upnp._State == UpnpState::Discovering || upnp._State == UpnpState::Mapping )
        {
          activeIp       = "Resolving...";
          activeRoomCode = "RESOLVING...";
          isResolving    = true;
        }
        else
        {
          activeIp       = lanIp;
          activeRoomCode = lanRoomCode;
          isInternetCode = false;
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::BeginChild("##RoomCodeCard", ImVec2(0.0f, 96.0f), true, ImGuiWindowFlags_NoScrollbar);
        {
          ImGui::SetCursorPos(ImVec2(16.0f, 8.0f));
          if ( isInternetCode )
          {
            ImGui::TextColored(ImVec4(0.00f, 0.95f, 1.00f, 1.0f), "ONLINE ROOM CODE (INTERNET PLAY):");
          }
          else if ( isResolving )
          {
            ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f), "RESOLVING PUBLIC INTERNET IP (STUN)...");
          }
          else
          {
            ImGui::TextColored(ImVec4(1.00f, 0.65f, 0.25f, 1.0f), "LOCAL ROOM CODE (LAN / DIRECT IP ONLY - NO INTERNET):");
          }

          ImGui::SetCursorPos(ImVec2(16.0f, 28.0f));
          ImFont * bigFont = _Fonts._MonoFont ? _Fonts._MonoFont : ( _Fonts._HeaderFont ? _Fonts._HeaderFont : ImGui::GetFont() );
          ImGui::PushFont(bigFont);
          ImGui::TextColored(isResolving ? ImVec4(0.70f, 0.70f, 0.70f, 1.0f) : ImVec4(0.00f, 0.95f, 1.00f, 1.0f),
                             "%s", activeRoomCode.c_str());
          ImGui::PopFont();

          ImGui::SameLine(0.0f, 14.0f);
          ImGui::SetCursorPosY(34.0f);
          ImGui::TextDisabled("(%s:%u)", activeIp.c_str(), static_cast<unsigned int>(_NetSession->GetBoundPort()));

          const float cardW = ImGui::GetWindowWidth();
          ImGui::SetCursorPos(ImVec2(std::max(16.0f, cardW - 350.0f), 22.0f));
          ImGui::BeginDisabled(isResolving);
          if ( ImGui::Button("COPY ROOM CODE", ImVec2(160.0f, 36.0f)) )
          {
            if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
            ImGui::SetClipboardText(activeRoomCode.c_str());
            _MpState->_CopiedFeedbackTimer = 2.5f;
          }
          ImGui::SameLine(0.0f, 10.0f);
          if ( ImGui::Button("COPY IP:PORT", ImVec2(130.0f, 36.0f)) )
          {
            if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
            char ipPort[96] = {};
            std::snprintf(ipPort, sizeof(ipPort), "%s:%u", activeIp.c_str(), static_cast<unsigned int>(_NetSession->GetBoundPort()));
            ImGui::SetClipboardText(ipPort);
            _MpState->_CopiedFeedbackTimer = 2.5f;
          }
          ImGui::EndDisabled();

          // Bottom helper row
          ImGui::SetCursorPos(ImVec2(16.0f, 68.0f));
          if ( isInternetCode )
          {
            ImGui::TextDisabled("Same Wi-Fi? Local LAN Code: %s (%s:%u)",
                                lanRoomCode.c_str(), lanIp.c_str(), static_cast<unsigned int>(_NetSession->GetBoundPort()));
          }
          else if ( isResolving )
          {
            ImGui::TextDisabled("Querying STUN (stun.l.google.com) to find your globally routable public address...");
          }
          else
          {
            ImGui::TextDisabled("Share this with players on the same local network / Wi-Fi, or setup port forwarding.");
          }

          if ( _MpState->_CopiedFeedbackTimer > 0.0f )
          {
            ImGui::SetCursorPos(ImVec2(std::max(16.0f, cardW - 350.0f), 65.0f));
            ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "Copied to clipboard!");
          }
        }
        ImGui::EndChild();

        // --- UPnP Status Banner ---
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if ( !_NetSession->IsUpnpEnabled() )
        {
          ImGui::TextDisabled("UPnP disabled: Internet players need a manual port forward (UDP %u) or a VPN.",
                              _NetSession->GetBoundPort());
        }
        else if ( UpnpState::Discovering == upnp._State || UpnpState::Mapping == upnp._State )
        {
          ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f), "[UPnP] %s", upnp._StatusMessage.c_str());
        }
        else if ( UpnpState::Success == upnp._State )
        {
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f),
                             "[UPnP] UDP port %u open | %s",
                             static_cast<unsigned int>(upnp._ExternalPort),
                             upnp._StatusMessage.c_str());
        }
        else if ( UpnpState::NotAvailable == upnp._State || UpnpState::Failed == upnp._State )
        {
          ImGui::TextColored(ImVec4(1.00f, 0.55f, 0.25f, 1.0f),
                             "[UPnP] %s - manual port forward (UDP %u) or VPN required for Internet play.",
                             upnp._StatusMessage.c_str(), _NetSession->GetBoundPort());
        }
      }

      ImGui::Dummy(ImVec2(0.0f, 10.0f));
      const float availW = ImGui::GetContentRegionAvail().x;
      const float btnW = ( availW - 16.0f ) * 0.5f;

      if ( GameMenuUI::DrawActionButton("BACK (ESC)", Vec2(btnW, 46.0f), false, _Theme, _Fonts) )
      {
        if ( _Audio ) _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
        SetMenuPage(TetrisMenuPage::MultiplayerTypeSelect);
      }

      ImGui::SameLine(0.0f, 16.0f);
      ImGui::BeginDisabled(!peerReady);
      if ( GameMenuUI::DrawActionButton(peerReady ? "LAUNCH ONLINE MATCH (ENTER)" : "WAITING FOR OPPONENT TO JOIN...", Vec2(btnW, 46.0f), peerReady, _Theme, _Fonts) ||
           ( peerReady && cardConfirmed ) )
      {
        if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
        _MpState->_RequestStartOnlineMatch = true;
      }
      ImGui::EndDisabled();
    }
    else
    {
      // --- JOIN MATCH TAB ---
      const bool isClientOrConnecting = _NetSession && ( _NetSession->GetRole() == NetRole::Client ) &&
                                        ( NetState::Connecting == netState || NetState::Connected == netState );

      // 1. PRIMARY: JOIN VIA ROOM CODE
      ImGui::TextColored(ImVec4(0.00f, 0.95f, 1.00f, 1.0f), "JOIN VIA ROOM CODE");
      ImGui::Separator();
      ImGui::Dummy(ImVec2(0.0f, 4.0f));

      ImGui::BeginChild("##JoinCodeCard", ImVec2(0.0f, 96.0f), true, ImGuiWindowFlags_NoScrollbar);
      {
        ImGui::SetCursorPos(ImVec2(16.0f, 10.0f));
        ImGui::TextColored(ImVec4(0.70f, 0.80f, 0.90f, 1.0f), "Enter or paste the Room Code provided by the host:");

        ImGui::SetCursorPos(ImVec2(16.0f, 30.0f));
        ImGui::SetNextItemWidth(260.0f);
        ImFont * monoFont = _Fonts._MonoFont ? _Fonts._MonoFont : ImGui::GetFont();
        ImGui::PushFont(monoFont);
        ImGui::InputTextWithHint("##RoomCodeInput", "e.g. 92FA-K49B", _MpState->_RoomCodeInputBuf, sizeof(_MpState->_RoomCodeInputBuf));
        ImGui::PopFont();

        ImGui::SameLine(0.0f, 10.0f);
        if ( ImGui::Button("PASTE", ImVec2(80.0f, 32.0f)) )
        {
          const char * clip = ImGui::GetClipboardText();
          if ( clip )
          {
            std::snprintf(_MpState->_RoomCodeInputBuf, sizeof(_MpState->_RoomCodeInputBuf), "%s", clip);
            // Trim whitespace
            char * end = _MpState->_RoomCodeInputBuf + std::strlen(_MpState->_RoomCodeInputBuf) - 1;
            while ( end >= _MpState->_RoomCodeInputBuf && ( *end == ' ' || *end == '\r' || *end == '\n' || *end == '\t' ) )
            {
              *end = '\0';
              --end;
            }
          }
        }

        std::string resolvedIP;
        uint16_t    resolvedPort = 0;
        const bool  hasValidCode = RoomCode::Decode(_MpState->_RoomCodeInputBuf, resolvedIP, resolvedPort);

        ImGui::SameLine(0.0f, 16.0f);
        if ( !isClientOrConnecting )
        {
          ImGui::BeginDisabled(!hasValidCode);
          if ( ImGui::Button("CONNECT VIA ROOM CODE", ImVec2(240.0f, 32.0f)) )
          {
            if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
            std::snprintf(_MpState->_JoinAddressBuf, sizeof(_MpState->_JoinAddressBuf), "%s", resolvedIP.c_str());
            _MpState->_OnlinePort = resolvedPort;
            _MpState->_RequestJoinOnline = true;
          }
          ImGui::EndDisabled();
        }
        else
        {
          if ( ImGui::Button("DISCONNECT", ImVec2(240.0f, 32.0f)) )
          {
            if ( _Audio ) _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
            _MpState->_RequestDisconnectOnline = true;
          }
        }

        // Live validation feedback
        ImGui::SetCursorPos(ImVec2(16.0f, 68.0f));
        if ( hasValidCode )
        {
          const bool isPublic = RoomCode::IsPublicInternetIP( resolvedIP );
          if ( isPublic )
          {
            ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f),
                               "Valid Internet Code -> resolves to %s:%u (Public IP)",
                               resolvedIP.c_str(), static_cast<unsigned int>(resolvedPort));
          }
          else
          {
            ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.20f, 1.0f),
                               "Valid LAN Code -> resolves to %s:%u (Private LAN IP - only works on same Wi-Fi!)",
                               resolvedIP.c_str(), static_cast<unsigned int>(resolvedPort));
          }
        }
        else if ( _MpState->_RoomCodeInputBuf[0] != '\0' )
        {
          ImGui::TextColored(ImVec4(1.00f, 0.55f, 0.25f, 1.0f), "Invalid room code format (check for typos, e.g. 92FA-K49B)");
        }
        else
        {
          ImGui::TextDisabled("Codes are 8 or 11 characters (case-insensitive, hyphens optional).");
        }
      }
      ImGui::EndChild();

      // 2. SECONDARY: AUTOMATIC LAN DISCOVERY
      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      ImGui::TextColored(ImVec4(0.00f, 0.90f, 0.96f, 1.0f), "LOCAL NETWORK (LAN) DISCOVERY");
      ImGui::Separator();

      ImGui::BeginChild("##LanRoomsList", ImVec2(0.0f, 110.0f), true);
      const std::vector<LanRoomInfo> emptyRooms;
      const std::vector<LanRoomInfo> & rooms = _NetSession ? _NetSession->GetDiscoveredLanRooms() : emptyRooms;

      if ( rooms.empty() )
      {
        ImGui::Dummy(ImVec2(0.0f, 24.0f));
        const char * scanMsg = "Scanning local network (and localhost 127.0.0.1) for active rooms...";
        const float tw = ImGui::CalcTextSize(scanMsg).x;
        ImGui::SetCursorPosX(std::max(12.0f, ( ImGui::GetContentRegionAvail().x - tw ) * 0.5f));
        ImGui::TextDisabled("%s", scanMsg);
      }
      else
      {
        for ( size_t i = 0; i < rooms.size(); ++i )
        {
          const LanRoomInfo & r = rooms[i];
          ImGui::PushID(static_cast<int>(i));
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "[LAN ROOM]");
          ImGui::SameLine();
          ImGui::Text("Host: %s", r._HostName.c_str());
          ImGui::SameLine(280.0f);
          ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f), "%s", r._RoomInfo.c_str());
          ImGui::SameLine(560.0f);
          ImGui::TextDisabled("%s:%u", r._Address.c_str(), static_cast<unsigned int>(r._GamePort));
          ImGui::SameLine(ImGui::GetContentRegionAvail().x - 125.0f);
          if ( ImGui::Button("JOIN ROOM", ImVec2(120.0f, 26.0f)) )
          {
            if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
            std::snprintf(_MpState->_JoinAddressBuf, sizeof(_MpState->_JoinAddressBuf), "%s", r._Address.c_str());
            _MpState->_OnlinePort        = static_cast<int>(r._GamePort);
            _MpState->_RequestJoinOnline = true;
          }
          ImGui::Separator();
          ImGui::PopID();
        }
      }
      ImGui::EndChild();

      // 3. TERTIARY: DIRECT IP / PORT
      ImGui::Dummy(ImVec2(0.0f, 4.0f));
      ImGui::TextColored(ImVec4(0.70f, 0.80f, 0.90f, 1.0f), "Direct IP / VPN Connection:");
      ImGui::SetNextItemWidth(260.0f);
      ImGui::InputText("##DirectIP", _MpState->_JoinAddressBuf, sizeof(_MpState->_JoinAddressBuf));

      ImGui::SameLine(0.0f, 14.0f);
      ImGui::SetNextItemWidth(100.0f);
      if ( ImGui::InputInt("Port##JoinPort", &_MpState->_OnlinePort, 0, 0) )
        _MpState->_OnlinePort = MathUtil::Clamp(_MpState->_OnlinePort, 1024, 65535);

      ImGui::SameLine(0.0f, 16.0f);
      if ( !isClientOrConnecting )
      {
        if ( ImGui::Button("DIRECT CONNECT", ImVec2(160.0f, 26.0f)) )
        {
          if ( _Audio ) _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
          _MpState->_RequestJoinOnline = true;
        }
      }
      else
      {
        if ( ImGui::Button("DISCONNECT##Direct", ImVec2(160.0f, 26.0f)) )
        {
          if ( _Audio ) _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
          _MpState->_RequestDisconnectOnline = true;
        }
      }

      ImGui::Dummy(ImVec2(0.0f, 6.0f));
      if ( NetState::Connected == netState && _NetSession && _NetSession->GetRole() == NetRole::Client )
      {
        ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f),
                           "CONNECTED TO HOST '%s' (Ping: %u ms) — Waiting for Host to launch match...",
                           _MpState->_RemotePlayerName.c_str(),
                           _NetSession->GetPingMs());
      }
      else if ( NetState::Connecting == netState && _NetSession && _NetSession->GetRole() == NetRole::Client )
      {
        ImGui::TextColored(ImVec4(1.00f, 0.85f, 0.25f, 1.0f),
                           "CONNECTING TO HOST (%s:%u)... Sending UDP handshake across Internet (timeout: 6s)...",
                           _MpState->_JoinAddressBuf, static_cast<unsigned int>(_MpState->_OnlinePort));
      }
      else
      {
        ImGui::TextDisabled("Tip: In Online matches, your board is always on the LEFT (Full Keyboard + Gamepad 1).");
      }

      ImGui::Dummy(ImVec2(0.0f, 10.0f));
      if ( GameMenuUI::DrawActionButton("BACK (ESC)", Vec2(260.0f, 44.0f), false, _Theme, _Fonts) )
      {
        if ( _Audio ) _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
        SetMenuPage(TetrisMenuPage::MultiplayerTypeSelect);
      }
    }
  }
  GameMenuUI::EndMenuPanel();

  GameMenuUI::DrawFooterBar(
    "ONLINE 2-PLAYER LOBBY (ENET UDP)",
    "[LB / RB] Switch Host/Join Tab    [ENTER] Action    [ESC] Back to Menu",
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
      if ( _Audio && !iMenuInput._Back )
        _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      SetMenuPage(TetrisMenuPage::MainMenu);
    }

    ImGui::SameLine(0.0f, 16.0f);

    if ( GameMenuUI::DrawActionButton("RESET SESSION STATS", Vec2(btnW, 44.0f), false, _Theme, _Fonts) )
    {
      if ( _Audio && !iMenuInput._Confirm )
        _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);
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
    _OptionsTabIndex = ( _OptionsTabIndex + 3 ) % 4;
  if ( iMenuInput._NextTab )
    _OptionsTabIndex = ( _OptionsTabIndex + 1 ) % 4;

  const ImGuiIO & io = ImGui::GetIO();
  const Vec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.54f);
  const Vec2 panelSize(780.0f, 510.0f);

  if ( GameMenuUI::BeginMenuPanel("##TetrisOptionsPanel", "OPTIONS & CONFIGURATION", center, panelSize, _Theme, _Fonts) )
  {
    const std::vector<const char *> tabs =
    {
      "GAMEPLAY",
      "GRAPHICS",
      "AUDIO",
      "CONTROLS"
    };
    if ( GameMenuUI::DrawTabBar(tabs, _OptionsTabIndex, _Theme, _Fonts) )
    {
      if ( _Audio && !iMenuInput._PrevTab && !iMenuInput._NextTab )
        _Audio->PlaySound2D("ui_nav", 0.65f, 1.0f);
    }

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

      Vec3 edgeColor = ioSceneMgr.GetBoardEdgeColor();
      float edgeColorArr[3] = { edgeColor.x, edgeColor.y, edgeColor.z };
      if ( ImGui::ColorEdit3("Board Edges / Frame Color", edgeColorArr) )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(edgeColorArr[0], edgeColorArr[1], edgeColorArr[2]));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }

      ImGui::TextDisabled("Edge Presets:");
      ImGui::SameLine();
      if ( ImGui::SmallButton("Steel") )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(0.12f, 0.14f, 0.18f));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }
      ImGui::SameLine();
      if ( ImGui::SmallButton("Cyan") )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(0.0f, 0.85f, 0.95f));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }
      ImGui::SameLine();
      if ( ImGui::SmallButton("Purple") )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(0.72f, 0.25f, 0.95f));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }
      ImGui::SameLine();
      if ( ImGui::SmallButton("Gold") )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(0.95f, 0.75f, 0.15f));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }
      ImGui::SameLine();
      if ( ImGui::SmallButton("Crimson") )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(0.90f, 0.18f, 0.22f));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }
      ImGui::SameLine();
      if ( ImGui::SmallButton("White") )
      {
        ioSceneMgr.SetBoardEdgeColor(ioScene, Vec3(0.95f, 0.95f, 0.95f));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
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
      if ( _Audio )
      {
        GameMenuUI::DrawAudioSettings( *_Audio, _Theme, _Fonts );
      }
      else
      {
        ImGui::TextDisabled("Audio engine not available.");
      }
    }
    else if ( 3 == _OptionsTabIndex )
    {
      if ( _Gamepad )
      {
        ImGui::TextDisabled("Xbox / XInput Controller Status & Tuning");
        ImGui::Separator();

        if ( _Gamepad->IsConnected() )
        {
          ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "Controller #1 Connected: %s (GLFW Slot %d)",
                             _Gamepad->GetName().c_str(), _Gamepad->GetJoystickId() + 1);
        }
        else
        {
          ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "Controller #1: Not Connected");
        }

        if ( _Gamepad2 )
        {
          if ( _Gamepad2->IsConnected() )
          {
            ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f), "Controller #2 Connected: %s (GLFW Slot %d)",
                               _Gamepad2->GetName().c_str(), _Gamepad2->GetJoystickId() + 1);
          }
          else
          {
            ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "Controller #2: Not Connected");
          }
        }

        bool stickHardDrop = _Gamepad->GetAllowStickHardDrop();
        if ( ImGui::Checkbox("Allow Left Stick Up for Hard Drop (Strict Vertical Gate)", &stickHardDrop) )
        {
          _Gamepad->SetAllowStickHardDrop(stickHardDrop);
          if ( _Gamepad2 )
            _Gamepad2->SetAllowStickHardDrop(stickHardDrop);
        }

        ImGui::SameLine(0.0f, 24.0f);
        float deadzone = _Gamepad->GetStickDeadzone();
        ImGui::SetNextItemWidth(160.0f);
        if ( ImGui::SliderFloat("Stick Deadzone", &deadzone, 0.05f, 0.45f, "%.2f") )
        {
          _Gamepad->SetStickDeadzone(deadzone);
          if ( _Gamepad2 )
            _Gamepad2->SetStickDeadzone(deadzone);
        }

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
      }

      const std::vector<GameKeybindEntry> bindings =
      {
        { "1P / 2P MOVEMENT",   "Move Left (P1 Left / P2 Right)",    "A",           "LEFT ARROW",        "D-PAD / L-STICK LEFT"  },
        { "1P / 2P MOVEMENT",   "Move Right (P1 Left / P2 Right)",   "D",           "RIGHT ARROW",       "D-PAD / L-STICK RIGHT" },
        { "1P / 2P MOVEMENT",   "Soft Drop (P1 Left / P2 Right)",    "S",           "DOWN ARROW",        "D-PAD / L-STICK DOWN"  },
        { "1P / 2P MOVEMENT",   "Hard Drop (P1 Left / P2 Right)",    "SPACE",       "R-SHIFT / ENTER",   "D-PAD UP / L-STICK UP" },
        { "ROTATION & HOLD",    "Rotate Clockwise (90 CW)",          "H / X",       "UP / NUM 2 / /",    "B BUTTON"              },
        { "ROTATION & HOLD",    "Rotate Counter-Clockwise",          "G / Z",       "NUM 1 / . / R-ALT", "A BUTTON"              },
        { "ROTATION & HOLD",    "Rotate 180 Degrees",                "W / Y",       "NUM 3 / ,",         "X / Y BUTTON"          },
        { "ROTATION & HOLD",    "Hold Active Piece",                 "L-SHIFT / C", "R-CTRL / NUM 0",    "LB / RB / LT / RT"     },
        { "SYSTEM & RENDERERS", "Pause / Back",                      "ESCAPE",      "P",                 "START (MENU) / B"      },
        { "SYSTEM & RENDERERS", "Quick Restart Match",               "R",           "",                  "BACK (VIEW)"           },
        { "SYSTEM & RENDERERS", "Options Menu",                      "F1",          "",                  "START -> OPTIONS"      },
        { "SYSTEM & RENDERERS", "Switch Renderer (Def / PT / SW)",   "F2 / F3 / F4","",                  ""                      },
        { "SYSTEM & RENDERERS", "Cycle / Orbit 3D Camera",           "V / MOUSE",   "",                  "R3 / RIGHT STICK"      }
      };

      GameMenuUI::DrawKeybindingTable(bindings, _Theme, _Fonts);
    }

    ImGui::EndChild();

    ImGui::Separator();
    if ( GameMenuUI::DrawActionButton("BACK (ESC / B)", Vec2(220.0f, 42.0f), true, _Theme, _Fonts) )
    {
      if ( _Audio && !iMenuInput._Back )
        _Audio->PlaySound2D("ui_back", 0.8f, 1.0f);
      oBackRequested = true;
    }
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

  if ( TetrisMenuPage::MainMenu == _MenuPage )
  {
    DrawMainMenuPage(ioAppState, ioLogic, ioRendererMode, iMenuInput, oRequestExit);
  }
  else if ( TetrisMenuPage::ModeSelect == _MenuPage )
  {
    DrawModeSelectPage(ioAppState, ioLogic, iMenuInput);
  }
  else if ( TetrisMenuPage::MultiplayerTypeSelect == _MenuPage )
  {
    DrawMultiplayerTypeSelectPage(iMenuInput);
  }
  else if ( TetrisMenuPage::MultiplayerLobby == _MenuPage )
  {
    DrawMultiplayerLobbyPage(ioAppState, iMenuInput);
  }
  else if ( TetrisMenuPage::OnlineLobby == _MenuPage )
  {
    DrawOnlineLobbyPage(ioAppState, iMenuInput);
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
// DrawMultiplayerGameplayOverlay
// ----------------------------------------------------------------------------
void TetrisHud::DrawMultiplayerGameplayOverlay( Scene & ioScene )
{
  if ( !_Logics || !_MpState )
    return;

  const Vec3 originP1 = TetrisSceneManager::GetBoardOrigin(0, 2);
  const Vec3 originP2 = TetrisSceneManager::GetBoardOrigin(1, 2);

  const char * p1Name = _MpState->_IsOnlineMatch ? _MpState->_LocalPlayerName : "PLAYER 1";
  const char * p2Name = _MpState->_IsOnlineMatch ? _MpState->_RemotePlayerName.c_str() : "PLAYER 2";

  // 1. 3D-Projected Board Headers, Hold/Next Labels, Garbage Danger Meters & Action Banners
  DrawFrameLabel3DColored(ioScene, originP1 + Vec3( 0.0f, 21.35f, -0.2f), p1Name, _Fonts._HeaderFont, IM_COL32(0, 228, 245, 220), IM_COL32(180, 248, 255, 255));
  DrawFrameLabel3DColored(ioScene, originP1 + Vec3(-8.0f, 20.85f, -0.2f), "HOLD", _Fonts._HeaderFont, IM_COL32(0, 228, 245, 150));
  DrawFrameLabel3DColored(ioScene, originP1 + Vec3( 8.0f, 20.85f, -0.2f), "NEXT", _Fonts._HeaderFont, IM_COL32(0, 228, 245, 150));

  DrawFrameLabel3DColored(ioScene, originP2 + Vec3( 0.0f, 21.35f, -0.2f), p2Name, _Fonts._HeaderFont, IM_COL32(255, 150, 45, 220), IM_COL32(255, 230, 190, 255));
  DrawFrameLabel3DColored(ioScene, originP2 + Vec3(-8.0f, 20.85f, -0.2f), "HOLD", _Fonts._HeaderFont, IM_COL32(255, 150, 45, 150));
  DrawFrameLabel3DColored(ioScene, originP2 + Vec3( 8.0f, 20.85f, -0.2f), "NEXT", _Fonts._HeaderFont, IM_COL32(255, 150, 45, 150));

  DrawGarbageMeter3D(ioScene, originP1, _Logics[0].GetPendingGarbage(), _Fonts._HeaderFont);
  DrawGarbageMeter3D(ioScene, originP2, _Logics[1].GetPendingGarbage(), _Fonts._HeaderFont);

  DrawBoardActionBanner3D(ioScene, originP1, _Logics[0], _Fonts._TitleFont);
  DrawBoardActionBanner3D(ioScene, originP2, _Logics[1], _Fonts._TitleFont);

  ImGuiIO & io = ImGui::GetIO();
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                               | ImGuiWindowFlags_AlwaysAutoResize
                               | ImGuiWindowFlags_NoSavedSettings
                               | ImGuiWindowFlags_NoFocusOnAppearing
                               | ImGuiWindowFlags_NoNav
                               | ImGuiWindowFlags_NoMove;

  GameMenuUI::PushThemeStyle(_Theme);

  const float pad = 16.0f;

  // 2. Top-Center VS Scoreboard
  const float scoreW = 360.0f;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, pad), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowSize(ImVec2(scoreW, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.84f);

  if ( ImGui::Begin("##TetrisVsScoreboard", nullptr, flags) )
  {
    const TetrisGameMode mode = _Logics[0].GetGameMode();
    const char * modeTitle = _MpState->_IsOnlineMatch ? "ONLINE VS GARBAGE BATTLE" : "VS GARBAGE BATTLE";
    if ( TetrisGameMode::Sprint == mode )        modeTitle = _MpState->_IsOnlineMatch ? "ONLINE SPRINT 40L RACE" : "SPRINT 40L RACE";
    else if ( TetrisGameMode::Ultra == mode )    modeTitle = _MpState->_IsOnlineMatch ? "ONLINE ULTRA 2:00 DUEL" : "ULTRA 2:00 DUEL";
    else if ( TetrisGameMode::Marathon == mode ) modeTitle = _MpState->_IsOnlineMatch ? "ONLINE MARATHON DUEL" : "MARATHON 150L DUEL";

    const float titleW = ImGui::CalcTextSize(modeTitle).x;
    ImGui::SetCursorPosX(std::max(8.0f, ( scoreW - titleW ) * 0.5f));
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "%s", modeTitle);
    ImGui::Separator();

    char scoreBuf[128];
    std::snprintf(scoreBuf, sizeof(scoreBuf), "%s  [ %d ]   -   [ %d ]  %s",
                  p1Name, _MpState->_RoundWins[0], _MpState->_RoundWins[1], p2Name);
    if ( _Fonts._HeaderFont ) ImGui::PushFont(_Fonts._HeaderFont);
    const float scW = ImGui::CalcTextSize(scoreBuf).x;
    ImGui::SetCursorPosX(std::max(8.0f, ( scoreW - scW ) * 0.5f));
    ImGui::Text("%s", scoreBuf);
    if ( _Fonts._HeaderFont ) ImGui::PopFont();

    std::string timeStr = ( TetrisGameMode::Ultra == mode )
                          ? ( "TIME LEFT: " + FormatTime(_Logics[0].GetRemainingTime()) )
                          : ( "TIME: " + FormatTime(_Logics[0].GetElapsedTime()) + "  |  FIRST TO " + std::to_string(_MpState->_TargetRoundWins) );
    if ( _MpState->_IsOnlineMatch && _NetSession )
    {
      timeStr += "  |  PING: " + std::to_string(_NetSession->GetPingMs()) + " ms";
    }
    const float tmW = ImGui::CalcTextSize(timeStr.c_str()).x;
    ImGui::SetCursorPosX(std::max(8.0f, ( scoreW - tmW ) * 0.5f));
    ImGui::TextDisabled("%s", timeStr.c_str());
  }
  ImGui::End();

  // 3. Left Telemetry Panel (Local Player / Player 1)
  ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(230.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.78f);

  if ( ImGui::Begin("##TetrisP1Telemetry", nullptr, flags) )
  {
    if ( _Fonts._HeaderFont ) ImGui::PushFont(_Fonts._HeaderFont);
    ImGui::TextColored(ImVec4(0.0f, 0.90f, 0.96f, 1.0f), "%s", p1Name);
    if ( _Fonts._HeaderFont ) ImGui::PopFont();
    ImGui::SameLine();
    if ( _MpState->_IsOnlineMatch )
      ImGui::TextDisabled("[YOU]");
    else
      ImGui::TextDisabled("[%s]", GetControlDeviceShortBadge(_MpState->_PlayerDevices[0]));
    ImGui::Separator();

    ImGui::TextDisabled("SCORE");
    if ( _Fonts._MonoFont ) ImGui::PushFont(_Fonts._MonoFont);
    ImGui::Text("%lld", _Logics[0].GetScore());
    if ( _Fonts._MonoFont ) ImGui::PopFont();

    ImGui::Text("LEVEL : %d", _Logics[0].GetLevel());
    if ( TetrisGameMode::Sprint == _Logics[0].GetGameMode() )
      ImGui::Text("LINES : %d / 40", _Logics[0].GetLinesCleared());
    else
      ImGui::Text("LINES : %d", _Logics[0].GetLinesCleared());

    if ( TetrisGameMode::Versus == _Logics[0].GetGameMode() )
    {
      ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.60f, 1.0f), "ATK SENT : %d", _Logics[0].GetTotalAttacksSent());
      if ( _Logics[0].GetPendingGarbage() > 0 )
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "DANGER   : +%d GARBAGE", _Logics[0].GetPendingGarbage());
    }

    if ( _Logics[0].GetComboCount() > 0 )
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "COMBO : %dx", _Logics[0].GetComboCount());
    if ( _Logics[0].IsBackToBackActive() )
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.95f, 1.0f), "[BACK-TO-BACK]");
  }
  ImGui::End();

  // 4. Right Telemetry Panel (Remote Opponent / Player 2)
  const float rightW = 230.0f;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - rightW - pad, pad), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(rightW, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.78f);

  if ( ImGui::Begin("##TetrisP2Telemetry", nullptr, flags) )
  {
    if ( _Fonts._HeaderFont ) ImGui::PushFont(_Fonts._HeaderFont);
    ImGui::TextColored(ImVec4(1.0f, 0.60f, 0.20f, 1.0f), "%s", p2Name);
    if ( _Fonts._HeaderFont ) ImGui::PopFont();
    ImGui::SameLine();
    if ( _MpState->_IsOnlineMatch )
      ImGui::TextDisabled("[ONLINE]");
    else
      ImGui::TextDisabled("[%s]", GetControlDeviceShortBadge(_MpState->_PlayerDevices[1]));
    ImGui::Separator();

    ImGui::TextDisabled("SCORE");
    if ( _Fonts._MonoFont ) ImGui::PushFont(_Fonts._MonoFont);
    ImGui::Text("%lld", _Logics[1].GetScore());
    if ( _Fonts._MonoFont ) ImGui::PopFont();

    ImGui::Text("LEVEL : %d", _Logics[1].GetLevel());
    if ( TetrisGameMode::Sprint == _Logics[1].GetGameMode() )
      ImGui::Text("LINES : %d / 40", _Logics[1].GetLinesCleared());
    else
      ImGui::Text("LINES : %d", _Logics[1].GetLinesCleared());

    if ( TetrisGameMode::Versus == _Logics[1].GetGameMode() )
    {
      ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.60f, 1.0f), "ATK SENT : %d", _Logics[1].GetTotalAttacksSent());
      if ( _Logics[1].GetPendingGarbage() > 0 )
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "DANGER   : +%d GARBAGE", _Logics[1].GetPendingGarbage());
    }

    if ( _Logics[1].GetComboCount() > 0 )
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "COMBO : %dx", _Logics[1].GetComboCount());
    if ( _Logics[1].IsBackToBackActive() )
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.95f, 1.0f), "[BACK-TO-BACK]");
  }
  ImGui::End();

  GameMenuUI::PopThemeStyle();
}

// ----------------------------------------------------------------------------
// DrawGameplayOverlay
// ----------------------------------------------------------------------------
void TetrisHud::DrawGameplayOverlay( const TetrisGameLogic & iLogic, Scene & ioScene )
{
  if ( _MpState && ( 2 == _MpState->_NumPlayers ) )
  {
    DrawMultiplayerGameplayOverlay(ioScene);
    return;
  }

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
  if ( _MpState && ( 2 == _MpState->_NumPlayers ) )
    return;

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

  const bool is2P = _MpState && ( 2 == _MpState->_NumPlayers ) && _Logics;
  const bool isOnline = is2P && _MpState->_IsOnlineMatch;

  // 0. Opponent Disconnected Modal (Online Only)
  if ( isOnline && _MpState->_PeerDisconnectedModal )
  {
    if ( GameMenuUI::BeginMenuPanel("##TetrisPeerDiscModal", "OPPONENT DISCONNECTED", center, Vec2(500.0f, 0.0f), _Theme, _Fonts) )
    {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                         "Connection with '%s' was closed.", _MpState->_RemotePlayerName.c_str());
      ImGui::Dummy(ImVec2(0.0f, 10.0f));
      if ( GameMenuUI::DrawActionButton("RETURN TO ONLINE LOBBY (ENTER)", Vec2(450.0f, 48.0f), true, _Theme, _Fonts) ||
           iMenuInput._Confirm || iMenuInput._Back )
      {
        _MpState->_PeerDisconnectedModal = false;
        _MpState->_ShowOnlineInGameMenu  = false;
        _MpState->_IsOnlineMatch         = false;
        _MpState->_NumPlayers            = 1;
        SetMenuPage(TetrisMenuPage::OnlineLobby);
        ioAppState = TetrisAppState::TitleScreen;
      }
    }
    GameMenuUI::EndMenuPanel();
    return;
  }

  const bool anyEnded = is2P
                        ? ( _Logics[0].IsGameOver() || _Logics[0].IsVictory() || _Logics[1].IsGameOver() || _Logics[1].IsVictory() )
                        : ( ioLogic.IsGameOver() || ioLogic.IsVictory() );

  // 0b. Online Non-Blocking ESC Menu (Match Continues Underneath)
  if ( isOnline && _MpState->_ShowOnlineInGameMenu && !anyEnded )
  {
    if ( iMenuInput._Back )
    {
      _MpState->_ShowOnlineInGameMenu = false;
      return;
    }

    if ( GameMenuUI::BeginMenuPanel("##TetrisOnlineMenuModal", "ONLINE MATCH MENU (LIVE)", center, Vec2(480.0f, 0.0f), _Theme, _Fonts) )
    {
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "Note: Online matches remain live while this menu is open.");
      ImGui::Dummy(ImVec2(0.0f, 4.0f));

      const std::vector<GameMenuItem> onlineItems =
      {
        { "RESUME MATCH",             "Return to active online board",           "ESC", true, false },
        { "OPTIONS",                  "Configure handling, renderer & audio",    "F1",  true, false },
        { "DISCONNECT & LEAVE MATCH", "Close connection and return to Lobby",    "",    true, true  }
      };

      const int act = GameMenuUI::DrawMenuButtonList(onlineItems, _PauseMenuNav, iMenuInput, _Theme, _Fonts, 54.0f);
      if ( ( act >= 0 ) && _Audio && !iMenuInput._Confirm )
        _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

      if ( 0 == act )
      {
        _MpState->_ShowOnlineInGameMenu = false;
      }
      else if ( 1 == act )
      {
        _ShowInGameOptions = true;
      }
      else if ( 2 == act )
      {
        _MpState->_ShowOnlineInGameMenu    = false;
        _MpState->_RequestDisconnectOnline = true;
        _MpState->_IsOnlineMatch           = false;
        _MpState->_NumPlayers              = 1;
        SetMenuPage(TetrisMenuPage::OnlineLobby);
        ioAppState = TetrisAppState::TitleScreen;
      }
    }
    GameMenuUI::EndMenuPanel();
    return;
  }

  const bool isPaused = is2P
                        ? ( !isOnline && ( _Logics[0].IsPaused() || _Logics[1].IsPaused() ) && !anyEnded )
                        : ( ioLogic.IsPaused() && !anyEnded );

  // 1. Pause Menu (1P & Local 2P)
  if ( isPaused )
  {
    if ( GameMenuUI::BeginMenuPanel("##TetrisPauseModal", "GAME PAUSED", center, Vec2(480.0f, 0.0f), _Theme, _Fonts) )
    {
      const std::vector<GameMenuItem> pauseItems =
      {
        { "RESUME GAME",            "Return to active match",                 "ESC / P", true, false },
        { "RESTART ROUND",          "Start a fresh board in current mode",    "R",       true, false },
        { "OPTIONS",                "Configure handling, renderer & visuals", "F1",      true, false },
        { "RETURN TO TITLE SCREEN", "Exit match and return to Main Menu",     "",        true, true  }
      };

      const int act = GameMenuUI::DrawMenuButtonList(pauseItems, _PauseMenuNav, iMenuInput, _Theme, _Fonts, 56.0f);
      if ( ( act >= 0 ) && _Audio && !iMenuInput._Confirm )
        _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

      if ( 0 == act )
      {
        ioLogic.SetPaused(false);
        if ( is2P )
          _Logics[1].SetPaused(false);
      }
      else if ( 1 == act )
      {
        if ( is2P )
          _MpState->_RequestNextRound = true;
        else
          ioLogic.Reset();
      }
      else if ( 2 == act )
      {
        _ShowInGameOptions = true;
      }
      else if ( 3 == act )
      {
        ioLogic.SetPaused(false);
        if ( is2P )
          _Logics[1].SetPaused(false);
        SetMenuPage(TetrisMenuPage::MainMenu);
        ioAppState = TetrisAppState::TitleScreen;
      }
    }
    GameMenuUI::EndMenuPanel();
    return;
  }

  // 2. 2-Player Round Over / Series Winner Modal (Local & Online)
  if ( is2P && anyEnded )
  {
    const std::string p1Name = isOnline ? std::string(_MpState->_LocalPlayerName) : "PLAYER 1";
    const std::string p2Name = isOnline ? _MpState->_RemotePlayerName : "PLAYER 2";

    const bool seriesWon = ( _MpState->_RoundWins[0] >= _MpState->_TargetRoundWins ) ||
                           ( _MpState->_RoundWins[1] >= _MpState->_TargetRoundWins );

    std::string headerTitle = "ROUND COMPLETE";
    if ( seriesWon )
    {
      if ( _MpState->_RoundWins[0] > _MpState->_RoundWins[1] )
        headerTitle = p1Name + " WINS THE MATCH!";
      else
        headerTitle = p2Name + " WINS THE MATCH!";
    }
    else
    {
      if ( 0 == _MpState->_LastRoundWinner )
        headerTitle = p1Name + " WINS THE ROUND!";
      else if ( 1 == _MpState->_LastRoundWinner )
        headerTitle = p2Name + " WINS THE ROUND!";
      else
        headerTitle = "ROUND DRAW!";
    }

    if ( GameMenuUI::BeginMenuPanel("##Tetris2PEndModal", headerTitle.c_str(), center, Vec2(620.0f, 0.0f), _Theme, _Fonts) )
    {
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                         "SERIES SCORE :   %s  [ %d ]   -   [ %d ]  %s   (First to %d)",
                         p1Name.c_str(), _MpState->_RoundWins[0], _MpState->_RoundWins[1], p2Name.c_str(), _MpState->_TargetRoundWins);
      ImGui::Dummy(ImVec2(0.0f, 4.0f));

      const std::vector<GameStatCard> summaryCards =
      {
        {
          p1Name + " STATS",
          "Score: " + std::to_string(_Logics[0].GetScore()),
          "Lines: " + std::to_string(_Logics[0].GetLinesCleared()) + "  |  Atk: " + std::to_string(_Logics[0].GetTotalAttacksSent()),
          _Logics[0].IsVictory() ? "ROUND WINNER" : _Logics[0].GetGameOverReason(),
          Vec4(0.00f, 0.90f, 0.95f, 1.0f)
        },
        {
          p2Name + " STATS",
          "Score: " + std::to_string(_Logics[1].GetScore()),
          "Lines: " + std::to_string(_Logics[1].GetLinesCleared()) + "  |  Atk: " + std::to_string(_Logics[1].GetTotalAttacksSent()),
          _Logics[1].IsVictory() ? "ROUND WINNER" : _Logics[1].GetGameOverReason(),
          Vec4(1.00f, 0.60f, 0.20f, 1.0f)
        }
      };
      GameMenuUI::DrawStatCardsGrid(summaryCards, 2, _Theme, _Fonts);

      ImGui::Dummy(ImVec2(0.0f, 6.0f));

      const bool isOnlineClient = isOnline && _NetSession && !_NetSession->IsHost();
      if ( isOnlineClient )
      {
        ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.55f, 1.0f),
                           "Waiting for Host (%s) to launch the next round or rematch...",
                           p2Name.c_str());
        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        const std::vector<GameMenuItem> clientEndItems =
        {
          { "DISCONNECT & RETURN TO LOBBY", "Leave match and return to Online Lobby", "", true, true }
        };
        const int act = GameMenuUI::DrawMenuButtonList(clientEndItems, _GameOverMenuNav, iMenuInput, _Theme, _Fonts, 52.0f);
        if ( 0 == act )
        {
          _MpState->_RequestDisconnectOnline = true;
          _MpState->_IsOnlineMatch           = false;
          _MpState->_NumPlayers              = 1;
          SetMenuPage(TetrisMenuPage::OnlineLobby);
          ioAppState = TetrisAppState::TitleScreen;
        }
      }
      else
      {
        std::vector<GameMenuItem> endItems;
        if ( !seriesWon )
        {
          endItems =
          {
            { "NEXT ROUND",             "Start next round in the series",          "ENTER / R", true, false },
            { "REMATCH (RESET 0-0)",    "Reset series score and start fresh",      "",          true, false },
            { isOnline ? "RETURN TO ONLINE LOBBY" : "MULTIPLAYER LOBBY",
              isOnline ? "Disconnect or reconfigure online match" : "Change 2P mode, level, or controls",
              "", true, false },
            { "RETURN TO TITLE SCREEN", "Exit to Main Menu",                       "",          true, true  }
          };
        }
        else
        {
          endItems =
          {
            { "PLAY REMATCH SERIES",    "Start a new series in current mode",      "ENTER / R", true, false },
            { isOnline ? "RETURN TO ONLINE LOBBY" : "MULTIPLAYER LOBBY",
              isOnline ? "Disconnect or reconfigure online match" : "Change 2P mode, level, or controls",
              "", true, false },
            { "RETURN TO TITLE SCREEN", "Exit to Main Menu",                       "",          true, true  }
          };
        }

        const int act = GameMenuUI::DrawMenuButtonList(endItems, _GameOverMenuNav, iMenuInput, _Theme, _Fonts, 52.0f);
        if ( ( act >= 0 ) && _Audio && !iMenuInput._Confirm )
          _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

        if ( !seriesWon )
        {
          if ( 0 == act )      _MpState->_RequestNextRound = true;
          else if ( 1 == act ) _MpState->_RequestRematch   = true;
          else if ( 2 == act )
          {
            if ( isOnline )
            {
              _MpState->_IsOnlineMatch = false;
              _MpState->_NumPlayers    = 1;
              SetMenuPage(TetrisMenuPage::OnlineLobby);
            }
            else
              SetMenuPage(TetrisMenuPage::MultiplayerLobby);
            ioAppState = TetrisAppState::TitleScreen;
          }
          else if ( 3 == act )
          {
            if ( isOnline )
            {
              _MpState->_RequestDisconnectOnline = true;
              _MpState->_IsOnlineMatch           = false;
              _MpState->_NumPlayers              = 1;
            }
            SetMenuPage(TetrisMenuPage::MainMenu);
            ioAppState = TetrisAppState::TitleScreen;
          }
        }
        else
        {
          if ( 0 == act )      _MpState->_RequestRematch   = true;
          else if ( 1 == act )
          {
            if ( isOnline )
            {
              _MpState->_IsOnlineMatch = false;
              _MpState->_NumPlayers    = 1;
              SetMenuPage(TetrisMenuPage::OnlineLobby);
            }
            else
              SetMenuPage(TetrisMenuPage::MultiplayerLobby);
            ioAppState = TetrisAppState::TitleScreen;
          }
          else if ( 2 == act )
          {
            if ( isOnline )
            {
              _MpState->_RequestDisconnectOnline = true;
              _MpState->_IsOnlineMatch           = false;
              _MpState->_NumPlayers              = 1;
            }
            SetMenuPage(TetrisMenuPage::MainMenu);
            ioAppState = TetrisAppState::TitleScreen;
          }
        }
      }
    }
    GameMenuUI::EndMenuPanel();
    return;
  }

  // 3. 1-Player Game Over / Victory Summary Modal
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
      if ( ( act >= 0 ) && _Audio && !iMenuInput._Confirm )
        _Audio->PlaySound2D("ui_confirm", 0.8f, 1.0f);

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

  const bool is2P = _MpState && ( 2 == _MpState->_NumPlayers ) && _Logics;
  const bool isOnline = is2P && _MpState->_IsOnlineMatch;
  const bool inModal = is2P
                       ? ( ( !isOnline && ( _Logics[0].IsPaused() || _Logics[1].IsPaused() ) ) ||
                           _Logics[0].IsGameOver() || _Logics[0].IsVictory() ||
                           _Logics[1].IsGameOver() || _Logics[1].IsVictory() ||
                           _ShowInGameOptions ||
                           ( isOnline && ( _MpState->_ShowOnlineInGameMenu || _MpState->_PeerDisconnectedModal ) ) )
                       : ( ioLogic.IsPaused() || ioLogic.IsGameOver() || ioLogic.IsVictory() || _ShowInGameOptions );

  if ( _Audio )
  {
    const bool inMenu = ( TetrisAppState::TitleScreen == ioAppState ) || inModal;
    if ( inMenu )
    {
      if ( iMenuInput._NavUp || iMenuInput._NavDown || iMenuInput._NavLeft || iMenuInput._NavRight || iMenuInput._PrevTab || iMenuInput._NextTab )
      {
        _Audio->PlaySound2D( "ui_nav", 0.65f, 1.0f );
      }
      else if ( iMenuInput._Confirm )
      {
        _Audio->PlaySound2D( "ui_confirm", 0.8f, 1.0f );
      }
      else if ( iMenuInput._Back )
      {
        _Audio->PlaySound2D( "ui_back", 0.8f, 1.0f );
      }
    }
  }

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

    if ( inModal )
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
