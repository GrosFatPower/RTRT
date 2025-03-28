#include "Camera.h"

//#define USE_GLM_MATRIX_FUNC

namespace RTRT
{

Camera::Camera()
: _Pos({0.f, 0.f, -1.f})
, _Pivot({0.f, 0.f, 0.f})
, _FOV(MathUtil::ToRadians(80.f))
, _WorldUp({0.f, 1.f, 0.f})
, _Radius(glm::distance(_Pos, _Pivot))
{
  Vec3 dir = glm::normalize(_Pivot - _Pos);
  _Pitch = MathUtil::ToDegrees(asin(dir.y));
  _Yaw = MathUtil::ToDegrees(atan2(dir.z, dir.x));

  Update();
}

Camera::Camera(Vec3 iPos, Vec3 iLookAt, float iFOV)
: _Pos(iPos)
, _Pivot(iLookAt)
, _FOV(MathUtil::ToRadians(iFOV))
, _WorldUp({0.f, 1.f, 0.f})
, _Radius(glm::distance(_Pos, iLookAt))
{
  Vec3 dir = glm::normalize(_Pivot - _Pos);
  _Pitch = MathUtil::ToDegrees(asin(dir.y));
  _Yaw = MathUtil::ToDegrees(atan2(dir.z, dir.x));

  Update();
}

void Camera::Initialize(Vec3 iPos, Vec3 iLookAt, float iFOV)
{
  _Pos     = iPos;
  _Pivot   = iLookAt;
  _FOV     = MathUtil::ToRadians(iFOV);
  _WorldUp = Vec3(0.f, 1.f, 0.f);
  _Radius  = glm::distance(_Pos, iLookAt);

  Vec3 dir = glm::normalize(_Pivot - _Pos);
  _Pitch = MathUtil::ToDegrees(asin(dir.y));
  _Yaw = MathUtil::ToDegrees(atan2(dir.z, dir.x));

  Update();
}

void Camera::Update()
{
  _Forward.x = cos(MathUtil::ToRadians(_Yaw)) * cos(MathUtil::ToRadians(_Pitch));
  _Forward.y = sin(MathUtil::ToRadians(_Pitch));
  _Forward.z = sin(MathUtil::ToRadians(_Yaw)) * cos(MathUtil::ToRadians(_Pitch));
  _Forward = glm::normalize(_Forward);

  _Right = glm::normalize(glm::cross(_Forward, _WorldUp));
  _Up = glm::normalize(glm::cross(_Right, _Forward));

  if ( CameraMode::FreeLook == _Mode )
  {
    _Pivot = _Pos + _Forward * _Radius;
  }
  else if ( CameraMode::Orbit == _Mode )
  {
    _Pos = _Pivot - _Forward * _Radius;
  }
}

void Camera::SetFOV( float iFOV )
{
  _FOV = MathUtil::ToRadians(iFOV);
}

void Camera::SetFOVInDegrees( float iFOV )
{
  _FOV = MathUtil::ToRadians(iFOV);
}

float Camera::GetFOVInDegrees() const
{
  return MathUtil::ToDegrees(_FOV);
}

void Camera::SetRadius( float iRadius )
{
  _Radius = iRadius;
  Update();
}

void Camera::IncreaseRadius( float iIncr )
{
  SetRadius(_Radius + iIncr);
}

void Camera::Yaw(float iDx)
{
  _Yaw += iDx;
  Update();
}

void Camera::Pitch(float iDy)
{
  _Pitch += iDy;
  Update();
}

void Camera::OffsetOrientations(float iYaw, float iPitch)
{
  _Yaw += iYaw;
  _Pitch += iPitch;

  if ( fabs(_Yaw) > 360.f )
    _Yaw -= MathUtil::Sign(_Yaw) * 360.f * floor( fabs(_Yaw / 360.f) );
  if ( fabs(_Pitch) >= 90.f )
    _Pitch = MathUtil::Sign(_Pitch) * 89.999f;

  Update();
}

void Camera::Strafe(float iDx, float iDy)
{
  Vec3 translation = _Right * -iDx + _Up * iDy;

  if ( CameraMode::FreeLook == _Mode )
  {
    _Pos += translation;
  }
  else if ( CameraMode::Orbit == _Mode )
  {
    _Pivot += translation;
  }

  Update();
}

void Camera::Walk(float iDz)
{
  Vec3 translation = _Forward * iDz;

  if ( CameraMode::FreeLook == _Mode )
  {
    _Pos += translation;
  }
  else if ( CameraMode::Orbit == _Mode )
  {
    _Pivot += translation;
  }

  Update();
}

void Camera::LookAt( Vec3 iPivot )
{
  _Pivot = iPivot;
  Vec3 dir = glm::normalize(_Pivot - _Pos);

  Update();
}

void Camera::ComputeLookAtMatrix( Mat4x4 & oM )
{
#ifdef USE_GLM_MATRIX_FUNC
  oM = glm::lookAt(_Pos, _Pivot, _WorldUp);
#else
  Vec3 Z = glm::normalize(_Pos - _Pivot);
  Vec3 X = glm::normalize(glm::cross(_WorldUp, Z));
  Vec3 Y = glm::normalize(glm::cross(Z, X));
  
  oM[0][0] = X.x;                oM[0][1] = Y.x;                oM[0][2] = Z.x;                oM[0][3] = 0.f;
  oM[1][0] = X.y;                oM[1][1] = Y.y;                oM[1][2] = Z.y;                oM[1][3] = 0.f;
  oM[2][0] = X.z;                oM[2][1] = Y.z;                oM[2][2] = Z.z;                oM[2][3] = 0.f;
  oM[3][0] = -glm::dot(X, _Pos); oM[3][1] = -glm::dot(Y, _Pos); oM[3][2] = -glm::dot(Z, _Pos); oM[3][3] = 1.f;
#endif
}

void Camera::ComputePerspectiveProjMatrix( float iAspectRatio, Mat4x4 & oM, float * oTop, float * oRight )
{
  float vFOV = ComputeVerticalFOV(iAspectRatio);
  float top = _ZNear * tanf(vFOV * .5f);
  float right = top * iAspectRatio;

  ComputeFrustum(-right, right, -top, top, _ZNear, _ZFar, oM);

  if ( oTop )
    *oTop = top;
  if ( oRight )
    *oRight = right;
}

void Camera::ComputeFrustum( float iLeft, float iRight, float iBottom, float iTop, float iZNear, float iZFar, Mat4x4 & oM )
{
#ifdef USE_GLM_MATRIX_FUNC
  oM = glm::frustum(iLeft, iRight, iBottom, iTop, iZNear, iZFar);
#else
  float width  = iRight - iLeft;
  float height = iTop - iBottom;
  float depth  = iZFar - iZNear;
  
  oM[0][0] = 2.f * iZNear / width;
  oM[0][1] = 0.f;
  oM[0][2] = 0.f;
  oM[0][3] = 0.f;
  
  oM[1][0] = 0.f;
  oM[1][1] = 2.f * iZNear / height;
  oM[1][2] = 0.f;
  oM[1][3] = 0.f;
  
  oM[2][0] = ( iLeft + iRight ) / width;
  oM[2][1] = ( iBottom + iTop ) / height;
  oM[2][2] = (-iZFar -iZNear) / depth;
  oM[2][3] = -1.f;
  
  oM[3][0] = 0.f;
  oM[3][1] = 0.f;
  oM[3][2] = -2.f * iZNear * iZFar / depth;
  oM[3][3] = 0.f;
#endif
}

float Camera::ComputeVerticalFOV( float iAspectRatio ) const
{
  if ( iAspectRatio > 1.f )
    return 2.f * atanf(tanf(_FOV * .5f) / iAspectRatio);
  else
    return 2.f * atanf(tanf(_FOV * .5f) * iAspectRatio);
}

void Camera::ComputeRasterMatrix( int iWidth, int iHeight, Mat4x4 & oM )
{
  oM = glm::scale(Vec3(iWidth, iHeight, 1.f)) * glm::scale(Vec3(0.5f, 0.5f, 1.0f)) * glm::translate(Vec3(1.f, 1.f, 0.f));
}

}
