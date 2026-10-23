#ifndef _FpsCollision_
#define _FpsCollision_

#include "MathUtil.h"

namespace RTRT
{

struct FpsCollisionObb
{
  int        _PropIndex = -1;
  int        _ColliderIndex = -1;
  Vec3       _Center = Vec3(0.f);
  Vec3       _Axis[3] = { Vec3(1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 1.f) };
  Vec3       _HalfExtents = Vec3(0.5f);
  AABB<Vec3> _Bounds;
};

struct FpsCollisionAxisResult
{
  bool  _Hit = false;
  float _Correction = 0.f;
  Vec3  _Normal = Vec3(0.f);
};

struct FpsCollisionSphereResult
{
  bool  _Hit = false;
  Vec3  _Correction = Vec3(0.f);
  Vec3  _Normal = Vec3(0.f, 1.f, 0.f);
  float _Penetration = 0.f;
};

class FpsCollision
{
public:
  static FpsCollisionObb MakeObb( int iPropIndex, int iColliderIndex, const Vec3 & iLocalCenter, const Vec3 & iLocalHalfExtents, const Mat4x4 & iLocalToWorld );
  static FpsCollisionObb MakeObb( int iPropIndex, int iColliderIndex, const AABB<Vec3> & iLocalBounds, const Mat4x4 & iLocalToWorld );
  static FpsCollisionObb MakeObb( int iPropIndex, int iColliderIndex, const Vec3 & iWorldCenter, const Vec3 iWorldAxes[3], const Vec3 & iWorldHalfExtents );

  static bool OverlapAABB( const AABB<Vec3> & iA, const AABB<Vec3> & iB );
  static bool OverlapAabbObb( const Vec3 & iAabbCenter, const Vec3 & iAabbHalfExtents, const FpsCollisionObb & iObb );
  static bool ResolveAabbObbAxis( const Vec3 & iAabbCenter, const Vec3 & iAabbHalfExtents, const FpsCollisionObb & iObb, int iAxis, float iDelta, FpsCollisionAxisResult & oResult );
  static bool ResolveSphereObb( const Vec3 & iSphereCenter, float iSphereRadius, const FpsCollisionObb & iObb, FpsCollisionSphereResult & oResult );

  static Mat4x4 EulerTransform( const Vec3 & iPosition, const Vec3 & iRotation, const Vec3 & iScale = Vec3(1.f) );
};

}

#endif /* _FpsCollision_ */
