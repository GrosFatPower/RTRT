#ifndef _FpsHeadBob_
#define _FpsHeadBob_

#include "MathUtil.h"

namespace RTRT
{

struct FpsHeadBobSettings
{
  bool  _Enabled = true;
  float _Amplitude = 0.075f;
  float _Frequency = 0.90f;
  float _Sway = 0.040f;
  float _Smoothing = 25.0f;
};

struct FpsHeadBobUpdate
{
  float _DeltaTime = 0.f;
  float _HorizontalSpeed = 0.f;
  bool  _Grounded = false;
  bool  _Enabled = true;
  Vec3  _Right = Vec3(1.f, 0.f, 0.f);
  Vec3  _Up = Vec3(0.f, 1.f, 0.f);
};

class FpsHeadBob
{
public:
  void Reset();
  void Update( const FpsHeadBobSettings & iSettings, const FpsHeadBobUpdate & iUpdate );

  const Vec3 & GetOffset() const { return _Offset; }
  float GetPhase() const { return _Phase; }

protected:
  float _Phase = 0.f;
  Vec3  _Offset = Vec3(0.f);
};

}

#endif /* _FpsHeadBob_ */
