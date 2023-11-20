#ifndef _Camera_
#define _Camera_

#include "MathUtil.h"

namespace RTRT
{

class Camera
{
public:
  Camera();
  Camera(Vec3 iPos, Vec3 iLookAt, float iFOV);

  void Initialize(Vec3 iPos, Vec3 iLookAt, float iFOV);

  const Vec3 & GetPos()     const { return _Pos; }
  const Vec3 & GetUp()      const { return _Up; }
  const Vec3 & GetRight()   const { return _Right; }
  const Vec3 & GetForward() const { return _Forward; }

  float GetYaw()   const { return _Yaw; }
  float GetPitch() const { return _Pitch; }

  float GetRadius() const { return _Radius; }
  void SetRadius( float iRadius );
  void IncreaseRadius( float iIncr );

  void SetFOV( float iFOV );
  float GetFOV() const { return _FOV; } // radian

  void SetFOVInDegrees( float iFOV );
  float GetFOVInDegrees() const;

  void SetZNearFar( float iZNear, float iZFar);
  void GetZNearFar( float & oZNear, float & oZFar) const;

  void SetFocalDist( float iFocalDist ) { _FocalDist = iFocalDist; }
  float GetFocalDist() const { return _FocalDist; }

  void SetAperture( float iAperture ) { _Aperture = iAperture; }
  float GetAperture() const { return _Aperture; }

  void Yaw(float iDx);
  void Pitch(float iDy);
  void OffsetOrientations(float iYaw, float iPitch);
  void Strafe(float iDx, float iDy);

  void LookAt( Vec3 iPivot );

  // Model view matrix, column-major
  void ComputeLookAtMatrix( Mat4x4 & oM );

  // Perspective projection matrix, column-major / RH / vertival fov in degrees / Z [-1,1]
  void ComputePerspectiveProjMatrix( float iAspectRatio, Mat4x4 & oM );

  // Frustum matrix, column-major / RH / vertival fov in degrees / Z [-1,1]
  void ComputeFrustum( float iLeft, float iRight, float iBottom, float iTop, float iZNear, float iZFar, Mat4x4 & oM );

private:

  void Update();

  float ComputeVerticalFOV( float iAspectRatio ) const;

  Vec3 _Pos;
  Vec3 _Up;
  Vec3 _Right;
  Vec3 _Forward;
  Vec3 _WorldUp;
  Vec3 _Pivot;

  float _FOV;   // horizontal / radian
  float _Pitch; // degrees
  float _Yaw;   // degrees
  float _Radius;

  float _FocalDist = .1f;
  float _Aperture  = 0.f;

  float _ZNear = 1.f;
  float _ZFar  = 1000.f;
};

inline void Camera::SetZNearFar( float iZNear, float iZFar) {
  _ZNear = iZNear; _ZFar = iZFar; }

inline void Camera::GetZNearFar( float & oZNear, float & oZFar) const {
  oZNear = _ZNear; oZFar = _ZFar; }

}

#endif /* _Camera_ */
