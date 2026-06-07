#include "FpsHeadBob.h"

#include <algorithm>
#include <cmath>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void FpsHeadBob::Reset()
{
  _Phase = 0.f;
  _Offset = Vec3(0.f);
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
void FpsHeadBob::Update( const FpsHeadBobSettings & iSettings, const FpsHeadBobUpdate & iUpdate )
{
  const float dt = MathUtil::Clamp(iUpdate._DeltaTime, 0.f, 0.1f);
  const float speed = std::max(0.f, iUpdate._HorizontalSpeed);
  const bool active = iSettings._Enabled && iUpdate._Enabled && iUpdate._Grounded && ( speed > 0.05f );

  Vec3 targetOffset(0.f);
  if ( active )
  {
    const float frequency = std::max(0.f, iSettings._Frequency);
    const float speedScale = MathUtil::Clamp(speed / 5.f, 0.5f, 1.8f);
    _Phase += dt * frequency * speedScale * 6.28318530718f;
    if ( _Phase > 6.28318530718f )
      _Phase = std::fmod(_Phase, 6.28318530718f);

    const float amplitude = std::max(0.f, iSettings._Amplitude);
    const float sway = std::max(0.f, iSettings._Sway);
    const float vertical = std::sin(_Phase * 2.f) * amplitude;
    const float lateral = std::sin(_Phase) * sway;
    targetOffset = iUpdate._Up * vertical + iUpdate._Right * lateral;
  }

  const float smoothing = std::max(0.f, iSettings._Smoothing);
  const float blend = ( smoothing <= EPSILON ) ? 1.f : ( 1.f - std::exp(-smoothing * dt) );
  _Offset = MathUtil::Lerp(_Offset, targetOffset, blend);

  if ( !active && ( glm::length(_Offset) < 0.0001f ) )
  {
    _Offset = Vec3(0.f);
    _Phase = 0.f;
  }
}

}
