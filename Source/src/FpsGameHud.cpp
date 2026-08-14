#include "FpsGameHud.h"

#include "imgui.h"

#include <algorithm>
#include <string>

namespace RTRT
{

// ----------------------------------------------------------------------------
// FpsGameHudContext
// ----------------------------------------------------------------------------
FpsGameHudContext::FpsGameHudContext( const FpsGameSettings & iGameSettings,
                                      const FpsGameWorld & iGameWorld )
: _GameSettings(iGameSettings)
, _GameWorld(iGameWorld)
{
}

// ----------------------------------------------------------------------------
// Draw
// ----------------------------------------------------------------------------
void FpsGameHud::Draw( const FpsGameHudContext & iContext )
{
  const ImGuiIO & io = ImGui::GetIO();
  ImDrawList * drawList = ImGui::GetForegroundDrawList();
  const FpsPlayer & player = iContext._GameWorld.GetPlayer();

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
    { "HEALTH", player._Health, iContext._GameSettings._MaxHealth, IM_COL32(220, 42, 42, 235) },
    { "ARMOR", player._Armor, iContext._GameSettings._MaxArmor, IM_COL32(64, 142, 255, 235) },
    { "PROJECTILES", iContext._GameWorld.GetProjectileAmmo(), iContext._GameSettings._MaxProjectileAmmo, IM_COL32(235, 190, 48, 235) }
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
