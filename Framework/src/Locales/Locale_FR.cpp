#include "Localization.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// French String Table
// ----------------------------------------------------------------------------
static const LocStringEntry s_FrenchEntries[] =
{
  // Common / Actions
  { LocId::Common_Back, "Common_Back", "Retour" },
  { LocId::Common_Confirm, "Common_Confirm", "Confirmer" },
  { LocId::Common_Cancel, "Common_Cancel", "Annuler" },
  { LocId::Common_Apply, "Common_Apply", "Appliquer" },
  { LocId::Common_Close, "Common_Close", "Fermer" },
  { LocId::Common_Default, "Common_Default", "Par défaut" },
  { LocId::Common_Yes, "Common_Yes", "Oui" },
  { LocId::Common_No, "Common_No", "Non" },
  { LocId::Common_On, "Common_On", "Activé" },
  { LocId::Common_Off, "Common_Off", "Désactivé" },
  { LocId::Common_Custom, "Common_Custom", "Personnalisé" },
  { LocId::Common_Preset, "Common_Preset", "Préréglage" },

  // Navigation / Tabs
  { LocId::Tab_Gameplay, "Tab_Gameplay", "JOUABILITÉ" },
  { LocId::Tab_Graphics, "Tab_Graphics", "GRAPHISMES" },
  { LocId::Tab_Audio, "Tab_Audio", "AUDIO" },
  { LocId::Tab_Controls, "Tab_Controls", "COMMANDES" },

  // Main Menu
  { LocId::Menu_Title, "Menu_Title", "TETRIS 3D" },
  { LocId::Menu_SinglePlayer, "Menu_SinglePlayer", "PARTIE SOLO (1J)" },
  { LocId::Menu_SinglePlayerDesc, "Menu_SinglePlayerDesc", "Modes solo : Marathon 150L, Sprint 40L ou Ultra 2m" },
  { LocId::Menu_SinglePlayerBadge, "Menu_SinglePlayerBadge", "SOLO" },
  { LocId::Menu_Multiplayer, "Menu_Multiplayer", "MULTIJOUEUR (2J)" },
  { LocId::Menu_MultiplayerDesc, "Menu_MultiplayerDesc", "Affrontement à 2 en écran partagé local ou en ligne" },
  { LocId::Menu_MultiplayerBadge, "Menu_MultiplayerBadge", "VERSUS" },
  { LocId::Menu_QuickStart, "Menu_QuickStart", "PARTIE RAPIDE (1J)" },
  { LocId::Menu_QuickStartDesc, "Menu_QuickStartDesc", "Lancer directement le mode Marathon solo au niveau 1" },
  { LocId::Menu_QuickStartBadge, "Menu_QuickStartBadge", "RAPIDE" },
  { LocId::Menu_HighScores, "Menu_HighScores", "MEILLEURS SCORES & STATS" },
  { LocId::Menu_HighScoresDesc, "Menu_HighScoresDesc", "Consulter les records personnels et les statistiques" },
  { LocId::Menu_Options, "Menu_Options", "OPTIONS" },
  { LocId::Menu_OptionsDesc, "Menu_OptionsDesc", "Configurer la maniabilité (DAS/ARR), le rendu, l'audio et les touches" },
  { LocId::Menu_Exit, "Menu_Exit", "QUITTER VERS LE BUREAU" },
  { LocId::Menu_ExitDesc, "Menu_ExitDesc", "Fermer l'application" },

  // Mode Select
  { LocId::ModeSelect_Title, "ModeSelect_Title", "SÉLECTION DU MODE DE JEU" },
  { LocId::Mode_Marathon, "Mode_Marathon", "MARATHON (150 LIGNES)" },
  { LocId::Mode_MarathonDesc, "Mode_MarathonDesc", "Éliminez 150 lignes avec une vitesse de chute croissante" },
  { LocId::Mode_MarathonBadge, "Mode_MarathonBadge", "ENDURANCE" },
  { LocId::Mode_Sprint, "Mode_Sprint", "SPRINT (40 LIGNES)" },
  { LocId::Mode_SprintDesc, "Mode_SprintDesc", "Course contre la montre : éliminez 40 lignes le plus vite possible" },
  { LocId::Mode_SprintBadge, "Mode_SprintBadge", "CONTRE-LA-MONTRE" },
  { LocId::Mode_Ultra, "Mode_Ultra", "ULTRA (2 MINUTES)" },
  { LocId::Mode_UltraDesc, "Mode_UltraDesc", "Marquez un maximum de points en 120 secondes chrono" },
  { LocId::Mode_UltraBadge, "Mode_UltraBadge", "SCORE ATTACK" },
  { LocId::Mode_BackToMain, "Mode_BackToMain", "RETOUR AU MENU PRINCIPAL" },
  { LocId::Mode_BackToMainDesc, "Mode_BackToMainDesc", "Retourner à l'écran d'accueil" },

  // Multiplayer Type Select
  { LocId::MpSelect_Title, "MpSelect_Title", "SÉLECTION DU MODE MULTIJOUEUR" },
  { LocId::Mp_Local, "Mp_Local", "ÉCRAN PARTAGÉ LOCAL" },
  { LocId::Mp_LocalDesc, "Mp_LocalDesc", "Deux joueurs sur la même machine au clavier ou manettes" },
  { LocId::Mp_LocalBadge, "Mp_LocalBadge", "MÊME PC" },
  { LocId::Mp_Online, "Mp_Online", "EN LIGNE (RÉSEAU)" },
  { LocId::Mp_OnlineDesc, "Mp_OnlineDesc", "Héberger ou rejoindre une partie en réseau local ou Internet" },
  { LocId::Mp_OnlineBadge, "Mp_OnlineBadge", "EN LIGNE" },

  // Multiplayer & Online Lobby
  { LocId::Lobby_LocalTitle, "Lobby_LocalTitle", "SALON MULTIJOUEUR LOCAL" },
  { LocId::Lobby_OnlineTitle, "Lobby_OnlineTitle", "SALON MULTIJOUEUR EN LIGNE" },
  { LocId::Lobby_Player1, "Lobby_Player1", "Joueur 1" },
  { LocId::Lobby_Player2, "Lobby_Player2", "Joueur 2" },
  { LocId::Lobby_Ready, "Lobby_Ready", "PRÊT" },
  { LocId::Lobby_NotReady, "Lobby_NotReady", "PAS PRÊT" },
  { LocId::Lobby_StartMatch, "Lobby_StartMatch", "LANCER LA PARTIE" },
  { LocId::Lobby_HostRoom, "Lobby_HostRoom", "CRÉER UN SALON" },
  { LocId::Lobby_JoinRoom, "Lobby_JoinRoom", "REJOINDRE UN SALON" },
  { LocId::Lobby_RoomCode, "Lobby_RoomCode", "Code du salon" },
  { LocId::Lobby_ConnectDirect, "Lobby_ConnectDirect", "Connexion IP directe" },
  { LocId::Lobby_Disconnect, "Lobby_Disconnect", "DÉCONNEXION" },
  { LocId::Lobby_FirstTo, "Lobby_FirstTo", "Manches gagnantes" },
  { LocId::Lobby_StartLevel, "Lobby_StartLevel", "Niveau de départ" },

  // In-Game HUD
  { LocId::Hud_Score, "Hud_Score", "SCORE" },
  { LocId::Hud_Level, "Hud_Level", "NIVEAU" },
  { LocId::Hud_Lines, "Hud_Lines", "LIGNES" },
  { LocId::Hud_Time, "Hud_Time", "TEMPS" },
  { LocId::Hud_PPS, "Hud_PPS", "PPS" },
  { LocId::Hud_Hold, "Hud_Hold", "RÉSERVE" },
  { LocId::Hud_Next, "Hud_Next", "SUIVANT" },
  { LocId::Hud_Garbage, "Hud_Garbage", "PÉNALITÉS" },
  { LocId::Hud_Paused, "Hud_Paused", "EN PAUSE" },
  { LocId::Hud_GameOver, "Hud_GameOver", "PARTIE TERMINÉE" },
  { LocId::Hud_Victory, "Hud_Victory", "VICTOIRE !" },
  { LocId::Hud_Winner, "Hud_Winner", "GAGNANT !" },
  { LocId::Hud_Combo, "Hud_Combo", "Combo" },
  { LocId::Hud_BackToBack, "Hud_BackToBack", "Enchaînement" },
  { LocId::Hud_PerfectClear, "Hud_PerfectClear", "SANS FAUTE !" },
  { LocId::Hud_TSpin, "Hud_TSpin", "T-SPIN" },
  { LocId::Hud_Tetris, "Hud_Tetris", "TÉTRIS !" },

  // In-Game Modals / Pause Menu
  { LocId::Modal_PausedTitle, "Modal_PausedTitle", "JEU EN PAUSE" },
  { LocId::Modal_Resume, "Modal_Resume", "REPRENDRE LA PARTIE" },
  { LocId::Modal_ResumeDesc, "Modal_ResumeDesc", "Poursuivre la partie en cours" },
  { LocId::Modal_Restart, "Modal_Restart", "RECOMMENCER LA PARTIE" },
  { LocId::Modal_RestartDesc, "Modal_RestartDesc", "Recommencer le mode actuel depuis le niveau 1" },
  { LocId::Modal_Options, "Modal_Options", "OPTIONS" },
  { LocId::Modal_OptionsDesc, "Modal_OptionsDesc", "Ajuster la jouabilité, le son et les graphismes" },
  { LocId::Modal_MainMenu, "Modal_MainMenu", "RETOUR AU MENU" },
  { LocId::Modal_MainMenuDesc, "Modal_MainMenuDesc", "Quitter la partie et retourner au menu principal" },
  { LocId::Modal_Exit, "Modal_Exit", "QUITTER LE JEU" },
  { LocId::Modal_ExitDesc, "Modal_ExitDesc", "Fermer l'application" },

  // High Scores
  { LocId::HighScores_Title, "HighScores_Title", "TABLEAU DES MEILLEURS SCORES" },
  { LocId::HighScores_NoRecords, "HighScores_NoRecords", "Aucun score enregistré. Terminez une partie pour inscrire un record !" },

  // Options - General / Gameplay
  { LocId::Option_Title, "Option_Title", "CONFIGURATION & PARAMÈTRES" },
  { LocId::Option_Language, "Option_Language", "Langue" },
  { LocId::Option_HandlingHeader, "Option_HandlingHeader", "Paramètres de maniabilité" },
  { LocId::Option_DAS, "Option_DAS", "DAS (Délai de répétition)" },
  { LocId::Option_ARR, "Option_ARR", "ARR (Vitesse de répétition)" },
  { LocId::Option_SDF, "Option_SDF", "SDF (Facteur de chute douce)" },
  { LocId::Option_LockDelay, "Option_LockDelay", "Délai de verrouillage" },
  { LocId::Option_PresetGuideline, "Option_PresetGuideline", "Standard officiel" },
  { LocId::Option_PresetCompetitive, "Option_PresetCompetitive", "Compétition rapide" },
  { LocId::Option_AssistsHeader, "Option_AssistsHeader", "Aides visuelles et assistance" },
  { LocId::Option_ShowGhost, "Option_ShowGhost", "Afficher l'ombre de la pièce" },
  { LocId::Option_StartLevel, "Option_StartLevel", "Niveau de départ par défaut" },

  // Options - Visuals & Materials
  { LocId::Option_VisualsHeader, "Option_VisualsHeader", "Visuels & Présentation" },
  { LocId::Option_MaterialPreset, "Option_MaterialPreset", "Style des pièces (matériau)" },
  { LocId::Option_MatGlossy, "Option_MatGlossy", "Plastique brillant" },
  { LocId::Option_MatGlass, "Option_MatGlass", "Verre dépoli" },
  { LocId::Option_MatNeon, "Option_MatNeon", "Néon lumineux" },
  { LocId::Option_MatMetal, "Option_MatMetal", "Métal brossé" },
  { LocId::Option_CameraPreset, "Option_CameraPreset", "Perspective caméra" },
  { LocId::Option_CamFrontal, "Option_CamFrontal", "Frontale classique" },
  { LocId::Option_CamIsometric, "Option_CamIsometric", "Isométrique 3D" },
  { LocId::Option_CamOrbit, "Option_CamOrbit", "Dynamic Orbit" },
  { LocId::Option_ClearAnim, "Option_ClearAnim", "Durée d'animation d'élimination" },
  { LocId::Option_BackplateOpacity, "Option_BackplateOpacity", "Opacité du fond de grille" },
  { LocId::Option_BoardEdgeColor, "Option_BoardEdgeColor", "Couleur des bordures du plateau" },
  { LocId::Option_EdgePresets, "Option_EdgePresets", "Styles de bordure prédéfinis" },
  { LocId::Option_DynamicLighting, "Option_DynamicLighting", "Éclairage néon dynamique" },
  { LocId::Option_CameraMotion, "Option_CameraMotion", "Mouvement de caméra" },
  { LocId::Option_MotionSpeed, "Option_MotionSpeed", "Vitesse d'oscillation" },
  { LocId::Option_BoardSway, "Option_BoardSway", "Amplitude d'oscillation" },
  { LocId::Option_HdrBackground, "Option_HdrBackground", "Environnement HDR / Ciel" },
  { LocId::Option_NextBackground, "Option_NextBackground", "Changer d'environnement" },
  { LocId::Option_RotateBgLevel, "Option_RotateBgLevel", "Changer de décor par niveau" },
  { LocId::Option_SkyboxRotation, "Option_SkyboxRotation", "Rotation continue du ciel" },

  // Options - Engine & Renderer
  { LocId::Option_EngineHeader, "Option_EngineHeader", "Moteur de rendu & Affichage" },
  { LocId::Option_ActiveRenderer, "Option_ActiveRenderer", "Moteur de rendu actif" },
  { LocId::Option_RenderResolution, "Option_RenderResolution", "Résolution de rendu" },
  { LocId::Option_RenderScale, "Option_RenderScale", "Échelle de rendu (%)" },
  { LocId::Option_Fullscreen, "Option_Fullscreen", "Mode plein écran" },
  { LocId::Option_VSync, "Option_VSync", "Synchronisation verticale (VSync)" },
  { LocId::Option_ToneMapping, "Option_ToneMapping", "Opérateur de tone mapping" },
  { LocId::Option_FXAA, "Option_FXAA", "Anti-aliasing rapide (FXAA)" },
  { LocId::Option_Skybox, "Option_Skybox", "Afficher le ciel / arrière-plan" },
  { LocId::Option_Exposure, "Option_Exposure", "Exposition" },
  { LocId::Option_Gamma, "Option_Gamma", "Correction gamma" },
  { LocId::Option_DeferredFeatures, "Option_DeferredFeatures", "Fonctionnalités Deferred OpenGL" },
  { LocId::Option_ShadowMapping, "Option_ShadowMapping", "Ombres portées directionnelles" },
  { LocId::Option_SSAO, "Option_SSAO", "Occlusion ambiante (SSAO)" },
  { LocId::Option_SSR, "Option_SSR", "Réflexions d'écran (SSR)" },
  { LocId::Option_Transparency, "Option_Transparency", "Transparence indépendante de l'ordre" },
  { LocId::Option_Refraction, "Option_Refraction", "Réfraction de surface" },
  { LocId::Option_PathTracerFeatures, "Option_PathTracerFeatures", "Paramètres Path Tracer GPU" },
  { LocId::Option_MaxBounces, "Option_MaxBounces", "Nombre max de rebonds" },
  { LocId::Option_SamplesPerPixel, "Option_SamplesPerPixel", "Échantillons par pixel (SPP)" },
  { LocId::Option_Accumulation, "Option_Accumulation", "Accumulation temporelle" },
  { LocId::Option_Denoiser, "Option_Denoiser", "Débruitage spatial en ondelettes" },
  { LocId::Option_SoftwareFeatures, "Option_SoftwareFeatures", "Rasteriseur logiciel CPU" },
  { LocId::Option_TiledRendering, "Option_TiledRendering", "Rendu par blocs multithreadé" },

  // Options - Audio
  { LocId::Option_AudioHeader, "Option_AudioHeader", "Paramètres sonores & Volume" },
  { LocId::Option_MuteAll, "Option_MuteAll", "Couper tout le son" },
  { LocId::Option_MasterVolume, "Option_MasterVolume", "Volume principal" },
  { LocId::Option_SfxVolume, "Option_SfxVolume", "Effets sonores (SFX)" },
  { LocId::Option_MusicVolume, "Option_MusicVolume", "Volume musique" },
  { LocId::Option_TestSfx, "Option_TestSfx", "Tester le son" },

  // Options - Controls
  { LocId::Option_ControlsHeader, "Option_ControlsHeader", "Attribution des touches & Manette" },
  { LocId::Option_ResetControls, "Option_ResetControls", "Rétablir les touches par défaut" }
};

const LocStringEntry * GetFrenchEntries( size_t & oCount )
{
  oCount = sizeof(s_FrenchEntries) / sizeof(s_FrenchEntries[0]);
  return s_FrenchEntries;
}

} // namespace RTRT
