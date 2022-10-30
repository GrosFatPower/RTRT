#include "Camera.h"

namespace RTRT
{

Camera::Camera(Vec3 iPos, Vec3 iLookAt, float iFOV)
: _Pos(iPos)
, _FOV(MathUtil::ToRadians(iFOV))
, _Pivot(iLookAt)
, _WorldUp(Vec3(0.f, 1.f, 0.f))
, _Pitch(0.f)
, _Yaw(0.f)
, _Radius(glm::distance(_Pos, iLookAt))
{
  Vec3 dir = glm::normalize(_Pivot - _Pos);
  _Pitch = MathUtil::ToDegrees(asin(dir.y));
  _Yaw = MathUtil::ToDegrees(atan2(dir.z, dir.x));
}

void Camera::Update()
{
  _Forward.x = cos(MathUtil::ToRadians(_Yaw)) * cos(MathUtil::ToRadians(_Pitch));
  _Forward.y = sin(MathUtil::ToRadians(_Pitch));
  _Forward.z = sin(MathUtil::ToRadians(_Yaw)) * cos(MathUtil::ToRadians(_Pitch));
  _Forward = glm::normalize(_Forward);

  _Pos = _Pivot - _Forward * _Radius;

  _Right = glm::normalize(glm::cross(_Forward, _WorldUp));
  _Up = glm::normalize(glm::cross(_Right, _Forward));
}

}
