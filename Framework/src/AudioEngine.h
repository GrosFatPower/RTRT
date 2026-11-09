#ifndef _AudioEngine_
#define _AudioEngine_

#include "MathUtil.h"

#include <memory>
#include <string>
#include <vector>

#ifdef PlaySound
#undef PlaySound
#endif

namespace RTRT
{

// ----------------------------------------------------------------------------
// AudioBus
// ----------------------------------------------------------------------------
enum class AudioBus
{
  Master,
  Sfx,
  Music
};

// ----------------------------------------------------------------------------
// WaveformType
// ----------------------------------------------------------------------------
enum class WaveformType
{
  Sine,
  Square,
  Triangle,
  Sawtooth,
  Noise
};

// ----------------------------------------------------------------------------
// ProceduralSoundDesc
// ----------------------------------------------------------------------------
struct ProceduralSoundDesc
{
  WaveformType _Waveform          = WaveformType::Sine;
  float        _StartFrequency    = 440.0f; // Hz
  float        _EndFrequency      = 440.0f; // Hz (pitch sweep)
  float        _FrequencyExp      = 1.0f;   // 1.0 = linear, >1 = exponential

  float        _Duration          = 0.15f;  // Seconds
  float        _AttackTime        = 0.005f; // Seconds
  float        _DecayTime         = 0.04f;  // Seconds
  float        _SustainLevel      = 0.7f;   // 0.0 - 1.0 amplitude
  float        _ReleaseTime       = 0.1f;   // Seconds

  float        _Harmonic2Weight   = 0.0f;   // 2nd harmonic (octave)
  float        _Harmonic3Weight   = 0.0f;   // 3rd harmonic (perfect fifth)
  float        _DutyCycle         = 0.5f;   // Square wave pulse width

  float        _ArpInterval2      = 0.0f;   // Semitone interval 2 (+4 = major 3rd, +7 = fifth, etc.)
  float        _ArpInterval3      = 0.0f;   // Semitone interval 3
  float        _ArpInterval4      = 0.0f;   // Semitone interval 4
  float        _ArpSpeed          = 0.0f;   // Seconds per arpeggio step (0 = simultaneous chord)

  float        _NoiseMix          = 0.0f;   // 0.0 = pure tone, 1.0 = pure noise
  float        _Volume            = 0.8f;   // Output amplitude
};

// ----------------------------------------------------------------------------
// AudioEngine
// ----------------------------------------------------------------------------
class AudioEngine
{
public:
  AudioEngine();
  virtual ~AudioEngine();

  bool Initialize();
  void Shutdown();
  bool IsInitialized() const;

  void SetMasterVolume( float iVolume );
  float GetMasterVolume() const;

  void SetSfxVolume( float iVolume );
  float GetSfxVolume() const;

  void SetMusicVolume( float iVolume );
  float GetMusicVolume() const;

  void SetMuted( bool iMuted );
  bool IsMuted() const;

  bool RegisterSoundFile( const std::string & iSoundId,
                          const std::string & iFilePath,
                          AudioBus iBus = AudioBus::Sfx );

  bool RegisterProceduralSound( const std::string & iSoundId,
                                const ProceduralSoundDesc & iDesc,
                                AudioBus iBus = AudioBus::Sfx );

  bool HasSound( const std::string & iSoundId ) const;

  void PlaySound2D( const std::string & iSoundId,
                    float iVolume = 1.0f,
                    float iPitch = 1.0f );

  void UpdateListener( const Vec3 & iPosition,
                       const Vec3 & iForward,
                       const Vec3 & iUp,
                       const Vec3 & iVelocity = Vec3( 0.0f ) );

  void PlaySound3D( const std::string & iSoundId,
                    const Vec3 & iWorldPos,
                    float iVolume = 1.0f,
                    float iPitch = 1.0f,
                    float iMinDistance = 1.5f,
                    float iMaxDistance = 40.0f );

  void StopAllSounds();

private:
  struct Impl;
  std::unique_ptr<Impl> _Impl;
};

} // namespace RTRT

#endif /* _AudioEngine_ */
