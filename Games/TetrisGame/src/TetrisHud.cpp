#include "TetrisHud.h"
#include "Renderer.h"
#include "Scene.h"
#include "imgui.h"

#include <cstdio>
#include <iomanip>
#include <sstream>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static std::string FormatTime( double iSeconds )
{
  if ( iSeconds < 0. )
    iSeconds = 0.;

  const int totalSec = static_cast<int>(iSeconds);
  const int minutes = totalSec / 60;
  const int seconds = totalSec % 60;
  const int millis = static_cast<int>(( iSeconds - totalSec ) * 1000.);

  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%02d:%02d.%03d", minutes, seconds, millis);
  return std::string(buffer);
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
TetrisHud::TetrisHud()
{
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisHud::~TetrisHud()
{
}

// ----------------------------------------------------------------------------
// Draw
// ----------------------------------------------------------------------------
int TetrisHud::Draw( TetrisGameLogic & ioLogic,
                    TetrisSceneManager & ioSceneMgr,
                    Scene & ioScene,
                    Renderer * ioRenderer,
                    RenderSettings & ioSettings,
                    TetrisRendererMode & ioRendererMode,
                    TetrisHandlingConfig & ioHandling,
                    double iFrameRate,
                    double iDeltaTime,
                    unsigned int iNbRenderedFrames,
                    bool & oReloadRenderer )
{
  DrawGameplayOverlay(ioLogic);
  DrawActionBanner(ioLogic);

  if ( ioLogic.IsGameOver() || ioLogic.IsVictory() || ioLogic.IsPaused() )
    DrawGameOverModal(ioLogic);

  if ( _ShowSettingsWindow )
  {
    DrawSettingsWindow(ioLogic, ioSceneMgr, ioScene, ioRenderer, ioSettings, ioRendererMode,
                       ioHandling, iFrameRate, iDeltaTime, iNbRenderedFrames, oReloadRenderer);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// DrawGameplayOverlay
// ----------------------------------------------------------------------------
void TetrisHud::DrawGameplayOverlay( const TetrisGameLogic & iLogic )
{
  ImGuiIO & io = ImGui::GetIO();
  const float pad = 16.0f;

  // 1. Left Telemetry Window (Score, Level, Lines, Hold Label)
  ImGui::SetNextWindowPos(ImVec2(pad, pad), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.65f);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                          ImGuiWindowFlags_AlwaysAutoResize |
                          ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoFocusOnAppearing |
                          ImGuiWindowFlags_NoNav;

  if ( ImGui::Begin("##TetrisLeftTelemetry", nullptr, flags) )
  {
    const char * modeName = "Marathon";
    if ( TetrisGameMode::Sprint == iLogic.GetGameMode() ) modeName = "Sprint (40 Lines)";
    else if ( TetrisGameMode::Ultra == iLogic.GetGameMode() ) modeName = "Ultra (2 Min)";

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "TETRIS 3D");
    ImGui::Separator();
    ImGui::Text("Mode: %s", modeName);
    ImGui::Text("Score: %lld", iLogic.GetScore());
    ImGui::Text("High Score: %lld", iLogic.GetHighScore());
    ImGui::Text("Level: %d", iLogic.GetLevel());
    ImGui::Text("Lines: %d", iLogic.GetLinesCleared());

    if ( iLogic.GetComboCount() > 0 )
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Combo: %dx", iLogic.GetComboCount());

    if ( iLogic.IsBackToBackActive() )
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.95f, 1.0f), "[BACK-TO-BACK]");

    ImGui::Dummy(ImVec2(0.f, 8.f));
    ImGui::TextDisabled("Hold: Left Shift / C");
    ImGui::TextDisabled("Settings: F1");
  }
  ImGui::End();

  // 2. Right Telemetry Window (Time, Next Queue)
  const float rightPanelWidth = 190.0f;
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - rightPanelWidth - pad, pad), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, 0.f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.65f);

  if ( ImGui::Begin("##TetrisRightTelemetry", nullptr, flags) )
  {
    if ( TetrisGameMode::Ultra == iLogic.GetGameMode() )
    {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "REMAINING TIME");
      ImGui::Text("%s", FormatTime(iLogic.GetRemainingTime()).c_str());
    }
    else
    {
      ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "TIME ELAPSED");
      ImGui::Text("%s", FormatTime(iLogic.GetElapsedTime()).c_str());
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "NEXT PIECES");
    for ( int i = 0; i < 5; ++i )
    {
      const TetrisPieceType piece = iLogic.PeekNextPiece(i);
      const Vec3 col = TetrisPiece::GetColor(piece);
      ImGui::TextColored(ImVec4(col.x, col.y, col.z, 1.0f), "%d. [%s]", i + 1, TetrisPiece::GetName(piece));
    }
  }
  ImGui::End();
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
  const ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.28f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowBgAlpha(0.75f * alpha);

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                          ImGuiWindowFlags_AlwaysAutoResize |
                          ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoInputs |
                          ImGuiWindowFlags_NoFocusOnAppearing |
                          ImGuiWindowFlags_NoNav;

  if ( ImGui::Begin("##TetrisActionBanner", nullptr, flags) )
  {
    ImGui::SetWindowFontScale(1.8f);
    ImGui::TextColored(ImVec4(1.0f, 0.92f, 0.25f, alpha), "%s", iLogic.GetActionBannerText().c_str());
    ImGui::SetWindowFontScale(1.0f);
  }
  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawGameOverModal
// ----------------------------------------------------------------------------
void TetrisHud::DrawGameOverModal( TetrisGameLogic & ioLogic )
{
  ImGuiIO & io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(340.f, 0.f), ImGuiCond_Always);

  const char * title = "GAME PAUSED";
  if ( ioLogic.IsVictory() ) title = "VICTORY!";
  else if ( ioLogic.IsGameOver() ) title = "GAME OVER";

  if ( ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize) )
  {
    if ( ioLogic.IsGameOver() )
    {
      ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Reason: %s", ioLogic.GetGameOverReason().c_str());
      ImGui::Separator();
    }
    else if ( ioLogic.IsVictory() )
    {
      ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "Congratulations! Objective reached!");
      ImGui::Separator();
    }

    ImGui::Text("Final Score: %lld", ioLogic.GetScore());
    ImGui::Text("Lines Cleared: %d", ioLogic.GetLinesCleared());
    ImGui::Text("Total Time: %s", FormatTime(ioLogic.GetElapsedTime()).c_str());

    ImGui::Dummy(ImVec2(0.f, 12.f));
    if ( ioLogic.IsPaused() )
    {
      if ( ImGui::Button("Resume (P)", ImVec2(140.f, 32.f)) )
        ioLogic.SetPaused(false);
      ImGui::SameLine();
    }

    if ( ImGui::Button("Restart Game (R)", ImVec2(140.f, 32.f)) )
      ioLogic.Reset();
  }
  ImGui::End();
}

// ----------------------------------------------------------------------------
// DrawSettingsWindow
// ----------------------------------------------------------------------------
void TetrisHud::DrawSettingsWindow( TetrisGameLogic & ioLogic,
                                   TetrisSceneManager & ioSceneMgr,
                                   Scene & ioScene,
                                   Renderer * ioRenderer,
                                   RenderSettings & ioSettings,
                                   TetrisRendererMode & ioRendererMode,
                                   TetrisHandlingConfig & ioHandling,
                                   double iFrameRate,
                                   double iDeltaTime,
                                   unsigned int iNbRenderedFrames,
                                   bool & oReloadRenderer )
{
  ImGui::SetNextWindowSize(ImVec2(480.f, 580.f), ImGuiCond_FirstUseEver);
  if ( ImGui::Begin("Game & Render Settings (F1 to hide)", &_ShowSettingsWindow) )
  {
    if ( ImGui::CollapsingHeader("Game Mode & Start Options", ImGuiTreeNodeFlags_DefaultOpen) )
    {
      int currentMode = (int)ioLogic.GetGameMode();
      const char * modeNames[] = { "Marathon (15 Levels)", "Sprint (40 Lines)", "Ultra (2 Minutes)" };
      if ( ImGui::Combo("Game Mode", &currentMode, modeNames, IM_ARRAYSIZE(modeNames)) )
      {
        ioLogic.StartNewGame(static_cast<TetrisGameMode>(currentMode), ioLogic.GetStartLevel(), 0);
      }

      int startLevel = ioLogic.GetStartLevel();
      if ( ImGui::SliderInt("Starting Level", &startLevel, 1, 15) )
        ioLogic.SetStartLevel(startLevel);

      bool endless = ioLogic.IsEndlessMarathon();
      if ( ImGui::Checkbox("Endless Marathon Mode", &endless) )
        ioLogic.SetEndlessMarathon(endless);

      if ( ImGui::Button("Start New Game", ImVec2(160.f, 26.f)) )
        ioLogic.Reset();
    }

    if ( ImGui::CollapsingHeader("Rendering Backend", ImGuiTreeNodeFlags_DefaultOpen) )
    {
      int currentRenderer = (int)ioRendererMode;
      const char * rendererNames[] = { "Deferred Renderer (Real-Time PBR)", "Photo Path Tracer (GPU Ray Tracing)", "Software Rasterizer (CPU SIMD)" };
      if ( ImGui::Combo("Active Renderer", &currentRenderer, rendererNames, IM_ARRAYSIZE(rendererNames)) )
      {
        if ( currentRenderer != (int)ioRendererMode )
        {
          ioRendererMode = static_cast<TetrisRendererMode>(currentRenderer);
          oReloadRenderer = true;
        }
      }
    }

    if ( ImGui::CollapsingHeader("3D Visuals & Materials", ImGuiTreeNodeFlags_DefaultOpen) )
    {
      int currentMatPreset = (int)ioSceneMgr.GetMaterialPreset();
      const char * matPresets[] = { "Glossy Arcade Plastic", "Crystal Glass (Transmission/IOR)", "Cyberpunk Neon Glow", "Brushed Metallic" };
      if ( ImGui::Combo("Block Material", &currentMatPreset, matPresets, IM_ARRAYSIZE(matPresets)) )
      {
        ioSceneMgr.SetMaterialPreset(ioScene, static_cast<TetrisMaterialPreset>(currentMatPreset));
        if ( ioRenderer )
          ioRenderer->Notify(DirtyState::SceneMaterials);
      }

      int currentCam = (int)ioSceneMgr.GetCameraPreset();
      const char * camNames[] = { "Frontal 3D (Default)", "Isometric / Angled", "Free Orbit (Mouse Drag)" };
      if ( ImGui::Combo("Camera Preset (V)", &currentCam, camNames, IM_ARRAYSIZE(camNames)) )
        ioSceneMgr.SetCameraPreset(static_cast<TetrisCameraPreset>(currentCam));

      bool ghost = ioSceneMgr.GetShowGhost();
      if ( ImGui::Checkbox("Show Ghost Piece", &ghost) )
        ioSceneMgr.SetShowGhost(ghost);

      bool dynLight = ioSceneMgr.GetDynamicPieceLighting();
      if ( ImGui::Checkbox("Active Piece Dynamic Point Light", &dynLight) )
        ioSceneMgr.SetDynamicPieceLighting(dynLight);
    }

    if ( ImGui::CollapsingHeader("Guideline Handling Tuning") )
    {
      float dasMs = ioHandling._DAS * 1000.f;
      if ( ImGui::SliderFloat("DAS Delay (ms)", &dasMs, 50.f, 300.f, "%.0f ms") )
        ioHandling._DAS = dasMs / 1000.f;

      float arrMs = ioHandling._ARR * 1000.f;
      if ( ImGui::SliderFloat("ARR Repeat Rate (ms)", &arrMs, 0.f, 100.f, "%.0f ms") )
        ioHandling._ARR = arrMs / 1000.f;

      ImGui::SliderFloat("Soft Drop Multiplier", &ioHandling._SoftDropFactor, 1.f, 40.f, "%.1fx");
      ImGui::SliderFloat("Lock Delay (s)", &ioHandling._LockDelay, 0.1f, 1.0f, "%.2f s");
      ImGui::SliderInt("Max Lock Manipulations", &ioHandling._MaxLockResets, 0, 30);
    }

    if ( ImGui::CollapsingHeader("Engine & Profiling Stats") )
    {
      RenderStatsUI::DrawFrameRateGraph(_StatsUIState, iFrameRate, iDeltaTime, iNbRenderedFrames);
      RenderStatsUI::DrawRenderOverview(ioSettings, iDeltaTime, iNbRenderedFrames);
      if ( ioRenderer )
        RenderStatsUI::DrawRenderPassTimings(ioRenderer);
    }
  }
  ImGui::End();
}

}
