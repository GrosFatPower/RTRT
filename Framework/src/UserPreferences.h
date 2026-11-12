#ifndef _UserPreferences_
#define _UserPreferences_

#include "AudioEngine.h"
#include "Localization.h"
#include "MathUtil.h"
#include "RenderSettings.h"

#include <string>

namespace RTRT
{

// ----------------------------------------------------------------------------
// AudioPreferences
// ----------------------------------------------------------------------------
struct AudioPreferences
{
  bool  _Muted        = false;
  float _MasterVolume = 0.8f;
  float _SfxVolume    = 0.8f;
  float _MusicVolume  = 0.7f;
};

// ----------------------------------------------------------------------------
// UserPreferences
// ----------------------------------------------------------------------------
struct UserPreferences
{
  Language         _Language = Language::English;
  AudioPreferences _Audio;

  // Window / Display options
  Vec2i            _CustomRenderResolution = { 0, 0 };
  int              _RenderScale            = 100;
  bool             _FullScreen             = false;
  bool             _VSync                  = true;
  bool             _ToneMapping            = true;
  bool             _FXAA                   = false;
  float            _Exposure               = 1.5f;
  float            _Gamma                  = 2.0f;
  bool             _EnableSkybox           = true;

  // Sync to/from runtime systems
  void ApplyTo( RenderSettings & ioSettings, AudioEngine * ioAudio = nullptr ) const;
  void CaptureFrom( const RenderSettings & iSettings, const AudioEngine * iAudio = nullptr );

  // Serialization via nlohmann::json
  bool LoadFromFile( const std::string & iFilePath );
  bool SaveToFile( const std::string & iFilePath ) const;

  // Resolves default user config path: e.g. <RepoRoot>/user_preferences.json
  static std::string GetDefaultFilePath( const std::string & iFilename = "user_preferences.json" );
};

} // namespace RTRT

#endif /* _UserPreferences_ */
