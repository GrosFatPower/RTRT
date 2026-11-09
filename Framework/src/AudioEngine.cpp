#include "AudioEngine.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Internal Sound Data & Voice Structures
// ----------------------------------------------------------------------------
namespace
{
constexpr float PI_F = 3.14159265358979323846f;
constexpr size_t MAX_VOICES = 32;

struct SoundResource
{
  std::vector<float> _PcmFrames;
  ma_uint32          _Channels   = 1;
  ma_uint32          _SampleRate = 44100;
  AudioBus           _Bus        = AudioBus::Sfx;
};

struct AudioVoice
{
  ma_sound        _Sound;
  ma_audio_buffer _Buffer;
  bool            _InUse      = false;
  uint64_t        _StartFrame = 0;
};
} // namespace

// ----------------------------------------------------------------------------
// AudioEngine::Impl
// ----------------------------------------------------------------------------
struct AudioEngine::Impl
{
  ma_engine                           _Engine;
  ma_sound_group                      _SfxGroup;
  ma_sound_group                      _MusicGroup;
  bool                                _Initialized = false;

  float                               _MasterVolume = 1.0f;
  float                               _SfxVolume    = 0.8f;
  float                               _MusicVolume  = 0.7f;
  bool                                _Muted        = false;

  std::unordered_map<std::string, SoundResource> _SoundBank;
  AudioVoice                          _Voices[MAX_VOICES];
  uint64_t                            _FrameCounter = 0;

  ~Impl()
  {
    Shutdown();
  }

  void Shutdown()
  {
    if( !_Initialized )
    {
      return;
    }

    for( size_t i = 0; i < MAX_VOICES; ++i )
    {
      if( _Voices[i]._InUse )
      {
        ma_sound_uninit( &_Voices[i]._Sound );
        ma_audio_buffer_uninit( &_Voices[i]._Buffer );
        _Voices[i]._InUse = false;
      }
    }

    ma_sound_group_uninit( &_SfxGroup );
    ma_sound_group_uninit( &_MusicGroup );
    ma_engine_uninit( &_Engine );

    _SoundBank.clear();
    _Initialized = false;
  }

  AudioVoice * AcquireVoice()
  {
    _FrameCounter++;

    // 1. Try finding a voice that has naturally finished or is unused
    for( size_t i = 0; i < MAX_VOICES; ++i )
    {
      if( !_Voices[i]._InUse )
      {
        return &_Voices[i];
      }
      if( ma_sound_at_end( &_Voices[i]._Sound ) )
      {
        ma_sound_uninit( &_Voices[i]._Sound );
        ma_audio_buffer_uninit( &_Voices[i]._Buffer );
        _Voices[i]._InUse = false;
        return &_Voices[i];
      }
    }

    // 2. All voices busy: steal oldest active voice
    size_t oldestIdx = 0;
    uint64_t oldestFrame = _Voices[0]._StartFrame;
    for( size_t i = 1; i < MAX_VOICES; ++i )
    {
      if( _Voices[i]._StartFrame < oldestFrame )
      {
        oldestFrame = _Voices[i]._StartFrame;
        oldestIdx = i;
      }
    }

    AudioVoice * victim = &_Voices[oldestIdx];
    ma_sound_uninit( &victim->_Sound );
    ma_audio_buffer_uninit( &victim->_Buffer );
    victim->_InUse = false;
    return victim;
  }

  void ApplyMasterVolume()
  {
    if( !_Initialized )
    {
      return;
    }
    float vol = _Muted ? 0.0f : _MasterVolume;
    ma_engine_set_volume( &_Engine, vol );
  }

  void ApplySfxVolume()
  {
    if( !_Initialized )
    {
      return;
    }
    ma_sound_group_set_volume( &_SfxGroup, _SfxVolume );
  }

  void ApplyMusicVolume()
  {
    if( !_Initialized )
    {
      return;
    }
    ma_sound_group_set_volume( &_MusicGroup, _MusicVolume );
  }
};

// ----------------------------------------------------------------------------
// AudioEngine
// ----------------------------------------------------------------------------
AudioEngine::AudioEngine()
  : _Impl( std::make_unique<Impl>() )
{
}

AudioEngine::~AudioEngine() = default;

bool AudioEngine::Initialize()
{
  if( _Impl->_Initialized )
  {
    return true;
  }

  ma_engine_config engineConfig = ma_engine_config_init();
  engineConfig.listenerCount = 1;

  ma_result res = ma_engine_init( &engineConfig, &_Impl->_Engine );
  if( res != MA_SUCCESS )
  {
    std::cerr << "[AudioEngine] Warning: Failed to initialize audio device (code " << res
              << "). Sound will run in silent fallback mode." << std::endl;
    _Impl->_Initialized = false;
    return false;
  }

  ma_sound_group_init( &_Impl->_Engine, 0, nullptr, &_Impl->_SfxGroup );
  ma_sound_group_init( &_Impl->_Engine, 0, nullptr, &_Impl->_MusicGroup );

  for( size_t i = 0; i < MAX_VOICES; ++i )
  {
    _Impl->_Voices[i]._InUse = false;
    _Impl->_Voices[i]._StartFrame = 0;
  }

  _Impl->_Initialized = true;
  _Impl->ApplyMasterVolume();
  _Impl->ApplySfxVolume();
  _Impl->ApplyMusicVolume();

  return true;
}

void AudioEngine::Shutdown()
{
  if( _Impl )
  {
    _Impl->Shutdown();
  }
}

bool AudioEngine::IsInitialized() const
{
  return _Impl && _Impl->_Initialized;
}

void AudioEngine::SetMasterVolume( float iVolume )
{
  if( !_Impl )
  {
    return;
  }
  _Impl->_MasterVolume = std::clamp( iVolume, 0.0f, 1.0f );
  _Impl->ApplyMasterVolume();
}

float AudioEngine::GetMasterVolume() const
{
  return _Impl ? _Impl->_MasterVolume : 1.0f;
}

void AudioEngine::SetSfxVolume( float iVolume )
{
  if( !_Impl )
  {
    return;
  }
  _Impl->_SfxVolume = std::clamp( iVolume, 0.0f, 1.0f );
  _Impl->ApplySfxVolume();
}

float AudioEngine::GetSfxVolume() const
{
  return _Impl ? _Impl->_SfxVolume : 0.8f;
}

void AudioEngine::SetMusicVolume( float iVolume )
{
  if( !_Impl )
  {
    return;
  }
  _Impl->_MusicVolume = std::clamp( iVolume, 0.0f, 1.0f );
  _Impl->ApplyMusicVolume();
}

float AudioEngine::GetMusicVolume() const
{
  return _Impl ? _Impl->_MusicVolume : 0.7f;
}

void AudioEngine::SetMuted( bool iMuted )
{
  if( !_Impl )
  {
    return;
  }
  _Impl->_Muted = iMuted;
  _Impl->ApplyMasterVolume();
}

bool AudioEngine::IsMuted() const
{
  return _Impl ? _Impl->_Muted : false;
}

bool AudioEngine::RegisterSoundFile( const std::string & iSoundId,
                                     const std::string & iFilePath,
                                     AudioBus iBus )
{
  if( !_Impl )
  {
    return false;
  }

  ma_decoder_config decoderConfig = ma_decoder_config_init( ma_format_f32, 2, 44100 );
  ma_uint64 totalFrameCount = 0;
  void * pFrames = nullptr;

  ma_result res = ma_decode_file( iFilePath.c_str(), &decoderConfig, &totalFrameCount, &pFrames );
  if( res != MA_SUCCESS || !pFrames || totalFrameCount == 0 )
  {
    return false;
  }

  SoundResource resource;
  resource._Channels = 2;
  resource._SampleRate = 44100;
  resource._Bus = iBus;
  const float * floatData = static_cast<const float *>( pFrames );
  resource._PcmFrames.assign( floatData, floatData + ( totalFrameCount * 2 ) );

  ma_free( pFrames, nullptr );

  _Impl->_SoundBank[iSoundId] = std::move( resource );
  return true;
}

namespace
{
float EvaluateOscillator( WaveformType iType, float iPhase, float iDutyCycle )
{
  float p = std::fmod( iPhase, 2.0f * PI_F );
  if( p < 0.0f )
  {
    p += 2.0f * PI_F;
  }

  switch( iType )
  {
  case WaveformType::Sine:
    return std::sin( p );

  case WaveformType::Square:
    return ( p < ( 2.0f * PI_F * std::clamp( iDutyCycle, 0.05f, 0.95f ) ) ) ? 1.0f : -1.0f;

  case WaveformType::Triangle:
  {
    float norm = p / ( 2.0f * PI_F );
    return 2.0f * std::fabs( 2.0f * ( norm - std::floor( norm + 0.5f ) ) ) - 1.0f;
  }

  case WaveformType::Sawtooth:
  {
    float norm = p / ( 2.0f * PI_F );
    return 2.0f * ( norm - std::floor( norm + 0.5f ) );
  }

  case WaveformType::Noise:
  default:
    return ( ( (float)std::rand() / (float)RAND_MAX ) * 2.0f ) - 1.0f;
  }
}
} // namespace

bool AudioEngine::RegisterProceduralSound( const std::string & iSoundId,
                                          const ProceduralSoundDesc & iDesc,
                                          AudioBus iBus )
{
  if( !_Impl )
  {
    return false;
  }

  const uint32_t sampleRate = 44100;
  float duration = std::max( 0.01f, iDesc._Duration );
  size_t totalFrames = static_cast<size_t>( duration * sampleRate );

  SoundResource resource;
  resource._Channels = 1;
  resource._SampleRate = sampleRate;
  resource._Bus = iBus;
  resource._PcmFrames.resize( totalFrames );

  float aTime = std::max( 0.001f, iDesc._AttackTime );
  float dTime = std::max( 0.001f, iDesc._DecayTime );
  float rTime = std::max( 0.001f, iDesc._ReleaseTime );
  float sLevel = std::clamp( iDesc._SustainLevel, 0.0f, 1.0f );

  // Normalize envelope stages if longer than total duration
  float totalEnv = aTime + dTime + rTime;
  if( totalEnv > duration )
  {
    float scale = ( duration * 0.95f ) / totalEnv;
    aTime *= scale;
    dTime *= scale;
    rTime *= scale;
  }
  float sTime = std::max( 0.0f, duration - aTime - dTime - rTime );

  float phase = 0.0f;
  float phaseHarm2 = 0.0f;
  float phaseHarm3 = 0.0f;

  for( size_t i = 0; i < totalFrames; ++i )
  {
    float t = static_cast<float>( i ) / static_cast<float>( sampleRate );

    // ADSR Envelope
    float env = 0.0f;
    if( t < aTime )
    {
      env = t / aTime;
    }
    else if( t < ( aTime + dTime ) )
    {
      float dt = t - aTime;
      env = 1.0f - ( 1.0f - sLevel ) * ( dt / dTime );
    }
    else if( t < ( aTime + dTime + sTime ) )
    {
      env = sLevel;
    }
    else
    {
      float rt = t - ( aTime + dTime + sTime );
      env = sLevel * std::max( 0.0f, 1.0f - ( rt / rTime ) );
    }

    // Frequency sweep curve
    float progress = static_cast<float>( i ) / static_cast<float>( totalFrames );
    float curve = std::pow( progress, std::max( 0.01f, iDesc._FrequencyExp ) );
    float baseFreq = iDesc._StartFrequency + ( iDesc._EndFrequency - iDesc._StartFrequency ) * curve;

    // Arpeggio step or chord
    float semitones = 0.0f;
    if( iDesc._ArpSpeed > 0.001f )
    {
      int step = static_cast<int>( t / iDesc._ArpSpeed ) % 4;
      if( step == 1 ) semitones = iDesc._ArpInterval2;
      else if( step == 2 ) semitones = iDesc._ArpInterval3;
      else if( step == 3 ) semitones = iDesc._ArpInterval4;
    }

    float activeFreq = baseFreq * std::pow( 2.0f, semitones / 12.0f );
    float dtPhase = ( 2.0f * PI_F * activeFreq ) / static_cast<float>( sampleRate );
    phase += dtPhase;
    phaseHarm2 += dtPhase * 2.0f;
    phaseHarm3 += dtPhase * 3.0f;

    float wave = EvaluateOscillator( iDesc._Waveform, phase, iDesc._DutyCycle );
    if( iDesc._Harmonic2Weight > 0.0f )
    {
      wave += iDesc._Harmonic2Weight * EvaluateOscillator( iDesc._Waveform, phaseHarm2, iDesc._DutyCycle );
    }
    if( iDesc._Harmonic3Weight > 0.0f )
    {
      wave += iDesc._Harmonic3Weight * EvaluateOscillator( iDesc._Waveform, phaseHarm3, iDesc._DutyCycle );
    }

    // Chord synthesis if simultaneous intervals requested (ArpSpeed == 0)
    if( iDesc._ArpSpeed <= 0.001f && ( iDesc._ArpInterval2 != 0.0f || iDesc._ArpInterval3 != 0.0f ) )
    {
      float count = 1.0f;
      if( iDesc._ArpInterval2 != 0.0f )
      {
        float f2 = baseFreq * std::pow( 2.0f, iDesc._ArpInterval2 / 12.0f );
        wave += EvaluateOscillator( iDesc._Waveform, ( 2.0f * PI_F * f2 * t ), iDesc._DutyCycle );
        count += 1.0f;
      }
      if( iDesc._ArpInterval3 != 0.0f )
      {
        float f3 = baseFreq * std::pow( 2.0f, iDesc._ArpInterval3 / 12.0f );
        wave += EvaluateOscillator( iDesc._Waveform, ( 2.0f * PI_F * f3 * t ), iDesc._DutyCycle );
        count += 1.0f;
      }
      wave /= count;
    }

    // Noise mix
    if( iDesc._NoiseMix > 0.0f )
    {
      float noise = ( ( (float)std::rand() / (float)RAND_MAX ) * 2.0f ) - 1.0f;
      wave = ( wave * ( 1.0f - iDesc._NoiseMix ) ) + ( noise * iDesc._NoiseMix );
    }

    float sample = wave * env * iDesc._Volume;
    resource._PcmFrames[i] = std::clamp( sample, -1.0f, 1.0f );
  }

  _Impl->_SoundBank[iSoundId] = std::move( resource );
  return true;
}

bool AudioEngine::HasSound( const std::string & iSoundId ) const
{
  if( !_Impl )
  {
    return false;
  }
  return _Impl->_SoundBank.find( iSoundId ) != _Impl->_SoundBank.end();
}

void AudioEngine::PlaySound2D( const std::string & iSoundId,
                              float iVolume,
                              float iPitch )
{
  if( !_Impl || !_Impl->_Initialized )
  {
    return;
  }

  auto it = _Impl->_SoundBank.find( iSoundId );
  if( it == _Impl->_SoundBank.end() )
  {
    return;
  }

  const SoundResource & res = it->second;
  if( res._PcmFrames.empty() )
  {
    return;
  }

  AudioVoice * voice = _Impl->AcquireVoice();
  if( !voice )
  {
    return;
  }

  ma_audio_buffer_config bufConfig = ma_audio_buffer_config_init(
    ma_format_f32,
    res._Channels,
    res._PcmFrames.size() / res._Channels,
    res._PcmFrames.data(),
    nullptr
  );

  if( ma_audio_buffer_init( &bufConfig, &voice->_Buffer ) != MA_SUCCESS )
  {
    return;
  }

  ma_sound_group * targetGroup = ( res._Bus == AudioBus::Music ) ? &_Impl->_MusicGroup : &_Impl->_SfxGroup;
  if( ma_sound_init_from_data_source( &_Impl->_Engine, &voice->_Buffer, 0, targetGroup, &voice->_Sound ) != MA_SUCCESS )
  {
    ma_audio_buffer_uninit( &voice->_Buffer );
    return;
  }

  voice->_InUse = true;
  voice->_StartFrame = _Impl->_FrameCounter;

  ma_sound_set_spatialization_enabled( &voice->_Sound, MA_FALSE );
  ma_sound_set_volume( &voice->_Sound, std::clamp( iVolume, 0.0f, 2.0f ) );
  ma_sound_set_pitch( &voice->_Sound, std::clamp( iPitch, 0.1f, 4.0f ) );
  ma_sound_start( &voice->_Sound );
}

void AudioEngine::UpdateListener( const Vec3 & iPosition,
                                 const Vec3 & iForward,
                                 const Vec3 & iUp,
                                 const Vec3 & iVelocity )
{
  if( !_Impl || !_Impl->_Initialized )
  {
    return;
  }

  ma_engine_listener_set_position( &_Impl->_Engine, 0, iPosition.x, iPosition.y, iPosition.z );
  ma_engine_listener_set_direction( &_Impl->_Engine, 0, iForward.x, iForward.y, iForward.z );
  ma_engine_listener_set_world_up( &_Impl->_Engine, 0, iUp.x, iUp.y, iUp.z );
  ma_engine_listener_set_velocity( &_Impl->_Engine, 0, iVelocity.x, iVelocity.y, iVelocity.z );
}

void AudioEngine::PlaySound3D( const std::string & iSoundId,
                              const Vec3 & iWorldPos,
                              float iVolume,
                              float iPitch,
                              float iMinDistance,
                              float iMaxDistance )
{
  if( !_Impl || !_Impl->_Initialized )
  {
    return;
  }

  auto it = _Impl->_SoundBank.find( iSoundId );
  if( it == _Impl->_SoundBank.end() )
  {
    return;
  }

  const SoundResource & res = it->second;
  if( res._PcmFrames.empty() )
  {
    return;
  }

  AudioVoice * voice = _Impl->AcquireVoice();
  if( !voice )
  {
    return;
  }

  ma_audio_buffer_config bufConfig = ma_audio_buffer_config_init(
    ma_format_f32,
    res._Channels,
    res._PcmFrames.size() / res._Channels,
    res._PcmFrames.data(),
    nullptr
  );

  if( ma_audio_buffer_init( &bufConfig, &voice->_Buffer ) != MA_SUCCESS )
  {
    return;
  }

  ma_sound_group * targetGroup = ( res._Bus == AudioBus::Music ) ? &_Impl->_MusicGroup : &_Impl->_SfxGroup;
  if( ma_sound_init_from_data_source( &_Impl->_Engine, &voice->_Buffer, 0, targetGroup, &voice->_Sound ) != MA_SUCCESS )
  {
    ma_audio_buffer_uninit( &voice->_Buffer );
    return;
  }

  voice->_InUse = true;
  voice->_StartFrame = _Impl->_FrameCounter;

  ma_sound_set_spatialization_enabled( &voice->_Sound, MA_TRUE );
  ma_sound_set_position( &voice->_Sound, iWorldPos.x, iWorldPos.y, iWorldPos.z );
  ma_sound_set_min_distance( &voice->_Sound, std::max( 0.1f, iMinDistance ) );
  ma_sound_set_max_distance( &voice->_Sound, std::max( iMinDistance + 0.1f, iMaxDistance ) );
  ma_sound_set_attenuation_model( &voice->_Sound, ma_attenuation_model_inverse );
  ma_sound_set_volume( &voice->_Sound, std::clamp( iVolume, 0.0f, 2.0f ) );
  ma_sound_set_pitch( &voice->_Sound, std::clamp( iPitch, 0.1f, 4.0f ) );
  ma_sound_start( &voice->_Sound );
}

void AudioEngine::StopAllSounds()
{
  if( !_Impl || !_Impl->_Initialized )
  {
    return;
  }

  for( size_t i = 0; i < MAX_VOICES; ++i )
  {
    if( _Impl->_Voices[i]._InUse )
    {
      ma_sound_stop( &_Impl->_Voices[i]._Sound );
      ma_sound_uninit( &_Impl->_Voices[i]._Sound );
      ma_audio_buffer_uninit( &_Impl->_Voices[i]._Buffer );
      _Impl->_Voices[i]._InUse = false;
    }
  }
}

} // namespace RTRT
