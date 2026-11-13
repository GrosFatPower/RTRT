#include "Animation.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace RTRT
{

namespace
{

Vec4 NormalizeQuat( const Vec4 & iQuat )
{
  const float len = glm::length(iQuat);
  if ( len > 1e-8f )
    return iQuat / len;
  return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

Vec4 SlerpQuat( const Vec4 & iQ0, const Vec4 & iQ1, float iT )
{
  Vec4 q0 = NormalizeQuat(iQ0);
  Vec4 q1 = NormalizeQuat(iQ1);

  float dot = glm::dot(q0, q1);
  if ( dot < 0.0f )
  {
    q1 = -q1;
    dot = -dot;
  }

  if ( dot > 0.9995f )
    return NormalizeQuat(glm::mix(q0, q1, iT));

  dot = std::min(1.0f, std::max(-1.0f, dot));
  const float theta0 = std::acos(dot);
  const float theta = theta0 * iT;
  const float sinTheta = std::sin(theta);
  const float sinTheta0 = std::sin(theta0);

  const float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
  const float s1 = sinTheta / sinTheta0;
  return NormalizeQuat(s0 * q0 + s1 * q1);
}

Vec4 EvalCubicHermite( const Vec4 & iP0, const Vec4 & iM0, const Vec4 & iP1, const Vec4 & iM1, float iT, float iDeltaTime )
{
  const float t2 = iT * iT;
  const float t3 = t2 * iT;
  const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
  const float h10 = t3 - 2.0f * t2 + iT;
  const float h01 = -2.0f * t3 + 3.0f * t2;
  const float h11 = t3 - t2;

  return h00 * iP0 + ( h10 * iDeltaTime ) * iM0 + h01 * iP1 + ( h11 * iDeltaTime ) * iM1;
}

} // namespace

// ----------------------------------------------------------------------------
// AnimationSampler
// ----------------------------------------------------------------------------
float AnimationSampler::GetMinTime() const
{
  if ( _InputTimes.empty() )
    return 0.0f;
  return _InputTimes.front();
}

// ----------------------------------------------------------------------------
float AnimationSampler::GetMaxTime() const
{
  if ( _InputTimes.empty() )
    return 0.0f;
  return _InputTimes.back();
}

// ----------------------------------------------------------------------------
void AnimationSampler::FindKeyframeInterval( float iTime, size_t & oIdx0, size_t & oIdx1, float & oT, float & oDeltaTime ) const
{
  const size_t n = _InputTimes.size();
  if ( n <= 1 )
  {
    oIdx0 = 0;
    oIdx1 = 0;
    oT = 0.0f;
    oDeltaTime = 0.0f;
    return;
  }

  if ( iTime <= _InputTimes.front() )
  {
    oIdx0 = 0;
    oIdx1 = 0;
    oT = 0.0f;
    oDeltaTime = 0.0f;
    return;
  }

  if ( iTime >= _InputTimes.back() )
  {
    oIdx0 = n - 1;
    oIdx1 = n - 1;
    oT = 0.0f;
    oDeltaTime = 0.0f;
    return;
  }

  auto it = std::upper_bound(_InputTimes.begin(), _InputTimes.end(), iTime);
  size_t idx1 = static_cast<size_t>(it - _InputTimes.begin());
  if ( idx1 == 0 )
    idx1 = 1;
  size_t idx0 = idx1 - 1;

  const float t0 = _InputTimes[idx0];
  const float t1 = _InputTimes[idx1];
  const float dt = t1 - t0;

  oIdx0 = idx0;
  oIdx1 = idx1;
  oDeltaTime = dt;
  oT = ( dt > 1e-8f ) ? ( iTime - t0 ) / dt : 0.0f;
}

// ----------------------------------------------------------------------------
Vec3 AnimationSampler::SampleVec3( float iTime ) const
{
  if ( _InputTimes.empty() || _OutputValues.empty() )
    return Vec3(0.0f);

  const size_t stride = ( _Interpolation == AnimationInterpolation::CubicSpline ) ? 3 : 1;
  const size_t valOffset = ( _Interpolation == AnimationInterpolation::CubicSpline ) ? 1 : 0;

  if ( _InputTimes.size() == 1 )
  {
    if ( valOffset < _OutputValues.size() )
      return Vec3(_OutputValues[valOffset]);
    return Vec3(0.0f);
  }

  size_t idx0 = 0;
  size_t idx1 = 0;
  float t = 0.0f;
  float dt = 0.0f;
  FindKeyframeInterval(iTime, idx0, idx1, t, dt);

  if ( idx0 == idx1 || _Interpolation == AnimationInterpolation::Step )
  {
    const size_t pos = idx0 * stride + valOffset;
    if ( pos < _OutputValues.size() )
      return Vec3(_OutputValues[pos]);
    return Vec3(0.0f);
  }

  if ( _Interpolation == AnimationInterpolation::CubicSpline )
  {
    const size_t base0 = idx0 * 3;
    const size_t base1 = idx1 * 3;
    if ( base1 + 2 < _OutputValues.size() )
    {
      const Vec4 & p0 = _OutputValues[base0 + 1];
      const Vec4 & b0 = _OutputValues[base0 + 2]; // out-tangent
      const Vec4 & a1 = _OutputValues[base1 + 0]; // in-tangent
      const Vec4 & p1 = _OutputValues[base1 + 1];
      return Vec3(EvalCubicHermite(p0, b0, p1, a1, t, dt));
    }
  }

  if ( idx1 < _OutputValues.size() )
    return Vec3(glm::mix(_OutputValues[idx0], _OutputValues[idx1], t));

  return Vec3(0.0f);
}

// ----------------------------------------------------------------------------
Vec4 AnimationSampler::SampleQuat( float iTime ) const
{
  if ( _InputTimes.empty() || _OutputValues.empty() )
    return Vec4(0.0f, 0.0f, 0.0f, 1.0f);

  const size_t stride = ( _Interpolation == AnimationInterpolation::CubicSpline ) ? 3 : 1;
  const size_t valOffset = ( _Interpolation == AnimationInterpolation::CubicSpline ) ? 1 : 0;

  if ( _InputTimes.size() == 1 )
  {
    if ( valOffset < _OutputValues.size() )
      return NormalizeQuat(_OutputValues[valOffset]);
    return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
  }

  size_t idx0 = 0;
  size_t idx1 = 0;
  float t = 0.0f;
  float dt = 0.0f;
  FindKeyframeInterval(iTime, idx0, idx1, t, dt);

  if ( idx0 == idx1 || _Interpolation == AnimationInterpolation::Step )
  {
    const size_t pos = idx0 * stride + valOffset;
    if ( pos < _OutputValues.size() )
      return NormalizeQuat(_OutputValues[pos]);
    return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
  }

  if ( _Interpolation == AnimationInterpolation::CubicSpline )
  {
    const size_t base0 = idx0 * 3;
    const size_t base1 = idx1 * 3;
    if ( base1 + 2 < _OutputValues.size() )
    {
      const Vec4 & p0 = _OutputValues[base0 + 1];
      const Vec4 & b0 = _OutputValues[base0 + 2];
      const Vec4 & a1 = _OutputValues[base1 + 0];
      const Vec4 & p1 = _OutputValues[base1 + 1];
      return NormalizeQuat(EvalCubicHermite(p0, b0, p1, a1, t, dt));
    }
  }

  if ( idx1 < _OutputValues.size() )
    return SlerpQuat(_OutputValues[idx0], _OutputValues[idx1], t);

  return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

// ----------------------------------------------------------------------------
// AnimationClip
// ----------------------------------------------------------------------------
void AnimationClip::ComputeDuration()
{
  _Duration = 0.0f;
  for ( const auto & sampler : _Samplers )
  {
    const float maxT = sampler.GetMaxTime();
    if ( maxT > _Duration )
      _Duration = maxT;
  }
}

// ----------------------------------------------------------------------------
// SceneNode
// ----------------------------------------------------------------------------
Mat4x4 SceneNode::ComputeLocalTransform() const
{
  if ( !_HasTRS )
    return _InitialLocalMatrix;

  const Mat4x4 T = glm::translate(Mat4x4(1.0f), _Translation);
  const Mat4x4 R = MathUtil::QuatToMatrix(_Rotation.x, _Rotation.y, _Rotation.z, _Rotation.w);
  const Mat4x4 S = glm::scale(Mat4x4(1.0f), _Scale);
  return T * R * S;
}

// ----------------------------------------------------------------------------
void SceneNode::ResetToInitialPose()
{
  _Translation    = _InitialTranslation;
  _Rotation       = _InitialRotation;
  _Scale          = _InitialScale;
  _LocalTransform = ComputeLocalTransform();
}

} // namespace RTRT
