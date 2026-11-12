#include "GameMenuUI.h"

#include "AudioEngine.h"
#include "PathTracer.h"
#include "PathUtils.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"

#include "imgui.h"
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace RTRT
{

namespace
{

// ----------------------------------------------------------------------------
// ToImVec4 / ToImU32
// ----------------------------------------------------------------------------
inline ImVec4 ToImVec4( const Vec4 & iCol, float iAlphaScale = 1.0f )
{
  return ImVec4(iCol.x, iCol.y, iCol.z, MathUtil::Clamp(iCol.w * iAlphaScale, 0.0f, 1.0f));
}

inline ImU32 ToImU32( const Vec4 & iCol, float iAlphaScale = 1.0f )
{
  return ImGui::ColorConvertFloat4ToU32(ToImVec4(iCol, iAlphaScale));
}

// ----------------------------------------------------------------------------
// ResolveFontPath
// ----------------------------------------------------------------------------
std::string ResolveFontPath( const std::string & iFontFile )
{
  namespace fs = std::filesystem;
  const std::vector<fs::path> candidates =
  {
    fs::path(PathUtils::GetAssetPath("Fonts/" + iFontFile)),
    fs::path("Dependencies") / "imgui" / "misc" / "fonts" / iFontFile,
    fs::path("..") / "Dependencies" / "imgui" / "misc" / "fonts" / iFontFile,
    fs::path("..") / ".." / "Dependencies" / "imgui" / "misc" / "fonts" / iFontFile,
    fs::path("..") / ".." / ".." / "Dependencies" / "imgui" / "misc" / "fonts" / iFontFile,
    fs::path(PathUtils::GetAssetPath("../Dependencies/imgui/misc/fonts/" + iFontFile))
  };

  std::error_code ec;
  for ( const fs::path & candidate : candidates )
  {
    if ( !candidate.empty() && fs::exists(candidate, ec) && !ec )
      return candidate.string();
  }

  return "";
}

// ----------------------------------------------------------------------------
// DrawKeycapPill
// ----------------------------------------------------------------------------
float DrawKeycapPill( ImDrawList * ioDrawList,
                      const ImVec2 & iPos,
                      const char * iKeyText,
                      const GameUITheme & iTheme,
                      ImFont * iFont,
                      float iFontSize )
{
  if ( !iKeyText || !iKeyText[0] )
    return 0.0f;

  const ImVec2 textSize = iFont ? iFont->CalcTextSizeA(iFontSize, FLT_MAX, 0.0f, iKeyText)
                                : ImGui::CalcTextSize(iKeyText);
  const float padX = 9.0f;
  const float padY = 4.0f;
  const float pillW = std::max(28.0f, textSize.x + padX * 2.0f);
  const float pillH = textSize.y + padY * 2.0f;

  const ImVec2 pMin(iPos.x, iPos.y);
  const ImVec2 pMax(iPos.x + pillW, iPos.y + pillH);

  // Subtle shadow + keycap body + border
  ioDrawList->AddRectFilled(ImVec2(pMin.x, pMin.y + 2.0f), ImVec2(pMax.x, pMax.y + 2.0f), IM_COL32(0, 0, 0, 140), 5.0f);
  ioDrawList->AddRectFilled(pMin, pMax, ToImU32(iTheme._CardHoverBg, 1.0f), 5.0f);
  ioDrawList->AddRect(pMin, pMax, ToImU32(iTheme._AccentPrimary, 0.65f), 5.0f, 0, 1.2f);

  const ImVec2 textPos(pMin.x + ( pillW - textSize.x ) * 0.5f, pMin.y + padY);
  if ( iFont )
    ioDrawList->AddText(iFont, iFontSize, textPos, ToImU32(iTheme._TextPrimary), iKeyText);
  else
    ioDrawList->AddText(textPos, ToImU32(iTheme._TextPrimary), iKeyText);

  return pillW;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// GameUITheme Presets
// ----------------------------------------------------------------------------
GameUITheme GameUITheme::CreateArcadeTheme()
{
  GameUITheme theme;
  theme._BackgroundDim   = Vec4(0.02f, 0.03f, 0.06f, 0.48f);
  theme._PanelBg         = Vec4(0.05f, 0.07f, 0.12f, 0.90f);
  theme._CardBg          = Vec4(0.08f, 0.11f, 0.18f, 0.88f);
  theme._CardHoverBg     = Vec4(0.13f, 0.19f, 0.30f, 0.96f);
  theme._AccentPrimary   = Vec4(0.00f, 0.90f, 0.96f, 1.00f);
  theme._AccentSecondary = Vec4(0.72f, 0.32f, 1.00f, 1.00f);
  theme._TextPrimary     = Vec4(0.96f, 0.98f, 1.00f, 1.00f);
  theme._TextMuted       = Vec4(0.62f, 0.70f, 0.82f, 1.00f);
  theme._DangerColor     = Vec4(1.00f, 0.30f, 0.36f, 1.00f);
  theme._SuccessColor    = Vec4(0.22f, 0.95f, 0.55f, 1.00f);
  theme._CornerRounding  = 8.0f;
  return theme;
}

GameUITheme GameUITheme::CreateTacticalTheme()
{
  GameUITheme theme;
  theme._BackgroundDim   = Vec4(0.03f, 0.03f, 0.03f, 0.55f);
  theme._PanelBg         = Vec4(0.07f, 0.07f, 0.08f, 0.92f);
  theme._CardBg          = Vec4(0.11f, 0.11f, 0.13f, 0.88f);
  theme._CardHoverBg     = Vec4(0.18f, 0.17f, 0.15f, 0.96f);
  theme._AccentPrimary   = Vec4(0.96f, 0.72f, 0.18f, 1.00f);
  theme._AccentSecondary = Vec4(0.90f, 0.25f, 0.22f, 1.00f);
  theme._TextPrimary     = Vec4(0.95f, 0.95f, 0.93f, 1.00f);
  theme._TextMuted       = Vec4(0.66f, 0.66f, 0.64f, 1.00f);
  theme._DangerColor     = Vec4(0.95f, 0.24f, 0.24f, 1.00f);
  theme._SuccessColor    = Vec4(0.30f, 0.88f, 0.45f, 1.00f);
  theme._CornerRounding  = 4.0f;
  return theme;
}

namespace GameMenuUI
{

// ----------------------------------------------------------------------------
// InitializeFonts
// ----------------------------------------------------------------------------
void InitializeFonts( GameUIFonts & oFonts )
{
  ImGuiIO & io = ImGui::GetIO();

  const std::string robotoPath = ResolveFontPath("Roboto-Medium.ttf");
  const std::string cousinePath = ResolveFontPath("Cousine-Regular.ttf");

  if ( !robotoPath.empty() )
  {
    oFonts._BodyFont   = io.Fonts->AddFontFromFileTTF(robotoPath.c_str(), 17.0f);
    oFonts._HeaderFont = io.Fonts->AddFontFromFileTTF(robotoPath.c_str(), 24.0f);
    oFonts._TitleFont  = io.Fonts->AddFontFromFileTTF(robotoPath.c_str(), 46.0f);
  }

  if ( !cousinePath.empty() )
  {
    oFonts._MonoFont = io.Fonts->AddFontFromFileTTF(cousinePath.c_str(), 22.0f);
  }

  ImFont * defaultFont = oFonts._BodyFont ? oFonts._BodyFont : io.Fonts->AddFontDefault();
  if ( !oFonts._BodyFont )   oFonts._BodyFont   = defaultFont;
  if ( !oFonts._HeaderFont ) oFonts._HeaderFont = defaultFont;
  if ( !oFonts._TitleFont )  oFonts._TitleFont  = defaultFont;
  if ( !oFonts._MonoFont )   oFonts._MonoFont   = defaultFont;

  io.FontDefault = oFonts._BodyFont;
}

// ----------------------------------------------------------------------------
// PushThemeStyle / PopThemeStyle
// ----------------------------------------------------------------------------
void PushThemeStyle( const GameUITheme & iTheme )
{
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, iTheme._CornerRounding);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, iTheme._CornerRounding * 0.6f);
  ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, iTheme._CornerRounding * 0.6f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

  ImGui::PushStyleColor(ImGuiCol_WindowBg, ToImVec4(iTheme._PanelBg));
  ImGui::PushStyleColor(ImGuiCol_Border, ToImVec4(iTheme._AccentPrimary, 0.45f));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, ToImVec4(iTheme._CardBg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ToImVec4(iTheme._CardHoverBg));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ToImVec4(iTheme._AccentPrimary, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_SliderGrab, ToImVec4(iTheme._AccentPrimary));
  ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ToImVec4(iTheme._AccentSecondary));
  ImGui::PushStyleColor(ImGuiCol_CheckMark, ToImVec4(iTheme._AccentPrimary));
  ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(iTheme._CardBg));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(iTheme._CardHoverBg));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(iTheme._AccentPrimary, 0.45f));
  ImGui::PushStyleColor(ImGuiCol_Header, ToImVec4(iTheme._CardBg));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToImVec4(iTheme._CardHoverBg));
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, ToImVec4(iTheme._AccentPrimary, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(iTheme._TextPrimary));
}

void PopThemeStyle()
{
  ImGui::PopStyleColor(15);
  ImGui::PopStyleVar(6);
}

// ----------------------------------------------------------------------------
// DrawBackgroundOverlay
// ----------------------------------------------------------------------------
void DrawBackgroundOverlay( const GameUITheme & iTheme, float iAlphaScale )
{
  const ImGuiIO & io = ImGui::GetIO();
  ImDrawList * bgList = ImGui::GetBackgroundDrawList();

  const ImU32 topCol = ToImU32(iTheme._BackgroundDim, iAlphaScale * 1.15f);
  const ImU32 midCol = ToImU32(iTheme._BackgroundDim, iAlphaScale * 0.55f);
  const ImU32 botCol = ToImU32(iTheme._BackgroundDim, iAlphaScale * 1.25f);

  const float w = io.DisplaySize.x;
  const float h = io.DisplaySize.y;

  bgList->AddRectFilledMultiColor(ImVec2(0.f, 0.f), ImVec2(w, h * 0.45f), topCol, topCol, midCol, midCol);
  bgList->AddRectFilledMultiColor(ImVec2(0.f, h * 0.45f), ImVec2(w, h), midCol, midCol, botCol, botCol);
}

// ----------------------------------------------------------------------------
// DrawTitleHeader
// ----------------------------------------------------------------------------
void DrawTitleHeader( const char * iTitle,
                      const char * iSubtitle,
                      const GameUITheme & iTheme,
                      const GameUIFonts & iFonts,
                      float iTopY )
{
  const ImGuiIO & io = ImGui::GetIO();
  ImDrawList * bgList = ImGui::GetBackgroundDrawList();

  const float centerX = io.DisplaySize.x * 0.5f;

  if ( iTitle && iTitle[0] )
  {
    ImFont * font = iFonts._TitleFont;
    const float fontSize = font ? font->LegacySize : 42.0f;
    const ImVec2 titleSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iTitle)
                                  : ImGui::CalcTextSize(iTitle);
    const ImVec2 titlePos(centerX - titleSize.x * 0.5f, iTopY);

    // Glow / drop shadow
    if ( font )
    {
      bgList->AddText(font, fontSize, ImVec2(titlePos.x + 3.0f, titlePos.y + 3.0f), IM_COL32(0, 0, 0, 200), iTitle);
      bgList->AddText(font, fontSize, titlePos, ToImU32(iTheme._TextPrimary), iTitle);
    }
    else
    {
      bgList->AddText(ImVec2(titlePos.x + 2.0f, titlePos.y + 2.0f), IM_COL32(0, 0, 0, 200), iTitle);
      bgList->AddText(titlePos, ToImU32(iTheme._TextPrimary), iTitle);
    }

    // Accent underline bar
    const float barHalfW = std::max(90.0f, titleSize.x * 0.42f);
    const float barY = titlePos.y + titleSize.y + 6.0f;
    bgList->AddRectFilledMultiColor(
      ImVec2(centerX - barHalfW, barY),
      ImVec2(centerX + barHalfW, barY + 3.0f),
      ToImU32(iTheme._AccentSecondary, 0.2f),
      ToImU32(iTheme._AccentPrimary, 1.0f),
      ToImU32(iTheme._AccentPrimary, 1.0f),
      ToImU32(iTheme._AccentSecondary, 0.2f)
    );

    iTopY = barY + 10.0f;
  }

  if ( iSubtitle && iSubtitle[0] )
  {
    ImFont * subFont = iFonts._BodyFont;
    const float subSize = subFont ? subFont->LegacySize : 16.0f;
    const ImVec2 textSize = subFont ? subFont->CalcTextSizeA(subSize, FLT_MAX, 0.0f, iSubtitle)
                                    : ImGui::CalcTextSize(iSubtitle);
    const ImVec2 subPos(centerX - textSize.x * 0.5f, iTopY);

    if ( subFont )
      bgList->AddText(subFont, subSize, subPos, ToImU32(iTheme._AccentPrimary, 0.90f), iSubtitle);
    else
      bgList->AddText(subPos, ToImU32(iTheme._AccentPrimary, 0.90f), iSubtitle);
  }
}

// ----------------------------------------------------------------------------
// DrawFooterBar
// ----------------------------------------------------------------------------
void DrawFooterBar( const char * iLeftStatus,
                    const char * iRightNavigationHints,
                    const GameUITheme & iTheme,
                    const GameUIFonts & iFonts )
{
  const ImGuiIO & io = ImGui::GetIO();
  ImDrawList * fgList = ImGui::GetForegroundDrawList();

  const float barHeight = 38.0f;
  const float yMin = io.DisplaySize.y - barHeight;
  const float w = io.DisplaySize.x;

  fgList->AddRectFilled(ImVec2(0.f, yMin), ImVec2(w, io.DisplaySize.y), ToImU32(iTheme._PanelBg, 0.95f));
  fgList->AddLine(ImVec2(0.f, yMin), ImVec2(w, yMin), ToImU32(iTheme._AccentPrimary, 0.45f), 1.5f);

  ImFont * font = iFonts._BodyFont;
  const float fontSize = font ? font->LegacySize : 16.0f;

  if ( iLeftStatus && iLeftStatus[0] )
  {
    const ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iLeftStatus)
                                 : ImGui::CalcTextSize(iLeftStatus);
    const ImVec2 pos(20.0f, yMin + ( barHeight - textSize.y ) * 0.5f);
    if ( font )
      fgList->AddText(font, fontSize, pos, ToImU32(iTheme._AccentPrimary), iLeftStatus);
    else
      fgList->AddText(pos, ToImU32(iTheme._AccentPrimary), iLeftStatus);
  }

  if ( iRightNavigationHints && iRightNavigationHints[0] )
  {
    const ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iRightNavigationHints)
                                 : ImGui::CalcTextSize(iRightNavigationHints);
    const ImVec2 pos(w - textSize.x - 20.0f, yMin + ( barHeight - textSize.y ) * 0.5f);
    if ( font )
      fgList->AddText(font, fontSize, pos, ToImU32(iTheme._TextMuted), iRightNavigationHints);
    else
      fgList->AddText(pos, ToImU32(iTheme._TextMuted), iRightNavigationHints);
  }
}

// ----------------------------------------------------------------------------
// BeginMenuPanel / EndMenuPanel
// ----------------------------------------------------------------------------
bool BeginMenuPanel( const char * iWindowId,
                     const char * iPanelTitle,
                     const Vec2 & iCenterPos,
                     const Vec2 & iSize,
                     const GameUITheme & iTheme,
                     const GameUIFonts & iFonts )
{
  PushThemeStyle(iTheme);

  ImGui::SetNextWindowPos(ImVec2(iCenterPos.x, iCenterPos.y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(iSize.x, iSize.y), ImGuiCond_Always);

  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                               | ImGuiWindowFlags_NoResize
                               | ImGuiWindowFlags_NoMove
                               | ImGuiWindowFlags_NoCollapse
                               | ImGuiWindowFlags_NoSavedSettings
                               | ImGuiWindowFlags_NoScrollbar
                               | ImGuiWindowFlags_NoScrollWithMouse;

  const bool opened = ImGui::Begin(iWindowId, nullptr, flags);
  if ( opened && iPanelTitle && iPanelTitle[0] )
  {
    DrawSectionHeader(iPanelTitle, iTheme, iFonts);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
  }

  return opened;
}

void EndMenuPanel()
{
  ImGui::End();
  PopThemeStyle();
}

// ----------------------------------------------------------------------------
// DrawSectionHeader
// ----------------------------------------------------------------------------
void DrawSectionHeader( const char * iTitle,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts )
{
  if ( !iTitle || !iTitle[0] )
    return;

  if ( iFonts._HeaderFont )
    ImGui::PushFont(iFonts._HeaderFont);

  ImGui::TextColored(ToImVec4(iTheme._AccentPrimary), "%s", iTitle);

  if ( iFonts._HeaderFont )
    ImGui::PopFont();

  ImDrawList * drawList = ImGui::GetWindowDrawList();
  const ImVec2 pMin = ImGui::GetItemRectMin();
  const ImVec2 pMax = ImGui::GetItemRectMax();
  const float availW = ImGui::GetContentRegionAvail().x;
  drawList->AddRectFilledMultiColor(
    ImVec2(pMin.x, pMax.y + 2.0f),
    ImVec2(pMin.x + availW, pMax.y + 4.0f),
    ToImU32(iTheme._AccentPrimary, 0.85f),
    ToImU32(iTheme._AccentSecondary, 0.10f),
    ToImU32(iTheme._AccentSecondary, 0.10f),
    ToImU32(iTheme._AccentPrimary, 0.85f)
  );
  ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

// ----------------------------------------------------------------------------
// DrawMenuButtonList
// ----------------------------------------------------------------------------
int DrawMenuButtonList( const std::vector<GameMenuItem> & iItems,
                        GameMenuNavState & ioNavState,
                        const GameMenuInput & iInput,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts,
                        float iButtonHeight )
{
  if ( iItems.empty() )
    return -1;

  const int itemCount = static_cast<int>(iItems.size());
  ioNavState._SelectedIndex = MathUtil::Clamp(ioNavState._SelectedIndex, 0, itemCount - 1);

  // Keyboard navigation
  if ( iInput._NavUp )
  {
    for ( int step = 1; step <= itemCount; ++step )
    {
      const int candidate = ( ioNavState._SelectedIndex - step + itemCount ) % itemCount;
      if ( iItems[candidate]._Enabled )
      {
        ioNavState._SelectedIndex = candidate;
        break;
      }
    }
  }
  else if ( iInput._NavDown )
  {
    for ( int step = 1; step <= itemCount; ++step )
    {
      const int candidate = ( ioNavState._SelectedIndex + step ) % itemCount;
      if ( iItems[candidate]._Enabled )
      {
        ioNavState._SelectedIndex = candidate;
        break;
      }
    }
  }

  int activatedIndex = -1;
  ImDrawList * drawList = ImGui::GetWindowDrawList();
  const ImGuiIO & io = ImGui::GetIO();
  const bool mouseMoved = ( std::fabs(io.MouseDelta.x) > 0.5f ) || ( std::fabs(io.MouseDelta.y) > 0.5f );

  for ( int i = 0; i < itemCount; ++i )
  {
    const GameMenuItem & item = iItems[i];
    ImGui::PushID(i);

    const float availWidth = ImGui::GetContentRegionAvail().x;
    const ImVec2 buttonSize(availWidth, iButtonHeight);

    const bool clicked = ImGui::InvisibleButton("##MenuBtn", buttonSize);
    const bool hovered = ImGui::IsItemHovered();

    if ( hovered && mouseMoved && item._Enabled )
      ioNavState._SelectedIndex = i;

    const bool isSelected = ( ioNavState._SelectedIndex == i ) && item._Enabled;

    if ( item._Enabled && clicked )
    {
      ioNavState._SelectedIndex = i;
      activatedIndex = i;
    }

    const ImVec2 pMin = ImGui::GetItemRectMin();
    const ImVec2 pMax = ImGui::GetItemRectMax();

    const Vec4 accentCol = item._IsDanger ? iTheme._DangerColor : iTheme._AccentPrimary;
    const Vec4 bgCol = isSelected ? iTheme._CardHoverBg : iTheme._CardBg;
    const float alphaMul = item._Enabled ? 1.0f : 0.45f;

    // Card body
    drawList->AddRectFilled(pMin, pMax, ToImU32(bgCol, alphaMul), iTheme._CornerRounding);

    // Left neon selection bar
    if ( isSelected )
    {
      drawList->AddRectFilled(
        pMin,
        ImVec2(pMin.x + 6.0f, pMax.y),
        ToImU32(accentCol, 1.0f),
        iTheme._CornerRounding,
        ImDrawFlags_RoundCornersLeft
      );
      drawList->AddRect(pMin, pMax, ToImU32(accentCol, 0.85f), iTheme._CornerRounding, 0, 1.8f);
    }
    else
    {
      drawList->AddRect(pMin, pMax, ToImU32(iTheme._TextMuted, 0.18f * alphaMul), iTheme._CornerRounding, 0, 1.0f);
    }

    // Label & optional description
    const float textLeftX = pMin.x + ( isSelected ? 20.0f : 16.0f );
    ImFont * headerFont = iFonts._HeaderFont;
    ImFont * bodyFont   = iFonts._BodyFont;
    const float headerSize = headerFont ? headerFont->LegacySize : 22.0f;
    const float bodySize   = bodyFont   ? bodyFont->LegacySize   : 15.0f;

    const bool hasDesc = !item._Description.empty();
    const float labelY = hasDesc ? ( pMin.y + 6.0f ) : ( pMin.y + ( iButtonHeight - headerSize ) * 0.5f );

    const Vec4 labelCol = isSelected ? ( item._IsDanger ? iTheme._DangerColor : iTheme._TextPrimary )
                                     : iTheme._TextPrimary;

    if ( headerFont )
      drawList->AddText(headerFont, headerSize, ImVec2(textLeftX, labelY), ToImU32(labelCol, alphaMul), item._Label.c_str());
    else
      drawList->AddText(ImVec2(textLeftX, labelY), ToImU32(labelCol, alphaMul), item._Label.c_str());

    if ( hasDesc )
    {
      const float descY = labelY + headerSize + 1.0f;
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize, ImVec2(textLeftX, descY), ToImU32(iTheme._TextMuted, alphaMul), item._Description.c_str());
      else
        drawList->AddText(ImVec2(textLeftX, descY), ToImU32(iTheme._TextMuted, alphaMul), item._Description.c_str());
    }

    // Right badge
    if ( !item._RightBadge.empty() )
    {
      const ImVec2 badgeSize = bodyFont ? bodyFont->CalcTextSizeA(bodySize, FLT_MAX, 0.0f, item._RightBadge.c_str())
                                        : ImGui::CalcTextSize(item._RightBadge.c_str());
      const float pillW = badgeSize.x + 18.0f;
      const float pillH = badgeSize.y + 8.0f;
      const ImVec2 bMin(pMax.x - pillW - 14.0f, pMin.y + ( iButtonHeight - pillH ) * 0.5f);
      const ImVec2 bMax(bMin.x + pillW, bMin.y + pillH);

      drawList->AddRectFilled(bMin, bMax, ToImU32(accentCol, isSelected ? 0.28f : 0.15f), pillH * 0.5f);
      drawList->AddRect(bMin, bMax, ToImU32(accentCol, isSelected ? 0.80f : 0.40f), pillH * 0.5f);

      const ImVec2 bTextPos(bMin.x + 9.0f, bMin.y + 4.0f);
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize, bTextPos, ToImU32(iTheme._TextPrimary, alphaMul), item._RightBadge.c_str());
      else
        drawList->AddText(bTextPos, ToImU32(iTheme._TextPrimary, alphaMul), item._RightBadge.c_str());
    }

    ImGui::PopID();
  }

  if ( ( activatedIndex < 0 ) && iInput._Confirm )
  {
    if ( ( ioNavState._SelectedIndex >= 0 ) && ( ioNavState._SelectedIndex < itemCount ) && iItems[ioNavState._SelectedIndex]._Enabled )
      activatedIndex = ioNavState._SelectedIndex;
  }

  return activatedIndex;
}

// ----------------------------------------------------------------------------
// DrawSelectionCards
// ----------------------------------------------------------------------------
bool DrawSelectionCards( const std::vector<GameSelectionCard> & iCards,
                         int & ioSelectedIndex,
                         const GameMenuInput & iInput,
                         const GameUITheme & iTheme,
                         const GameUIFonts & iFonts,
                         bool * oOutConfirmed )
{
  if ( oOutConfirmed )
    *oOutConfirmed = false;

  if ( iCards.empty() )
    return false;

  const int cardCount = static_cast<int>(iCards.size());
  const int prevSelected = ioSelectedIndex;
  ioSelectedIndex = MathUtil::Clamp(ioSelectedIndex, 0, cardCount - 1);

  if ( iInput._NavLeft )
    ioSelectedIndex = ( ioSelectedIndex - 1 + cardCount ) % cardCount;
  else if ( iInput._NavRight )
    ioSelectedIndex = ( ioSelectedIndex + 1 ) % cardCount;

  const float availW = ImGui::GetContentRegionAvail().x;
  const float spacing = 14.0f;
  const float cardW = ( availW - spacing * static_cast<float>(cardCount - 1) ) / static_cast<float>(cardCount);
  const float cardH = 195.0f;

  ImDrawList * drawList = ImGui::GetWindowDrawList();
  ImFont * headerFont = iFonts._HeaderFont;
  ImFont * bodyFont   = iFonts._BodyFont;
  const float headerSize = headerFont ? headerFont->LegacySize : 22.0f;
  const float bodySize   = bodyFont   ? bodyFont->LegacySize   : 15.0f;

  for ( int i = 0; i < cardCount; ++i )
  {
    if ( i > 0 )
      ImGui::SameLine(0.0f, spacing);

    ImGui::PushID(i);
    const GameSelectionCard & card = iCards[i];

    const bool clicked = ImGui::InvisibleButton("##SelectCard", ImVec2(cardW, cardH));
    const bool hovered = ImGui::IsItemHovered();
    const bool doubleClicked = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    if ( clicked )
      ioSelectedIndex = i;

    if ( doubleClicked && oOutConfirmed )
      *oOutConfirmed = true;

    const bool isSelected = ( ioSelectedIndex == i );
    const ImVec2 pMin = ImGui::GetItemRectMin();
    const ImVec2 pMax = ImGui::GetItemRectMax();

    const Vec4 bgCol = ( isSelected || hovered ) ? iTheme._CardHoverBg : iTheme._CardBg;
    drawList->AddRectFilled(pMin, pMax, ToImU32(bgCol), iTheme._CornerRounding);

    // Top color accent strip
    drawList->AddRectFilled(
      pMin,
      ImVec2(pMax.x, pMin.y + 6.0f),
      ToImU32(card._AccentColor, isSelected ? 1.0f : 0.55f),
      iTheme._CornerRounding,
      ImDrawFlags_RoundCornersTop
    );

    if ( isSelected )
      drawList->AddRect(pMin, pMax, ToImU32(card._AccentColor, 0.95f), iTheme._CornerRounding, 0, 2.2f);
    else
      drawList->AddRect(pMin, pMax, ToImU32(iTheme._TextMuted, 0.25f), iTheme._CornerRounding, 0, 1.0f);

    float curY = pMin.y + 16.0f;
    const float padX = 14.0f;

    // Title
    if ( headerFont )
      drawList->AddText(headerFont, headerSize, ImVec2(pMin.x + padX, curY), ToImU32(iTheme._TextPrimary), card._Title.c_str());
    else
      drawList->AddText(ImVec2(pMin.x + padX, curY), ToImU32(iTheme._TextPrimary), card._Title.c_str());
    curY += headerSize + 4.0f;

    // Subtitle
    if ( !card._Subtitle.empty() )
    {
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize, ImVec2(pMin.x + padX, curY), ToImU32(card._AccentColor), card._Subtitle.c_str());
      else
        drawList->AddText(ImVec2(pMin.x + padX, curY), ToImU32(card._AccentColor), card._Subtitle.c_str());
      curY += bodySize + 10.0f;
    }

    // Wrapped description
    if ( !card._Description.empty() )
    {
      const float wrapWidth = cardW - padX * 2.0f;
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize, ImVec2(pMin.x + padX, curY), ToImU32(iTheme._TextMuted), card._Description.c_str(), nullptr, wrapWidth);
      else
        drawList->AddText(ImVec2(pMin.x + padX, curY), ToImU32(iTheme._TextMuted), card._Description.c_str());
    }

    // Bottom stat badge
    if ( !card._StatLabel.empty() )
    {
      const ImVec2 statMin(pMin.x + 10.0f, pMax.y - 36.0f);
      const ImVec2 statMax(pMax.x - 10.0f, pMax.y - 10.0f);
      drawList->AddRectFilled(statMin, statMax, IM_COL32(0, 0, 0, 95), 4.0f);
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize, ImVec2(statMin.x + 8.0f, statMin.y + 4.0f), ToImU32(iTheme._TextPrimary, 0.9f), card._StatLabel.c_str());
      else
        drawList->AddText(ImVec2(statMin.x + 8.0f, statMin.y + 4.0f), ToImU32(iTheme._TextPrimary, 0.9f), card._StatLabel.c_str());
    }

    ImGui::PopID();
  }

  if ( iInput._Confirm && oOutConfirmed )
    *oOutConfirmed = true;

  return ( ioSelectedIndex != prevSelected );
}

// ----------------------------------------------------------------------------
// DrawTabBar
// ----------------------------------------------------------------------------
bool DrawTabBar( const std::vector<const char *> & iTabNames,
                 int & ioActiveTab,
                 const GameUITheme & iTheme,
                 const GameUIFonts & iFonts )
{
  if ( iTabNames.empty() )
    return false;

  const int tabCount = static_cast<int>(iTabNames.size());
  const int prevTab = ioActiveTab;
  ioActiveTab = MathUtil::Clamp(ioActiveTab, 0, tabCount - 1);

  const float availW = ImGui::GetContentRegionAvail().x;
  const float spacing = 8.0f;
  const float tabW = ( availW - spacing * static_cast<float>(tabCount - 1) ) / static_cast<float>(tabCount);
  const float tabH = 40.0f;

  ImDrawList * drawList = ImGui::GetWindowDrawList();
  ImFont * font = iFonts._HeaderFont ? iFonts._HeaderFont : iFonts._BodyFont;
  const float fontSize = font ? ( font->LegacySize * 0.85f ) : 18.0f;

  for ( int i = 0; i < tabCount; ++i )
  {
    if ( i > 0 )
      ImGui::SameLine(0.0f, spacing);

    ImGui::PushID(i);
    if ( ImGui::InvisibleButton("##TabBtn", ImVec2(tabW, tabH)) )
      ioActiveTab = i;

    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ( ioActiveTab == i );

    const ImVec2 pMin = ImGui::GetItemRectMin();
    const ImVec2 pMax = ImGui::GetItemRectMax();

    const Vec4 bgCol = active ? iTheme._CardHoverBg : ( hovered ? iTheme._CardHoverBg : iTheme._CardBg );
    drawList->AddRectFilled(pMin, pMax, ToImU32(bgCol, active ? 1.0f : 0.75f), iTheme._CornerRounding);

    if ( active )
    {
      drawList->AddRectFilled(
        ImVec2(pMin.x + 8.0f, pMax.y - 4.0f),
        ImVec2(pMax.x - 8.0f, pMax.y),
        ToImU32(iTheme._AccentPrimary),
        2.0f
      );
      drawList->AddRect(pMin, pMax, ToImU32(iTheme._AccentPrimary, 0.75f), iTheme._CornerRounding, 0, 1.5f);
    }

    ImFont * drawFont = font;
    float drawFontSize = fontSize;
    ImVec2 textSize = drawFont ? drawFont->CalcTextSizeA(drawFontSize, FLT_MAX, 0.0f, iTabNames[i])
                               : ImGui::CalcTextSize(iTabNames[i]);

    if ( textSize.x > ( tabW - 12.0f ) && iFonts._BodyFont )
    {
      drawFont = iFonts._BodyFont;
      drawFontSize = drawFont->LegacySize;
      textSize = drawFont->CalcTextSizeA(drawFontSize, FLT_MAX, 0.0f, iTabNames[i]);
    }
    if ( textSize.x > ( tabW - 8.0f ) && drawFont )
    {
      const float shrinkRatio = ( tabW - 8.0f ) / std::max(1.0f, textSize.x);
      drawFontSize = std::max(12.0f, drawFontSize * shrinkRatio);
      textSize = drawFont->CalcTextSizeA(drawFontSize, FLT_MAX, 0.0f, iTabNames[i]);
    }

    const ImVec2 textPos(pMin.x + ( tabW - textSize.x ) * 0.5f, pMin.y + ( tabH - textSize.y ) * 0.5f - 1.0f);

    drawList->PushClipRect(pMin, pMax, true);
    if ( drawFont )
      drawList->AddText(drawFont, drawFontSize, textPos, ToImU32(active ? iTheme._AccentPrimary : iTheme._TextMuted), iTabNames[i]);
    else
      drawList->AddText(textPos, ToImU32(active ? iTheme._AccentPrimary : iTheme._TextMuted), iTabNames[i]);
    drawList->PopClipRect();

    ImGui::PopID();
  }

  ImGui::Dummy(ImVec2(0.0f, 6.0f));
  return ( ioActiveTab != prevTab );
}

// ----------------------------------------------------------------------------
// DrawKeybindingTable
// ----------------------------------------------------------------------------
void DrawKeybindingTable( const std::vector<GameKeybindEntry> & iEntries,
                          const GameUITheme & iTheme,
                          const GameUIFonts & iFonts )
{
  ImDrawList * drawList = ImGui::GetWindowDrawList();
  ImFont * bodyFont = iFonts._BodyFont;
  const float fontSize = bodyFont ? bodyFont->LegacySize : 16.0f;

  std::string currentCategory;

  for ( size_t i = 0; i < iEntries.size(); ++i )
  {
    const GameKeybindEntry & entry = iEntries[i];
    if ( !entry._Category.empty() && ( entry._Category != currentCategory ) )
    {
      currentCategory = entry._Category;
      if ( i > 0 )
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
      ImGui::TextColored(ToImVec4(iTheme._AccentPrimary), "%s", currentCategory.c_str());
      ImGui::Separator();
    }

    const float availW = ImGui::GetContentRegionAvail().x;
    const float rowH = 32.0f;
    const ImVec2 rowMin = ImGui::GetCursorScreenPos();
    const ImVec2 rowMax(rowMin.x + availW, rowMin.y + rowH);

    if ( i % 2 == 0 )
      drawList->AddRectFilled(rowMin, rowMax, ToImU32(iTheme._CardBg, 0.45f), 4.0f);

    // Action label on left
    if ( bodyFont )
      drawList->AddText(bodyFont, fontSize, ImVec2(rowMin.x + 10.0f, rowMin.y + 6.0f), ToImU32(iTheme._TextPrimary), entry._Action.c_str());
    else
      drawList->AddText(ImVec2(rowMin.x + 10.0f, rowMin.y + 6.0f), ToImU32(iTheme._TextPrimary), entry._Action.c_str());

    // Keycap badges on right (Keyboard columns + Xbox Gamepad column)
    float rightX = rowMin.x + availW * 0.44f;
    if ( !entry._PrimaryKey.empty() )
    {
      const float w1 = DrawKeycapPill(drawList, ImVec2(rightX, rowMin.y + 3.0f), entry._PrimaryKey.c_str(), iTheme, bodyFont, fontSize);
      rightX += w1 + 6.0f;
    }
    if ( !entry._SecondaryKey.empty() )
    {
      DrawKeycapPill(drawList, ImVec2(rightX, rowMin.y + 3.0f), entry._SecondaryKey.c_str(), iTheme, bodyFont, fontSize);
    }

    if ( !entry._GamepadButton.empty() )
    {
      const float gpX = rowMin.x + availW * 0.75f;
      DrawKeycapPill(drawList, ImVec2(gpX, rowMin.y + 3.0f), entry._GamepadButton.c_str(), iTheme, bodyFont, fontSize);
    }

    ImGui::Dummy(ImVec2(availW, rowH - 2.0f));
  }
}

// ----------------------------------------------------------------------------
// DrawStatCardsGrid
// ----------------------------------------------------------------------------
void DrawStatCardsGrid( const std::vector<GameStatCard> & iCards,
                        int iColumns,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts )
{
  if ( iCards.empty() || ( iColumns <= 0 ) )
    return;

  const float availW = ImGui::GetContentRegionAvail().x;
  const float spacing = 12.0f;
  const float cardW = ( availW - spacing * static_cast<float>(iColumns - 1) ) / static_cast<float>(iColumns);
  const float cardH = 112.0f;

  ImDrawList * drawList = ImGui::GetWindowDrawList();
  ImFont * bodyFont = iFonts._BodyFont;
  ImFont * monoFont = iFonts._MonoFont ? iFonts._MonoFont : iFonts._HeaderFont;
  const float bodySize = bodyFont ? bodyFont->LegacySize : 16.0f;
  const float monoSize = monoFont ? monoFont->LegacySize : 22.0f;

  for ( int i = 0; i < static_cast<int>(iCards.size()); ++i )
  {
    if ( ( i % iColumns ) != 0 )
      ImGui::SameLine(0.0f, spacing);

    const GameStatCard & card = iCards[i];
    const ImVec2 pMin = ImGui::GetCursorScreenPos();
    const ImVec2 pMax(pMin.x + cardW, pMin.y + cardH);

    drawList->AddRectFilled(pMin, pMax, ToImU32(iTheme._CardBg), iTheme._CornerRounding);
    drawList->AddRectFilled(pMin, ImVec2(pMin.x + 5.0f, pMax.y), ToImU32(card._AccentColor), iTheme._CornerRounding, ImDrawFlags_RoundCornersLeft);
    drawList->AddRect(pMin, pMax, ToImU32(card._AccentColor, 0.40f), iTheme._CornerRounding);

    float curY = pMin.y + 10.0f;
    const float leftX = pMin.x + 16.0f;

    if ( bodyFont )
      drawList->AddText(bodyFont, bodySize, ImVec2(leftX, curY), ToImU32(card._AccentColor), card._Title.c_str());
    else
      drawList->AddText(ImVec2(leftX, curY), ToImU32(card._AccentColor), card._Title.c_str());
    curY += bodySize + 6.0f;

    if ( monoFont )
      drawList->AddText(monoFont, monoSize, ImVec2(leftX, curY), ToImU32(iTheme._TextPrimary), card._MainValue.c_str());
    else
      drawList->AddText(ImVec2(leftX, curY), ToImU32(iTheme._TextPrimary), card._MainValue.c_str());
    curY += monoSize + 6.0f;

    if ( !card._SubValue1.empty() )
    {
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize * 0.92f, ImVec2(leftX, curY), ToImU32(iTheme._TextMuted), card._SubValue1.c_str());
      else
        drawList->AddText(ImVec2(leftX, curY), ToImU32(iTheme._TextMuted), card._SubValue1.c_str());
    }

    if ( !card._SubValue2.empty() )
    {
      const ImVec2 sub2Size = bodyFont ? bodyFont->CalcTextSizeA(bodySize * 0.92f, FLT_MAX, 0.0f, card._SubValue2.c_str())
                                       : ImGui::CalcTextSize(card._SubValue2.c_str());
      if ( bodyFont )
        drawList->AddText(bodyFont, bodySize * 0.92f, ImVec2(pMax.x - sub2Size.x - 12.0f, curY), ToImU32(iTheme._TextMuted), card._SubValue2.c_str());
      else
        drawList->AddText(ImVec2(pMax.x - sub2Size.x - 12.0f, curY), ToImU32(iTheme._TextMuted), card._SubValue2.c_str());
    }

    ImGui::Dummy(ImVec2(cardW, cardH));
  }
}

// ----------------------------------------------------------------------------
// DrawActionButton
// ----------------------------------------------------------------------------
bool DrawActionButton( const char * iLabel,
                       const Vec2 & iSize,
                       bool iIsPrimary,
                       const GameUITheme & iTheme,
                       const GameUIFonts & iFonts )
{
  ImGui::PushID(iLabel);

  const float btnW = ( iSize.x > 0.0f ) ? iSize.x : ImGui::GetContentRegionAvail().x;
  const float btnH = ( iSize.y > 0.0f ) ? iSize.y : 44.0f;

  const bool clicked = ImGui::InvisibleButton("##ActionBtn", ImVec2(btnW, btnH));
  const bool hovered = ImGui::IsItemHovered();

  ImDrawList * drawList = ImGui::GetWindowDrawList();
  const ImVec2 pMin = ImGui::GetItemRectMin();
  const ImVec2 pMax = ImGui::GetItemRectMax();

  const Vec4 accent = iIsPrimary ? iTheme._AccentPrimary : iTheme._TextMuted;
  const Vec4 bg = iIsPrimary
    ? ( hovered ? Vec4(iTheme._AccentPrimary.x * 0.35f, iTheme._AccentPrimary.y * 0.35f, iTheme._AccentPrimary.z * 0.35f, 0.95f)
                : Vec4(iTheme._AccentPrimary.x * 0.22f, iTheme._AccentPrimary.y * 0.22f, iTheme._AccentPrimary.z * 0.22f, 0.90f) )
    : ( hovered ? iTheme._CardHoverBg : iTheme._CardBg );

  drawList->AddRectFilled(pMin, pMax, ToImU32(bg), iTheme._CornerRounding);
  drawList->AddRect(pMin, pMax, ToImU32(accent, hovered ? 1.0f : 0.65f), iTheme._CornerRounding, 0, iIsPrimary ? 1.8f : 1.2f);

  ImFont * font = iFonts._HeaderFont ? iFonts._HeaderFont : iFonts._BodyFont;
  const float fontSize = font ? ( font->LegacySize * 0.90f ) : 18.0f;
  const ImVec2 textSize = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, iLabel)
                               : ImGui::CalcTextSize(iLabel);
  const ImVec2 textPos(pMin.x + ( btnW - textSize.x ) * 0.5f, pMin.y + ( btnH - textSize.y ) * 0.5f);

  if ( font )
    drawList->AddText(font, fontSize, textPos, ToImU32(iTheme._TextPrimary), iLabel);
  else
    drawList->AddText(textPos, ToImU32(iTheme._TextPrimary), iLabel);

  ImGui::PopID();
  return clicked;
}

// ----------------------------------------------------------------------------
// DrawSharedRendererSettings
// ----------------------------------------------------------------------------
void DrawAudioSettings( AudioEngine & ioAudio,
                        const GameUITheme & iTheme,
                        const GameUIFonts & iFonts )
{
  DrawSectionHeader( "AUDIO OUTPUT & VOLUME", iTheme, iFonts );

  bool muted = ioAudio.IsMuted();
  if ( ImGui::Checkbox( "Mute All Audio", &muted ) )
  {
    ioAudio.SetMuted( muted );
  }

  ImGui::Spacing();

  float masterVol = ioAudio.GetMasterVolume() * 100.0f;
  if ( ImGui::SliderFloat( "Master Volume", &masterVol, 0.0f, 100.0f, "%.0f%%" ) )
  {
    ioAudio.SetMasterVolume( masterVol / 100.0f );
  }

  float sfxVol = ioAudio.GetSfxVolume() * 100.0f;
  if ( ImGui::SliderFloat( "SFX Volume", &sfxVol, 0.0f, 100.0f, "%.0f%%" ) )
  {
    ioAudio.SetSfxVolume( sfxVol / 100.0f );
  }

  float musicVol = ioAudio.GetMusicVolume() * 100.0f;
  if ( ImGui::SliderFloat( "Music Volume", &musicVol, 0.0f, 100.0f, "%.0f%%" ) )
  {
    ioAudio.SetMusicVolume( musicVol / 100.0f );
  }

  ImGui::Spacing();
  if ( ImGui::Button( "Test SFX", ImVec2( 120.0f, 32.0f ) ) )
  {
    if ( ioAudio.HasSound( "ui_confirm" ) )
    {
      ioAudio.PlaySound2D( "ui_confirm", 1.0f, 1.0f );
    }
    else if ( ioAudio.HasSound( "piece_harddrop" ) )
    {
      ioAudio.PlaySound2D( "piece_harddrop", 1.0f, 1.0f );
    }
  }
}

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
                                 unsigned int iNbRenderedFrames )
{
  if ( iRendererModeNames && ( iRendererModeCount > 0 ) )
  {
    int currentRenderer = ioRendererModeIndex;
    if ( ImGui::Combo("Active Renderer Backend", &currentRenderer, iRendererModeNames, iRendererModeCount) )
    {
      if ( currentRenderer != ioRendererModeIndex )
      {
        ioRendererModeIndex = currentRenderer;
        oReloadRenderer = true;
      }
    }
  }

  bool settingsDirty = false;

  struct ResolutionPreset
  {
    int _Width;
    int _Height;
    const char * _Label;
  };

  static const ResolutionPreset kResolutions[] =
  {
    {  800,  600, "800 x 600 (4:3 SVGA)" },
    { 1024,  768, "1024 x 768 (4:3 XGA)" },
    { 1280,  720, "1280 x 720 (16:9 720p HD)" },
    { 1366,  768, "1366 x 768 (16:9 WXGA)" },
    { 1600,  900, "1600 x 900 (16:9 900p HD+)" },
    { 1920, 1080, "1920 x 1080 (16:9 1080p FHD)" },
    { 1920, 1200, "1920 x 1200 (16:10 WUXGA)" },
    { 2560, 1440, "2560 x 1440 (16:9 1440p 2K QHD)" },
    { 2560, 1080, "2560 x 1080 (21:9 UW-FHD)" },
    { 3440, 1440, "3440 x 1440 (21:9 UW-QHD)" },
    { 3840, 2160, "3840 x 2160 (16:9 4K UHD)" }
  };
  const int numPresets = static_cast<int>(sizeof(kResolutions) / sizeof(kResolutions[0]));

  const Vec2i activeBase = ( ioSettings._CustomRenderResolution.x > 0 && ioSettings._CustomRenderResolution.y > 0 )
                           ? ioSettings._CustomRenderResolution
                           : ioSettings._WindowResolution;

  int currentPresetIdx = -1;
  if ( ( ioSettings._CustomRenderResolution.x <= 0 ) || ( ioSettings._CustomRenderResolution.y <= 0 ) )
  {
    currentPresetIdx = 0;
  }
  else
  {
    for ( int i = 0; i < numPresets; ++i )
    {
      if ( ( kResolutions[i]._Width == activeBase.x ) && ( kResolutions[i]._Height == activeBase.y ) )
      {
        currentPresetIdx = i + 1;
        break;
      }
    }
  }

  char nativeLabel[64];
  std::snprintf(nativeLabel, sizeof(nativeLabel), "Window Native (%d x %d)", ioSettings._WindowResolution.x, ioSettings._WindowResolution.y);

  char customLabel[64];
  if ( -1 == currentPresetIdx )
    std::snprintf(customLabel, sizeof(customLabel), "Custom (%d x %d)", activeBase.x, activeBase.y);

  const char * previewValue = nativeLabel;
  if ( 0 == currentPresetIdx )
    previewValue = nativeLabel;
  else if ( ( currentPresetIdx > 0 ) && ( currentPresetIdx <= numPresets ) )
    previewValue = kResolutions[currentPresetIdx - 1]._Label;
  else
    previewValue = customLabel;

  if ( ImGui::BeginCombo("Render Resolution", previewValue) )
  {
    const bool isNativeSelected = ( 0 == currentPresetIdx );
    if ( ImGui::Selectable(nativeLabel, isNativeSelected) )
    {
      ioSettings._CustomRenderResolution = Vec2i(0, 0);
      settingsDirty = true;
    }
    if ( isNativeSelected )
      ImGui::SetItemDefaultFocus();

    for ( int i = 0; i < numPresets; ++i )
    {
      const bool isSelected = ( ( i + 1 ) == currentPresetIdx );
      if ( ImGui::Selectable(kResolutions[i]._Label, isSelected) )
      {
        ioSettings._CustomRenderResolution = Vec2i(kResolutions[i]._Width, kResolutions[i]._Height);
        GLFWwindow * win = glfwGetCurrentContext();
        if ( win )
          glfwSetWindowSize(win, kResolutions[i]._Width, kResolutions[i]._Height);
        settingsDirty = true;
      }
      if ( isSelected )
        ImGui::SetItemDefaultFocus();
    }

    if ( -1 == currentPresetIdx )
    {
      if ( ImGui::Selectable(customLabel, true) )
      {
      }
      ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
  }

  const int prevScale = ioSettings._RenderScale;
  settingsDirty |= ImGui::SliderInt("Render Scale (%)", &ioSettings._RenderScale, 25, 100);

  if ( settingsDirty || ( prevScale != ioSettings._RenderScale ) )
  {
    const Vec2i baseRes = ( ioSettings._CustomRenderResolution.x > 0 && ioSettings._CustomRenderResolution.y > 0 )
                          ? ioSettings._CustomRenderResolution
                          : ioSettings._WindowResolution;
    ioSettings._RenderResolution.x = static_cast<int>(baseRes.x * ( ioSettings._RenderScale * 0.01f ));
    ioSettings._RenderResolution.y = static_cast<int>(baseRes.y * ( ioSettings._RenderScale * 0.01f ));
    settingsDirty = true;
  }

  GLFWwindow * currentWin = glfwGetCurrentContext();
  const bool isCurrentlyFullscreen = currentWin ? ( glfwGetWindowMonitor(currentWin) != nullptr ) : ioSettings._FullScreen;
  ioSettings._FullScreen = isCurrentlyFullscreen;
  bool fsToggle = isCurrentlyFullscreen;
  if ( ImGui::Checkbox("Fullscreen", &fsToggle) )
  {
    if ( currentWin )
      SetWindowFullscreen(currentWin, fsToggle, ioSettings);
    else
      ioSettings._FullScreen = fsToggle;
    settingsDirty = true;
  }
  ImGui::SameLine();
  if ( ImGui::Checkbox("VSync", &ioSettings._VSync) )
  {
    glfwSwapInterval(ioSettings._VSync ? 1 : 0);
  }
  ImGui::SameLine();
  settingsDirty |= ImGui::Checkbox("Tone Mapping", &ioSettings._ToneMapping);
  ImGui::SameLine();
  settingsDirty |= ImGui::Checkbox("FXAA", &ioSettings._FXAA);
  ImGui::SameLine();
  settingsDirty |= ImGui::Checkbox("Skybox Background", &ioSettings._EnableSkybox);

  settingsDirty |= ImGui::SliderFloat("Exposure", &ioSettings._Exposure, 0.1f, 4.0f, "%.2f");
  settingsDirty |= ImGui::SliderFloat("Gamma", &ioSettings._Gamma, 1.0f, 3.0f, "%.2f");

  if ( ioRenderer && ioRenderer->AsDeferredRenderer() )
  {
    ImGui::Separator();
    ImGui::TextDisabled("Deferred Renderer Features");
    settingsDirty |= ImGui::Checkbox("Shadow Mapping", &ioSettings._ShadowMapping);
    ImGui::SameLine();
    settingsDirty |= ImGui::Checkbox("SSAO", &ioSettings._SSAO);
    ImGui::SameLine();
    settingsDirty |= ImGui::Checkbox("SSR Reflections", &ioSettings._SSR);
    settingsDirty |= ImGui::Checkbox("Transparency Pass", &ioSettings._Transparency);
    ImGui::SameLine();
    settingsDirty |= ImGui::Checkbox("Refraction", &ioSettings._Refraction);
  }
  else if ( ioRenderer && ioRenderer->AsPathTracer() )
  {
    ImGui::Separator();
    ImGui::TextDisabled("Path Tracer Features");
    settingsDirty |= ImGui::SliderInt("Max Bounces", &ioSettings._Bounces, 1, 8);
    settingsDirty |= ImGui::SliderInt("Samples Per Pixel", &ioSettings._NbSamplesPerPixel, 1, 8);
    settingsDirty |= ImGui::Checkbox("Temporal Accumulation", &ioSettings._Accumulate);
    ImGui::SameLine();
    settingsDirty |= ImGui::Checkbox("Enable Denoiser", &ioSettings._Denoise);

    if ( ioSettings._Denoise )
    {
#if defined(RTRT_HAS_OIDN)
      static const char * denoisingMethods[] =
      {
        "Bilateral (Fast Shader)",
        "Wavelet (A-Trous)",
        "Edge-Aware (G-Buffer Guided)",
        "OIDN (Intel AI Denoiser)"
      };
      const int methodCount = 4;
#else
      static const char * denoisingMethods[] =
      {
        "Bilateral (Fast Shader)",
        "Wavelet (A-Trous)",
        "Edge-Aware (G-Buffer Guided)"
      };
      const int methodCount = 3;
#endif
      int denoisingMethod = MathUtil::Clamp(static_cast<int>(ioSettings._DenoisingMethod), 0, methodCount - 1);
      if ( ImGui::Combo("Denoising Method", &denoisingMethod, denoisingMethods, methodCount) )
      {
        ioSettings._DenoisingMethod = static_cast<unsigned int>(denoisingMethod);
        settingsDirty = true;
      }

      if ( 0 == ioSettings._DenoisingMethod )
      {
        settingsDirty |= ImGui::SliderFloat("Sigma Spatial", &ioSettings._DenoiserSigmaSpatial, 0.1f, 10.0f);
        settingsDirty |= ImGui::SliderFloat("Sigma Range", &ioSettings._DenoiserSigmaRange, 0.01f, 1.0f);
      }
      else if ( 1 == ioSettings._DenoisingMethod )
      {
        int waveletScale = static_cast<int>(ioSettings._DenoisingWaveletScale);
        if ( ImGui::SliderInt("Wavelet Scale", &waveletScale, 1, 5) )
        {
          ioSettings._DenoisingWaveletScale = static_cast<unsigned int>(waveletScale);
          settingsDirty = true;
        }
        settingsDirty |= ImGui::SliderFloat("Threshold", &ioSettings._DenoiserThreshold, 0.01f, 1.0f);
      }
      else if ( 2 == ioSettings._DenoisingMethod )
      {
        settingsDirty |= ImGui::SliderFloat("Color Phi", &ioSettings._DenoiserColorPhi, 0.01f, 1.0f);
        settingsDirty |= ImGui::SliderFloat("Normal Phi", &ioSettings._DenoiserNormalPhi, 0.01f, 1.0f);
        settingsDirty |= ImGui::SliderFloat("Position Phi", &ioSettings._DenoiserPositionPhi, 0.01f, 1.0f);
      }
    }
  }
  else if ( ioRenderer && ioRenderer->AsSoftwareRasterizer() )
  {
    ImGui::Separator();
    ImGui::TextDisabled("Software Rasterizer Features");
    settingsDirty |= ImGui::Checkbox("Tiled Multithreading", &ioSettings._TiledRendering);
    ImGui::SameLine();
    settingsDirty |= ImGui::Checkbox("Transparency", &ioSettings._Transparency);
  }

  if ( settingsDirty && ioRenderer )
    ioRenderer->Notify(DirtyState::RenderSettings);

  ImGui::Separator();
  RenderStatsUI::DrawFrameRateGraph(ioStatsState, iFrameRate, iDeltaTime, iNbRenderedFrames);
  RenderStatsUI::DrawRenderOverview(ioSettings, iDeltaTime, iNbRenderedFrames);
  RenderStatsUI::DrawRenderPassTimings(ioRenderer);
  RenderStatsUI::DrawSceneStats(&ioScene);
}

// ----------------------------------------------------------------------------
// SetWindowFullscreen
// ----------------------------------------------------------------------------
void SetWindowFullscreen( GLFWwindow * iWindow, bool iFullscreen, RenderSettings & ioSettings )
{
  if ( !iWindow )
  {
    ioSettings._FullScreen = iFullscreen;
    return;
  }

  static int s_SavedWindowedX = 100;
  static int s_SavedWindowedY = 100;
  static int s_SavedWindowedW = 1280;
  static int s_SavedWindowedH = 720;

  const bool isCurrentlyFullscreen = ( glfwGetWindowMonitor(iWindow) != nullptr );
  if ( iFullscreen == isCurrentlyFullscreen )
  {
    ioSettings._FullScreen = iFullscreen;
    return;
  }

  if ( iFullscreen )
  {
    glfwGetWindowPos(iWindow, &s_SavedWindowedX, &s_SavedWindowedY);
    glfwGetWindowSize(iWindow, &s_SavedWindowedW, &s_SavedWindowedH);
    if ( ( s_SavedWindowedW <= 0 ) || ( s_SavedWindowedH <= 0 ) )
    {
      s_SavedWindowedW = ( ioSettings._WindowResolution.x > 0 ) ? ioSettings._WindowResolution.x : 1280;
      s_SavedWindowedH = ( ioSettings._WindowResolution.y > 0 ) ? ioSettings._WindowResolution.y : 720;
    }

    GLFWmonitor * targetMonitor = glfwGetPrimaryMonitor();
    int monitorCount = 0;
    GLFWmonitor ** monitors = glfwGetMonitors(&monitorCount);
    for ( int i = 0; i < monitorCount; ++i )
    {
      int mx = 0, my = 0, mw = 0, mh = 0;
      glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
      if ( ( s_SavedWindowedX >= mx ) && ( s_SavedWindowedX < mx + mw ) &&
           ( s_SavedWindowedY >= my ) && ( s_SavedWindowedY < my + mh ) )
      {
        targetMonitor = monitors[i];
        break;
      }
    }

    if ( targetMonitor )
    {
      const GLFWvidmode * mode = glfwGetVideoMode(targetMonitor);
      if ( mode )
      {
        glfwSetWindowMonitor(iWindow, targetMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
      }
    }
    ioSettings._FullScreen = true;
  }
  else
  {
    glfwSetWindowMonitor(iWindow, nullptr, s_SavedWindowedX, s_SavedWindowedY, s_SavedWindowedW, s_SavedWindowedH, 0);
    ioSettings._FullScreen = false;
  }

  glfwSwapInterval(ioSettings._VSync ? 1 : 0);
}

// ----------------------------------------------------------------------------
// ToggleWindowFullscreen
// ----------------------------------------------------------------------------
void ToggleWindowFullscreen( GLFWwindow * iWindow, RenderSettings & ioSettings )
{
  if ( !iWindow )
    return;
  const bool isCurrentlyFullscreen = ( glfwGetWindowMonitor(iWindow) != nullptr );
  SetWindowFullscreen(iWindow, !isCurrentlyFullscreen, ioSettings);
}

} // namespace GameMenuUI

} // namespace RTRT
