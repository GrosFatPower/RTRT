#include "UserPreferences.h"
#include "PathUtils.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace RTRT
{

// ----------------------------------------------------------------------------
// ApplyTo
// ----------------------------------------------------------------------------
void UserPreferences::ApplyTo( RenderSettings & ioSettings, AudioEngine * ioAudio ) const
{
  Loc::SetLanguage( _Language );

  ioSettings._CustomRenderResolution = _CustomRenderResolution;
  ioSettings._RenderScale            = _RenderScale;
  ioSettings._FullScreen             = _FullScreen;
  ioSettings._VSync                  = _VSync;
  ioSettings._ToneMapping            = _ToneMapping;
  ioSettings._FXAA                   = _FXAA;
  ioSettings._Exposure               = _Exposure;
  ioSettings._Gamma                  = _Gamma;
  ioSettings._EnableSkybox           = _EnableSkybox;

  if ( ioAudio )
  {
    ioAudio->SetMuted( _Audio._Muted );
    ioAudio->SetMasterVolume( _Audio._MasterVolume );
    ioAudio->SetSfxVolume( _Audio._SfxVolume );
    ioAudio->SetMusicVolume( _Audio._MusicVolume );
  }
}

// ----------------------------------------------------------------------------
// CaptureFrom
// ----------------------------------------------------------------------------
void UserPreferences::CaptureFrom( const RenderSettings & iSettings, const AudioEngine * iAudio )
{
  _Language               = Loc::GetLanguage();
  _CustomRenderResolution = iSettings._CustomRenderResolution;
  _RenderScale            = iSettings._RenderScale;
  _FullScreen             = iSettings._FullScreen;
  _VSync                  = iSettings._VSync;
  _ToneMapping            = iSettings._ToneMapping;
  _FXAA                   = iSettings._FXAA;
  _Exposure               = iSettings._Exposure;
  _Gamma                  = iSettings._Gamma;
  _EnableSkybox           = iSettings._EnableSkybox;

  if ( iAudio )
  {
    _Audio._Muted        = iAudio->IsMuted();
    _Audio._MasterVolume = iAudio->GetMasterVolume();
    _Audio._SfxVolume    = iAudio->GetSfxVolume();
    _Audio._MusicVolume  = iAudio->GetMusicVolume();
  }
}

// ----------------------------------------------------------------------------
// LoadFromFile
// ----------------------------------------------------------------------------
bool UserPreferences::LoadFromFile( const std::string & iFilePath )
{
  std::ifstream file( iFilePath );
  if ( !file.is_open() )
  {
    return false;
  }

  try
  {
    nlohmann::json root = nlohmann::json::parse( file );

    if ( root.contains( "language" ) )
    {
      if ( root["language"].is_string() )
      {
        const std::string langStr = root["language"].get<std::string>();
        if ( langStr == "fr" || langStr == "French" || langStr == "Français" )
          _Language = Language::French;
        else
          _Language = Language::English;
      }
      else if ( root["language"].is_number_integer() )
      {
        int langInt = root["language"].get<int>();
        if ( langInt >= 0 && langInt < static_cast<int>(Language::Count) )
          _Language = static_cast<Language>(langInt);
      }
    }

    if ( root.contains( "audio" ) && root["audio"].is_object() )
    {
      const auto & a = root["audio"];
      if ( a.contains( "muted" ) && a["muted"].is_boolean() )
        _Audio._Muted = a["muted"].get<bool>();
      if ( a.contains( "master_volume" ) && a["master_volume"].is_number() )
        _Audio._MasterVolume = a["master_volume"].get<float>();
      if ( a.contains( "sfx_volume" ) && a["sfx_volume"].is_number() )
        _Audio._SfxVolume = a["sfx_volume"].get<float>();
      if ( a.contains( "music_volume" ) && a["music_volume"].is_number() )
        _Audio._MusicVolume = a["music_volume"].get<float>();
    }

    if ( root.contains( "graphics" ) && root["graphics"].is_object() )
    {
      const auto & g = root["graphics"];
      if ( g.contains( "custom_render_width" ) && g.contains( "custom_render_height" ) )
      {
        _CustomRenderResolution.x = g["custom_render_width"].get<int>();
        _CustomRenderResolution.y = g["custom_render_height"].get<int>();
      }
      if ( g.contains( "render_scale" ) && g["render_scale"].is_number_integer() )
        _RenderScale = g["render_scale"].get<int>();
      if ( g.contains( "fullscreen" ) && g["fullscreen"].is_boolean() )
        _FullScreen = g["fullscreen"].get<bool>();
      if ( g.contains( "vsync" ) && g["vsync"].is_boolean() )
        _VSync = g["vsync"].get<bool>();
      if ( g.contains( "tone_mapping" ) && g["tone_mapping"].is_boolean() )
        _ToneMapping = g["tone_mapping"].get<bool>();
      if ( g.contains( "fxaa" ) && g["fxaa"].is_boolean() )
        _FXAA = g["fxaa"].get<bool>();
      if ( g.contains( "exposure" ) && g["exposure"].is_number() )
        _Exposure = g["exposure"].get<float>();
      if ( g.contains( "gamma" ) && g["gamma"].is_number() )
        _Gamma = g["gamma"].get<float>();
      if ( g.contains( "enable_skybox" ) && g["enable_skybox"].is_boolean() )
        _EnableSkybox = g["enable_skybox"].get<bool>();
    }

    return true;
  }
  catch ( const std::exception & e )
  {
    std::cerr << "[UserPreferences] Failed to parse config file: " << e.what() << std::endl;
    return false;
  }
}

// ----------------------------------------------------------------------------
// SaveToFile
// ----------------------------------------------------------------------------
bool UserPreferences::SaveToFile( const std::string & iFilePath ) const
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
    std::cerr << "[UserPreferences] Failed to save config file: " << e.what() << std::endl;
    return false;
  }
}

// ----------------------------------------------------------------------------
// GetDefaultFilePath
// ----------------------------------------------------------------------------
std::string UserPreferences::GetDefaultFilePath( const std::string & iFilename )
{
  return PathUtils::GetAssetPath( iFilename );
}

} // namespace RTRT
