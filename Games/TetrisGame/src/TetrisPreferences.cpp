#include "TetrisPreferences.h"
#include "PathUtils.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace Tetris
{

// ----------------------------------------------------------------------------
// ApplyToTetris
// ----------------------------------------------------------------------------
void TetrisPreferences::ApplyToTetris( TetrisSceneManager & ioSceneMgr,
                                       Scene & ioScene,
                                       TetrisHandlingConfig & ioHandling,
                                       TetrisGameLogic & ioLogic,
                                       TetrisRendererMode & ioRendererMode ) const
{
  ioHandling = _Handling;
  ioSceneMgr.SetMaterialPreset( ioScene, _MaterialPreset );
  ioSceneMgr.SetCameraPreset( _CameraPreset );
  ioSceneMgr.SetLineClearAnimMode( _LineClearAnim );
  ioSceneMgr.SetBackplateOpacity( ioScene, _BackplateOpacity );
  ioSceneMgr.SetBoardEdgeColor( ioScene, _BoardEdgeColor );
  ioSceneMgr.SetDynamicPieceLighting( _DynamicLighting );
  ioSceneMgr.SetInGameCameraMotion( _CameraMotion );
  ioSceneMgr.SetBackgroundMotionSpeed( _CameraMotionSpeed );
  ioSceneMgr.SetBoardSwayAmplitude( _BoardSwayAmplitude );
  ioSceneMgr.SetShowGhost( _ShowGhost );
  ioLogic.SetStartLevel( _StartLevel );
  ioRendererMode = _RendererMode;
}

// ----------------------------------------------------------------------------
// CaptureFromTetris
// ----------------------------------------------------------------------------
void TetrisPreferences::CaptureFromTetris( const TetrisSceneManager & iSceneMgr,
                                           const TetrisHandlingConfig & iHandling,
                                           const TetrisGameLogic & iLogic,
                                           TetrisRendererMode iRendererMode )
{
  _Handling           = iHandling;
  _MaterialPreset     = iSceneMgr.GetMaterialPreset();
  _CameraPreset       = iSceneMgr.GetCameraPreset();
  _LineClearAnim      = iSceneMgr.GetLineClearAnimMode();
  _BackplateOpacity   = iSceneMgr.GetBackplateOpacity();
  _BoardEdgeColor     = iSceneMgr.GetBoardEdgeColor();
  _DynamicLighting    = iSceneMgr.GetDynamicPieceLighting();
  _CameraMotion       = iSceneMgr.GetInGameCameraMotion();
  _CameraMotionSpeed  = iSceneMgr.GetBackgroundMotionSpeed();
  _BoardSwayAmplitude = iSceneMgr.GetBoardSwayAmplitude();
  _ShowGhost          = iSceneMgr.GetShowGhost();
  _StartLevel         = iLogic.GetStartLevel();
  _RendererMode       = iRendererMode;
}

// ----------------------------------------------------------------------------
// LoadFromFile
// ----------------------------------------------------------------------------
bool TetrisPreferences::LoadFromFile( const std::string & iFilePath )
{
  // First load base preferences (Language, Audio, Display)
  const bool baseLoaded = UserPreferences::LoadFromFile( iFilePath );

  std::ifstream file( iFilePath );
  if ( !file.is_open() )
  {
    return baseLoaded;
  }

  try
  {
    nlohmann::json root = nlohmann::json::parse( file );

    if ( root.contains( "handling" ) && root["handling"].is_object() )
    {
      const auto & h = root["handling"];
      if ( h.contains( "das" ) && h["das"].is_number() )
        _Handling._DAS = h["das"].get<float>();
      if ( h.contains( "arr" ) && h["arr"].is_number() )
        _Handling._ARR = h["arr"].get<float>();
      if ( h.contains( "sdf" ) && h["sdf"].is_number() )
        _Handling._SoftDropFactor = h["sdf"].get<float>();
      if ( h.contains( "lock_delay" ) && h["lock_delay"].is_number() )
        _Handling._LockDelay = h["lock_delay"].get<float>();
    }

    if ( root.contains( "tetris" ) && root["tetris"].is_object() )
    {
      const auto & t = root["tetris"];
      if ( t.contains( "renderer_mode" ) && t["renderer_mode"].is_number_integer() )
      {
        int r = t["renderer_mode"].get<int>();
        if ( r >= 0 && r <= 2 )
          _RendererMode = static_cast<TetrisRendererMode>(r);
      }
      if ( t.contains( "material_preset" ) && t["material_preset"].is_number_integer() )
        _MaterialPreset = static_cast<TetrisMaterialPreset>(t["material_preset"].get<int>());
      if ( t.contains( "camera_preset" ) && t["camera_preset"].is_number_integer() )
        _CameraPreset = static_cast<TetrisCameraPreset>(t["camera_preset"].get<int>());
      if ( t.contains( "line_clear_anim" ) && t["line_clear_anim"].is_number_integer() )
        _LineClearAnim = static_cast<TetrisLineClearAnimMode>(t["line_clear_anim"].get<int>());
      if ( t.contains( "backplate_opacity" ) && t["backplate_opacity"].is_number() )
        _BackplateOpacity = t["backplate_opacity"].get<float>();
      if ( t.contains( "board_edge_color" ) && t["board_edge_color"].is_array() && t["board_edge_color"].size() >= 3 )
      {
        _BoardEdgeColor.x = t["board_edge_color"][0].get<float>();
        _BoardEdgeColor.y = t["board_edge_color"][1].get<float>();
        _BoardEdgeColor.z = t["board_edge_color"][2].get<float>();
      }
      if ( t.contains( "dynamic_lighting" ) && t["dynamic_lighting"].is_boolean() )
        _DynamicLighting = t["dynamic_lighting"].get<bool>();
      if ( t.contains( "camera_motion" ) && t["camera_motion"].is_boolean() )
        _CameraMotion = t["camera_motion"].get<bool>();
      if ( t.contains( "camera_motion_speed" ) && t["camera_motion_speed"].is_number() )
        _CameraMotionSpeed = t["camera_motion_speed"].get<float>();
      if ( t.contains( "board_sway_amplitude" ) && t["board_sway_amplitude"].is_number() )
        _BoardSwayAmplitude = t["board_sway_amplitude"].get<float>();
      if ( t.contains( "start_level" ) && t["start_level"].is_number_integer() )
        _StartLevel = t["start_level"].get<int>();
      if ( t.contains( "show_ghost" ) && t["show_ghost"].is_boolean() )
        _ShowGhost = t["show_ghost"].get<bool>();
    }

    return true;
  }
  catch ( const std::exception & e )
  {
    std::cerr << "[TetrisPreferences] Failed to parse extra config: " << e.what() << std::endl;
    return false;
  }
}

// ----------------------------------------------------------------------------
// SaveToFile
// ----------------------------------------------------------------------------
bool TetrisPreferences::SaveToFile( const std::string & iFilePath ) const
{
  try
  {
    nlohmann::json root;
    root["version"]  = 1;
    root["language"] = ( Language::French == _Language ) ? "fr" : "en";

    nlohmann::json audio;
    audio["muted"]         = _Audio._Muted;
    audio["master_volume"] = _Audio._MasterVolume;
    audio["sfx_volume"]    = _Audio._SfxVolume;
    audio["music_volume"]  = _Audio._MusicVolume;
    root["audio"]          = audio;

    nlohmann::json graphics;
    graphics["custom_render_width"]  = _CustomRenderResolution.x;
    graphics["custom_render_height"] = _CustomRenderResolution.y;
    graphics["render_scale"]         = _RenderScale;
    graphics["fullscreen"]           = _FullScreen;
    graphics["vsync"]                = _VSync;
    graphics["tone_mapping"]         = _ToneMapping;
    graphics["fxaa"]                 = _FXAA;
    graphics["exposure"]             = _Exposure;
    graphics["gamma"]                = _Gamma;
    graphics["enable_skybox"]        = _EnableSkybox;
    root["graphics"]                 = graphics;

    nlohmann::json handling;
    handling["das"]        = _Handling._DAS;
    handling["arr"]        = _Handling._ARR;
    handling["sdf"]        = _Handling._SoftDropFactor;
    handling["lock_delay"] = _Handling._LockDelay;
    root["handling"]       = handling;

    nlohmann::json tetris;
    tetris["renderer_mode"]        = static_cast<int>(_RendererMode);
    tetris["material_preset"]      = static_cast<int>(_MaterialPreset);
    tetris["camera_preset"]        = static_cast<int>(_CameraPreset);
    tetris["line_clear_anim"]      = static_cast<int>(_LineClearAnim);
    tetris["backplate_opacity"]    = _BackplateOpacity;
    tetris["board_edge_color"]     = { _BoardEdgeColor.x, _BoardEdgeColor.y, _BoardEdgeColor.z };
    tetris["dynamic_lighting"]     = _DynamicLighting;
    tetris["camera_motion"]        = _CameraMotion;
    tetris["camera_motion_speed"]  = _CameraMotionSpeed;
    tetris["board_sway_amplitude"] = _BoardSwayAmplitude;
    tetris["start_level"]          = _StartLevel;
    tetris["show_ghost"]           = _ShowGhost;
    root["tetris"]                 = tetris;

    std::ofstream file( iFilePath );
    if ( !file.is_open() )
    {
      return false;
    }

    file << root.dump( 2 ) << std::endl;
    return true;
  }
  catch ( const std::exception & e )
  {
    std::cerr << "[TetrisPreferences] Failed to save config: " << e.what() << std::endl;
    return false;
  }
}

// ----------------------------------------------------------------------------
// GetDefaultFilePath
// ----------------------------------------------------------------------------
std::string TetrisPreferences::GetDefaultFilePath()
{
  return PathUtils::GetAssetPath( "tetris_preferences.json" );
}

} // namespace Tetris
