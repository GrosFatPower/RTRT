#include "Localization.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// English String Table
// ----------------------------------------------------------------------------
static const LocStringEntry s_EnglishEntries[] =
{
  // Common / Actions
  { LocId::Common_Back, "Common_Back", "Back" },
  { LocId::Common_Confirm, "Common_Confirm", "Confirm" },
  { LocId::Common_Cancel, "Common_Cancel", "Cancel" },
  { LocId::Common_Apply, "Common_Apply", "Apply" },
  { LocId::Common_Close, "Common_Close", "Close" },
  { LocId::Common_Default, "Common_Default", "Default" },
  { LocId::Common_Yes, "Common_Yes", "Yes" },
  { LocId::Common_No, "Common_No", "No" },
  { LocId::Common_On, "Common_On", "On" },
  { LocId::Common_Off, "Common_Off", "Off" },
  { LocId::Common_Custom, "Common_Custom", "Custom" },
  { LocId::Common_Preset, "Common_Preset", "Preset" },

  // Navigation / Tabs
  { LocId::Tab_Gameplay, "Tab_Gameplay", "GAMEPLAY" },
  { LocId::Tab_Graphics, "Tab_Graphics", "GRAPHICS" },
  { LocId::Tab_Audio, "Tab_Audio", "AUDIO" },
  { LocId::Tab_Controls, "Tab_Controls", "CONTROLS" },

  // Main Menu
  { LocId::Menu_Title, "Menu_Title", "TETRIS 3D" },
  { LocId::Menu_SinglePlayer, "Menu_SinglePlayer", "SINGLE PLAYER (1P)" },
  { LocId::Menu_SinglePlayerDesc, "Menu_SinglePlayerDesc", "Classic solo modes: Marathon 150L, Sprint 40L, or Ultra 2m" },
  { LocId::Menu_SinglePlayerBadge, "Menu_SinglePlayerBadge", "SOLO" },
  { LocId::Menu_Multiplayer, "Menu_Multiplayer", "MULTIPLAYER (2P)" },
  { LocId::Menu_MultiplayerDesc, "Menu_MultiplayerDesc", "Play head-to-head on local split-screen or online over network" },
  { LocId::Menu_MultiplayerBadge, "Menu_MultiplayerBadge", "VERSUS" },
  { LocId::Menu_QuickStart, "Menu_QuickStart", "QUICK START (1P)" },
  { LocId::Menu_QuickStartDesc, "Menu_QuickStartDesc", "Jump directly into a Solo Marathon match at Level 1" },
  { LocId::Menu_QuickStartBadge, "Menu_QuickStartBadge", "FAST" },
  { LocId::Menu_HighScores, "Menu_HighScores", "HIGH SCORES & STATS" },
  { LocId::Menu_HighScoresDesc, "Menu_HighScoresDesc", "View personal bests, sprint records, and gameplay statistics" },
  { LocId::Menu_Options, "Menu_Options", "OPTIONS" },
  { LocId::Menu_OptionsDesc, "Menu_OptionsDesc", "Configure handling (DAS/ARR), renderer, audio, and controls" },
  { LocId::Menu_Exit, "Menu_Exit", "EXIT TO DESKTOP" },
  { LocId::Menu_ExitDesc, "Menu_ExitDesc", "Close the game application" },

  // Mode Select
  { LocId::ModeSelect_Title, "ModeSelect_Title", "SELECT GAME MODE" },
  { LocId::Mode_Marathon, "Mode_Marathon", "MARATHON (150 LINES)" },
  { LocId::Mode_MarathonDesc, "Mode_MarathonDesc", "Clear 150 lines with progressively faster gravity drops" },
  { LocId::Mode_MarathonBadge, "Mode_MarathonBadge", "ENDURANCE" },
  { LocId::Mode_Sprint, "Mode_Sprint", "SPRINT (40 LINES)" },
  { LocId::Mode_SprintDesc, "Mode_SprintDesc", "Race against the clock to clear 40 lines as fast as possible" },
  { LocId::Mode_SprintBadge, "Mode_SprintBadge", "TIME ATTACK" },
  { LocId::Mode_Ultra, "Mode_Ultra", "ULTRA (2 MINUTES)" },
  { LocId::Mode_UltraDesc, "Mode_UltraDesc", "Score as many points as possible before the 2-minute timer expires" },
  { LocId::Mode_UltraBadge, "Mode_UltraBadge", "SCORE ATTACK" },
  { LocId::Mode_BackToMain, "Mode_BackToMain", "BACK TO MAIN MENU" },
  { LocId::Mode_BackToMainDesc, "Mode_BackToMainDesc", "Return to the title screen" },

  // Multiplayer Type Select
  { LocId::MpSelect_Title, "MpSelect_Title", "SELECT MULTIPLAYER TYPE" },
  { LocId::Mp_Local, "Mp_Local", "LOCAL SPLIT-SCREEN" },
  { LocId::Mp_LocalDesc, "Mp_LocalDesc", "Two players on the same machine sharing keyboard / gamepads" },
  { LocId::Mp_LocalBadge, "Mp_LocalBadge", "SAME PC" },
  { LocId::Mp_Online, "Mp_Online", "ONLINE NETWORK" },
  { LocId::Mp_OnlineDesc, "Mp_OnlineDesc", "Host or join a match across local LAN or the internet" },
  { LocId::Mp_OnlineBadge, "Mp_OnlineBadge", "NETPLAY" },

  // Multiplayer & Online Lobby
  { LocId::Lobby_LocalTitle, "Lobby_LocalTitle", "LOCAL MULTIPLAYER LOBBY" },
  { LocId::Lobby_OnlineTitle, "Lobby_OnlineTitle", "ONLINE MULTIPLAYER LOBBY" },
  { LocId::Lobby_Player1, "Lobby_Player1", "Player 1" },
  { LocId::Lobby_Player2, "Lobby_Player2", "Player 2" },
  { LocId::Lobby_Ready, "Lobby_Ready", "READY" },
  { LocId::Lobby_NotReady, "Lobby_NotReady", "NOT READY" },
  { LocId::Lobby_StartMatch, "Lobby_StartMatch", "START MATCH" },
  { LocId::Lobby_HostRoom, "Lobby_HostRoom", "HOST ROOM" },
  { LocId::Lobby_JoinRoom, "Lobby_JoinRoom", "JOIN ROOM" },
  { LocId::Lobby_RoomCode, "Lobby_RoomCode", "Room Code" },
  { LocId::Lobby_ConnectDirect, "Lobby_ConnectDirect", "Direct IP Connect" },
  { LocId::Lobby_Disconnect, "Lobby_Disconnect", "DISCONNECT" },
  { LocId::Lobby_FirstTo, "Lobby_FirstTo", "First To" },
  { LocId::Lobby_StartLevel, "Lobby_StartLevel", "Starting Level" },

  // In-Game HUD
  { LocId::Hud_Score, "Hud_Score", "SCORE" },
  { LocId::Hud_Level, "Hud_Level", "LEVEL" },
  { LocId::Hud_Lines, "Hud_Lines", "LINES" },
  { LocId::Hud_Time, "Hud_Time", "TIME" },
  { LocId::Hud_PPS, "Hud_PPS", "PPS" },
  { LocId::Hud_Hold, "Hud_Hold", "HOLD" },
  { LocId::Hud_Next, "Hud_Next", "NEXT" },
  { LocId::Hud_Garbage, "Hud_Garbage", "GARBAGE" },
  { LocId::Hud_Paused, "Hud_Paused", "PAUSED" },
  { LocId::Hud_GameOver, "Hud_GameOver", "GAME OVER" },
  { LocId::Hud_Victory, "Hud_Victory", "VICTORY!" },
  { LocId::Hud_Winner, "Hud_Winner", "WINNER!" },
  { LocId::Hud_Combo, "Hud_Combo", "Combo" },
  { LocId::Hud_BackToBack, "Hud_BackToBack", "Back-to-Back" },
  { LocId::Hud_PerfectClear, "Hud_PerfectClear", "PERFECT CLEAR!" },
  { LocId::Hud_TSpin, "Hud_TSpin", "T-SPIN" },
  { LocId::Hud_Tetris, "Hud_Tetris", "TETRIS!" },

  // In-Game Modals / Pause Menu
  { LocId::Modal_PausedTitle, "Modal_PausedTitle", "GAME PAUSED" },
  { LocId::Modal_Resume, "Modal_Resume", "RESUME GAME" },
  { LocId::Modal_ResumeDesc, "Modal_ResumeDesc", "Continue playing" },
  { LocId::Modal_Restart, "Modal_Restart", "RESTART MATCH" },
  { LocId::Modal_RestartDesc, "Modal_RestartDesc", "Restart the current mode from level 1" },
  { LocId::Modal_Options, "Modal_Options", "OPTIONS" },
  { LocId::Modal_OptionsDesc, "Modal_OptionsDesc", "Adjust gameplay, audio, and graphics" },
  { LocId::Modal_MainMenu, "Modal_MainMenu", "RETURN TO MENU" },
  { LocId::Modal_MainMenuDesc, "Modal_MainMenuDesc", "Leave match and return to title screen" },
  { LocId::Modal_Exit, "Modal_Exit", "EXIT GAME" },
  { LocId::Modal_ExitDesc, "Modal_ExitDesc", "Close application" },

  // High Scores
  { LocId::HighScores_Title, "HighScores_Title", "LEADERBOARDS & PERSONAL BESTS" },
  { LocId::HighScores_NoRecords, "HighScores_NoRecords", "No records recorded yet. Play a game to record your score!" },

  // Options - General / Gameplay
  { LocId::Option_Title, "Option_Title", "CONFIGURATION & SETTINGS" },
  { LocId::Option_Language, "Option_Language", "Language" },
  { LocId::Option_HandlingHeader, "Option_HandlingHeader", "Handling Settings" },
  { LocId::Option_DAS, "Option_DAS", "DAS (Delayed Auto Shift)" },
  { LocId::Option_ARR, "Option_ARR", "ARR (Auto Repeat Rate)" },
  { LocId::Option_SDF, "Option_SDF", "SDF (Soft Drop Factor)" },
  { LocId::Option_LockDelay, "Option_LockDelay", "Lock Delay" },
  { LocId::Option_PresetGuideline, "Option_PresetGuideline", "Guideline Default" },
  { LocId::Option_PresetCompetitive, "Option_PresetCompetitive", "Competitive Fast" },
  { LocId::Option_AssistsHeader, "Option_AssistsHeader", "Assists & Visual Aids" },
  { LocId::Option_ShowGhost, "Option_ShowGhost", "Show Ghost Piece" },
  { LocId::Option_StartLevel, "Option_StartLevel", "Default Start Level" },

  // Options - Visuals & Materials
  { LocId::Option_VisualsHeader, "Option_VisualsHeader", "Visuals & Presentation" },
  { LocId::Option_MaterialPreset, "Option_MaterialPreset", "Tetromino Material Preset" },
  { LocId::Option_MatGlossy, "Option_MatGlossy", "Glossy Plastic" },
  { LocId::Option_MatGlass, "Option_MatGlass", "Frosted Glass" },
  { LocId::Option_MatNeon, "Option_MatNeon", "Neon Glow" },
  { LocId::Option_MatMetal, "Option_MatMetal", "Brushed Metal" },
  { LocId::Option_CameraPreset, "Option_CameraPreset", "Camera Perspective" },
  { LocId::Option_CamFrontal, "Option_CamFrontal", "Classic Frontal" },
  { LocId::Option_CamIsometric, "Option_CamIsometric", "Isometric 3D" },
  { LocId::Option_CamOrbit, "Option_CamOrbit", "Dynamic Orbit" },
  { LocId::Option_ClearAnim, "Option_ClearAnim", "Line Clear Effect Duration" },
  { LocId::Option_BackplateOpacity, "Option_BackplateOpacity", "Board Backplate Opacity" },
  { LocId::Option_BoardEdgeColor, "Option_BoardEdgeColor", "Board Frame Color" },
  { LocId::Option_EdgePresets, "Option_EdgePresets", "Edge Presets" },
  { LocId::Option_DynamicLighting, "Option_DynamicLighting", "Dynamic Neon Lighting" },
  { LocId::Option_CameraMotion, "Option_CameraMotion", "Camera Motion Mode" },
  { LocId::Option_MotionSpeed, "Option_MotionSpeed", "Camera Sway Speed" },
  { LocId::Option_BoardSway, "Option_BoardSway", "Board Sway Amplitude" },
  { LocId::Option_HdrBackground, "Option_HdrBackground", "HDR Skybox Environment" },
  { LocId::Option_NextBackground, "Option_NextBackground", "Cycle Skybox" },
  { LocId::Option_RotateBgLevel, "Option_RotateBgLevel", "Auto-Rotate Skybox by Level" },
  { LocId::Option_SkyboxRotation, "Option_SkyboxRotation", "Skybox Ambient Rotation" },

  // Options - Engine & Renderer
  { LocId::Option_EngineHeader, "Option_EngineHeader", "Rendering Pipeline & Display" },
  { LocId::Option_ActiveRenderer, "Option_ActiveRenderer", "Active Renderer Backend" },
  { LocId::Option_RenderResolution, "Option_RenderResolution", "Render Resolution" },
  { LocId::Option_RenderScale, "Option_RenderScale", "Render Resolution Scale (%)" },
  { LocId::Option_Fullscreen, "Option_Fullscreen", "Fullscreen Mode" },
  { LocId::Option_VSync, "Option_VSync", "Vertical Sync (VSync)" },
  { LocId::Option_ToneMapping, "Option_ToneMapping", "Tone Mapping Operator" },
  { LocId::Option_FXAA, "Option_FXAA", "Fast Approximate AA (FXAA)" },
  { LocId::Option_Skybox, "Option_Skybox", "Enable Skybox Rendering" },
  { LocId::Option_Exposure, "Option_Exposure", "Exposure" },
  { LocId::Option_Gamma, "Option_Gamma", "Gamma Correction" },
  { LocId::Option_DeferredFeatures, "Option_DeferredFeatures", "Deferred OpenGL Features" },
  { LocId::Option_ShadowMapping, "Option_ShadowMapping", "Directional Shadow Mapping" },
  { LocId::Option_SSAO, "Option_SSAO", "Screen-Space Ambient Occlusion (SSAO)" },
  { LocId::Option_SSR, "Option_SSR", "Screen-Space Reflections (SSR)" },
  { LocId::Option_Transparency, "Option_Transparency", "Order-Independent Transparency" },
  { LocId::Option_Refraction, "Option_Refraction", "Screen-Space Refraction" },
  { LocId::Option_PathTracerFeatures, "Option_PathTracerFeatures", "GPU Path Tracer Settings" },
  { LocId::Option_MaxBounces, "Option_MaxBounces", "Maximum Ray Bounces" },
  { LocId::Option_SamplesPerPixel, "Option_SamplesPerPixel", "Samples Per Pixel (SPP)" },
  { LocId::Option_Accumulation, "Option_Accumulation", "Temporal Accumulation" },
  { LocId::Option_Denoiser, "Option_Denoiser", "Spatial Wavelet Denoiser" },
  { LocId::Option_SoftwareFeatures, "Option_SoftwareFeatures", "CPU Software Rasterizer" },
  { LocId::Option_TiledRendering, "Option_TiledRendering", "Multi-Threaded Tiled Rendering" },

  // Options - Audio
  { LocId::Option_AudioHeader, "Option_AudioHeader", "Audio & Volume Levels" },
  { LocId::Option_MuteAll, "Option_MuteAll", "Mute All Audio" },
  { LocId::Option_MasterVolume, "Option_MasterVolume", "Master Volume" },
  { LocId::Option_SfxVolume, "Option_SfxVolume", "Sound Effects (SFX)" },
  { LocId::Option_MusicVolume, "Option_MusicVolume", "Music Volume" },
  { LocId::Option_TestSfx, "Option_TestSfx", "Play Test Sound" },

  // Options - Controls
  { LocId::Option_ControlsHeader, "Option_ControlsHeader", "Input Bindings & Gamepad" },
  { LocId::Option_ResetControls, "Option_ResetControls", "Reset to Default Controls" }
};

const LocStringEntry * GetEnglishEntries( size_t & oCount )
{
  oCount = sizeof(s_EnglishEntries) / sizeof(s_EnglishEntries[0]);
  return s_EnglishEntries;
}

} // namespace RTRT
