#ifndef _Localization_
#define _Localization_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Language
// ----------------------------------------------------------------------------
enum class Language : int
{
  English = 0,
  French,
  Count
};

// ----------------------------------------------------------------------------
// LanguageInfo
// ----------------------------------------------------------------------------
struct LanguageInfo
{
  Language    _Id;
  const char * _IsoCode;      // "en", "fr"
  const char * _DisplayName;  // "English", "Français"
};

// ----------------------------------------------------------------------------
// LocId (Strongly-Typed String Keys)
// ----------------------------------------------------------------------------
enum class LocId : uint32_t
{
  // Common / Actions
  Common_Back = 0,
  Common_Confirm,
  Common_Cancel,
  Common_Apply,
  Common_Close,
  Common_Default,
  Common_Yes,
  Common_No,
  Common_On,
  Common_Off,
  Common_Custom,
  Common_Preset,

  // Navigation / Tabs
  Tab_Gameplay,
  Tab_Graphics,
  Tab_Audio,
  Tab_Controls,

  // Main Menu
  Menu_Title,
  Menu_SinglePlayer,
  Menu_SinglePlayerDesc,
  Menu_SinglePlayerBadge,
  Menu_Multiplayer,
  Menu_MultiplayerDesc,
  Menu_MultiplayerBadge,
  Menu_QuickStart,
  Menu_QuickStartDesc,
  Menu_QuickStartBadge,
  Menu_HighScores,
  Menu_HighScoresDesc,
  Menu_Options,
  Menu_OptionsDesc,
  Menu_Exit,
  Menu_ExitDesc,

  // Mode Select
  ModeSelect_Title,
  Mode_Marathon,
  Mode_MarathonDesc,
  Mode_MarathonBadge,
  Mode_Sprint,
  Mode_SprintDesc,
  Mode_SprintBadge,
  Mode_Ultra,
  Mode_UltraDesc,
  Mode_UltraBadge,
  Mode_BackToMain,
  Mode_BackToMainDesc,

  // Multiplayer Type Select
  MpSelect_Title,
  Mp_Local,
  Mp_LocalDesc,
  Mp_LocalBadge,
  Mp_Online,
  Mp_OnlineDesc,
  Mp_OnlineBadge,

  // Multiplayer & Online Lobby
  Lobby_LocalTitle,
  Lobby_OnlineTitle,
  Lobby_Player1,
  Lobby_Player2,
  Lobby_Ready,
  Lobby_NotReady,
  Lobby_StartMatch,
  Lobby_HostRoom,
  Lobby_JoinRoom,
  Lobby_RoomCode,
  Lobby_ConnectDirect,
  Lobby_Disconnect,
  Lobby_FirstTo,
  Lobby_StartLevel,

  // In-Game HUD
  Hud_Score,
  Hud_Level,
  Hud_Lines,
  Hud_Time,
  Hud_PPS,
  Hud_Hold,
  Hud_Next,
  Hud_Garbage,
  Hud_Paused,
  Hud_GameOver,
  Hud_Victory,
  Hud_Winner,
  Hud_Combo,
  Hud_BackToBack,
  Hud_PerfectClear,
  Hud_TSpin,
  Hud_Tetris,

  // In-Game Modals / Pause Menu
  Modal_PausedTitle,
  Modal_Resume,
  Modal_ResumeDesc,
  Modal_Restart,
  Modal_RestartDesc,
  Modal_Options,
  Modal_OptionsDesc,
  Modal_MainMenu,
  Modal_MainMenuDesc,
  Modal_Exit,
  Modal_ExitDesc,

  // High Scores
  HighScores_Title,
  HighScores_NoRecords,

  // Options - General / Gameplay
  Option_Title,
  Option_Language,
  Option_HandlingHeader,
  Option_DAS,
  Option_ARR,
  Option_SDF,
  Option_LockDelay,
  Option_PresetGuideline,
  Option_PresetCompetitive,
  Option_AssistsHeader,
  Option_ShowGhost,
  Option_StartLevel,

  // Options - Visuals & Materials
  Option_VisualsHeader,
  Option_MaterialPreset,
  Option_MatGlossy,
  Option_MatGlass,
  Option_MatNeon,
  Option_MatMetal,
  Option_CameraPreset,
  Option_CamFrontal,
  Option_CamIsometric,
  Option_CamOrbit,
  Option_ClearAnim,
  Option_BackplateOpacity,
  Option_BoardEdgeColor,
  Option_EdgePresets,
  Option_DynamicLighting,
  Option_CameraMotion,
  Option_MotionSpeed,
  Option_BoardSway,
  Option_HdrBackground,
  Option_NextBackground,
  Option_RotateBgLevel,
  Option_SkyboxRotation,

  // Options - Engine & Renderer
  Option_EngineHeader,
  Option_ActiveRenderer,
  Option_RenderResolution,
  Option_RenderScale,
  Option_Fullscreen,
  Option_VSync,
  Option_ToneMapping,
  Option_FXAA,
  Option_Skybox,
  Option_Exposure,
  Option_Gamma,
  Option_DeferredFeatures,
  Option_ShadowMapping,
  Option_SSAO,
  Option_SSR,
  Option_Transparency,
  Option_Refraction,
  Option_PathTracerFeatures,
  Option_MaxBounces,
  Option_SamplesPerPixel,
  Option_Accumulation,
  Option_Denoiser,
  Option_SoftwareFeatures,
  Option_TiledRendering,

  // Options - Audio
  Option_AudioHeader,
  Option_MuteAll,
  Option_MasterVolume,
  Option_SfxVolume,
  Option_MusicVolume,
  Option_TestSfx,

  // Options - Controls
  Option_ControlsHeader,
  Option_ResetControls,

  // FPS Game - Main Menu & Headers
  Fps_Title,
  Fps_Subtitle,
  Fps_MainMenuTitle,
  Fps_Menu_Deploy,
  Fps_Menu_DeployDesc,
  Fps_Menu_DeployBadge,
  Fps_Menu_QuickStart,
  Fps_Menu_QuickStartDesc,
  Fps_Menu_QuickStartBadge,
  Fps_Menu_Editor,
  Fps_Menu_EditorDesc,
  Fps_Menu_Options,
  Fps_Menu_OptionsDesc,
  Fps_Menu_Exit,
  Fps_Menu_ExitDesc,
  Fps_Footer_Main,

  // FPS Game - Map Select
  Fps_Map_Title,
  Fps_Map_CityTitle,
  Fps_Map_CityDesc,
  Fps_Map_DefaultTitle,
  Fps_Map_DefaultDesc,
  Fps_Map_CustomTitle,
  Fps_Map_CustomSubtitle,
  Fps_Map_CustomDesc,
  Fps_Map_Active,
  Fps_Map_Available,
  Fps_Map_FileDialog,
  Fps_Map_FreeLook,
  Fps_Map_ShowWeapon,
  Fps_Map_Back,
  Fps_Map_Deploy,
  Fps_Footer_MapSelectLeft,
  Fps_Footer_MapSelect,

  // FPS Game - Options
  Fps_Opt_Title,
  Fps_Opt_Movement,
  Fps_Opt_MoveSpeed,
  Fps_Opt_SprintSpeed,
  Fps_Opt_MouseSensitivity,
  Fps_Opt_FOV,
  Fps_Opt_Gravity,
  Fps_Opt_JumpSpeed,
  Fps_Opt_FreeLook,
  Fps_Opt_HeadBobHeader,
  Fps_Opt_HeadBobEnabled,
  Fps_Opt_BobAmplitude,
  Fps_Opt_BobFrequency,
  Fps_Opt_BobSway,
  Fps_Opt_ProjectilesHeader,
  Fps_Opt_ProjSpeed,
  Fps_Opt_ProjGravity,
  Fps_Opt_ProjBounciness,
  Fps_Opt_ProjCooldown,
  Fps_Opt_MaxProjectiles,
  Fps_Opt_ShowViewWeapon,
  Fps_Opt_EngineHeader,
  Fps_Opt_DevTools,
  Fps_Opt_DevPanels,
  Fps_Opt_AudioNotAvail,
  Fps_Opt_GamepadHeader,
  Fps_Opt_GamepadConnected,
  Fps_Opt_GamepadNotFound,
  Fps_Opt_Deadzone,
  Fps_Opt_LookSpeed,
  Fps_Opt_BackBtn,
  Fps_Footer_Options,

  // FPS Game - Keybindings
  Fps_Key_CategoryMovement,
  Fps_Key_CategoryCombat,
  Fps_Key_CategorySystem,
  Fps_Key_Move,
  Fps_Key_Strafe,
  Fps_Key_Look,
  Fps_Key_Jump,
  Fps_Key_Sprint,
  Fps_Key_Fire,
  Fps_Key_Respawn,
  Fps_Key_Pause,
  Fps_Key_Options,
  Fps_Key_ToggleFreeLook,
  Fps_Key_MapEditor,
  Fps_Key_DevPanels,
  Fps_Key_SwitchRenderer,
  Fps_Key_PhotoMode,

  // FPS Game - Pause Modal
  Fps_Pause_Title,
  Fps_Pause_Resume,
  Fps_Pause_ResumeDesc,
  Fps_Pause_PhotoMode,
  Fps_Pause_PhotoModeDesc,
  Fps_Pause_Respawn,
  Fps_Pause_RespawnDesc,
  Fps_Pause_SwitchMap,
  Fps_Pause_SwitchMapDesc,
  Fps_Pause_EnterEditor,
  Fps_Pause_EnterEditorDesc,
  Fps_Pause_Options,
  Fps_Pause_OptionsDesc,
  Fps_Pause_MainMenu,
  Fps_Pause_MainMenuDesc,

  // FPS Game - Photo Mode
  Fps_Photo_SetupTitle,
  Fps_Photo_CameraHeader,
  Fps_Photo_FOV,
  Fps_Photo_ShowWeapon,
  Fps_Photo_ResetCam,
  Fps_Photo_ToneMappingHeader,
  Fps_Photo_ToneMapping,
  Fps_Photo_Exposure,
  Fps_Photo_Gamma,
  Fps_Photo_PtHeader,
  Fps_Photo_Passes,
  Fps_Photo_SPP,
  Fps_Photo_Bounces,
  Fps_Photo_Denoise,
  Fps_Photo_DenoiserCombo,
  Fps_Photo_CaptureBtn,
  Fps_Photo_ExitBtn,
  Fps_Photo_FooterPreviewTitle,
  Fps_Photo_FooterSetup,
  Fps_Photo_CapturingTitle,
  Fps_Photo_Accumulating,
  Fps_Photo_CancelBtn,
  Fps_Photo_CapturedTitle,
  Fps_Photo_SavedTo,
  Fps_Photo_SaveFailed,
  Fps_Photo_ReframeBtn,
  Fps_Photo_ExitPreviewBtn,

  // FPS Game - Gameplay HUD & Renderers
  Fps_Hud_Health,
  Fps_Hud_Armor,
  Fps_Hud_Projectiles,
  Fps_Hud_Hint,
  Fps_Renderer_Software,
  Fps_Renderer_Deferred,

  // FPS Game - Menu Bar
  Fps_Bar_File,
  Fps_Bar_EnterEditor,
  Fps_Bar_ExitEditor,
  Fps_Bar_OpenMap,
  Fps_Bar_Save,
  Fps_Bar_SaveAs,
  Fps_Bar_LoadProp,
  Fps_Bar_Exit,
  Fps_Bar_View,
  Fps_Bar_Debug,
  Fps_Bar_RenderSettings,
  Fps_Bar_RenderStats,
  Fps_Bar_Render,
  Fps_Bar_CaptureImage,
  Fps_Bar_Help,
  Fps_Bar_About,

  Count
};

// ----------------------------------------------------------------------------
// LocStringEntry
// ----------------------------------------------------------------------------
struct LocStringEntry
{
  LocId        _Id;
  const char * _Key;
  const char * _Text;
};

const LocStringEntry * GetEnglishEntries( size_t & oCount );
const LocStringEntry * GetFrenchEntries( size_t & oCount );

// ----------------------------------------------------------------------------
// LocalizationManager
// ----------------------------------------------------------------------------
class LocalizationManager
{
public:
  static LocalizationManager & Instance();

  void Initialize();
  void SetLanguage( Language iLang );
  Language GetLanguage() const;

  const LanguageInfo & GetLanguageInfo( Language iLang ) const;
  const std::vector<LanguageInfo> & GetAvailableLanguages() const;

  // Primary O(1) indexed lookup with fallback to English
  const char * Get( LocId iId ) const;

  // Dynamic string-key lookup (for external locale files / modding)
  const char * Get( const std::string & iKey, const char * iDefaultVal = nullptr ) const;

  // Load an external JSON translation file
  bool LoadLocaleFile( const std::string & iFilePath );

  // Check if string table is complete for a language (useful for unit tests)
  bool IsComplete( Language iLang ) const;

private:
  LocalizationManager();
  ~LocalizationManager() = default;
  LocalizationManager( const LocalizationManager & ) = delete;
  LocalizationManager & operator=( const LocalizationManager & ) = delete;

  Language                                     _CurrentLanguage = Language::English;
  std::vector<LanguageInfo>                    _AvailableLanguages;
  std::vector<const char *>                    _StringTables[static_cast<size_t>(Language::Count)];
  std::unordered_map<std::string, LocId>       _KeyToId;
  std::unordered_map<std::string, std::string> _CustomStrings;
  bool                                         _Initialized = false;
};

// ----------------------------------------------------------------------------
// Loc Facade
// ----------------------------------------------------------------------------
class Loc
{
public:
  static inline const char * Tr( LocId iId )
  {
    return LocalizationManager::Instance().Get(iId);
  }

  static inline const char * Tr( const std::string & iKey, const char * iDefaultVal = nullptr )
  {
    return LocalizationManager::Instance().Get(iKey, iDefaultVal);
  }

  static inline void SetLanguage( Language iLang )
  {
    LocalizationManager::Instance().SetLanguage(iLang);
  }

  static inline Language GetLanguage()
  {
    return LocalizationManager::Instance().GetLanguage();
  }

  static inline const std::vector<LanguageInfo> & GetAvailableLanguages()
  {
    return LocalizationManager::Instance().GetAvailableLanguages();
  }
};

} // namespace RTRT

#endif /* _Localization_ */
