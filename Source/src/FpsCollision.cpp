#include "FpsCollision.h"

#include <algorithm>
#include <cmath>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static float ProjectAabbRadius( const Vec3 & iHalfExtents, const Vec3 & iAxis )
{
  return std::abs(iAxis.x) * iHalfExtents.x
       + std::abs(iAxis.y) * iHalfExtents.y
       + std::abs(iAxis.z) * iHalfExtents.z;
}

static float ProjectObbRadius( const FpsCollisionObb & iObb, const Vec3 & iAxis )
{
  return std::abs(glm::dot(iAxis, iObb._Axis[0])) * iObb._HalfExtents.x
       + std::abs(glm::dot(iAxis, iObb._Axis[1])) * iObb._HalfExtents.y
       + std::abs(glm::dot(iAxis, iObb._Axis[2])) * iObb._HalfExtents.z;
}

static bool TestAxis( const Vec3 & iAabbCenter, const Vec3 & iAabbHalfExtents, const FpsCollisionObb & iObb, const Vec3 & iAxis )
{
  const float axisLenSq = glm::dot(iAxis, iAxis);
  if ( axisLenSq <= 1e-8f )
    return true;

  const Vec3 axis = iAxis / std::sqrt(axisLenSq);
  const float distance = std::abs(glm::dot(iObb._Center - iAabbCenter, axis));
  const float radiusA = ProjectAabbRadius(iAabbHalfExtents, axis);
  const float radiusB = ProjectObbRadius(iObb, axis);
  return distance <= ( radiusA + radiusB );
}

static Vec3 SafeNormalize( const Vec3 & iVector, const Vec3 & iFallback )
{
  const float len = glm::length(iVector);
  if ( len <= EPSILON )
    return iFallback;
  return iVector / len;
}

static AABB<Vec3> BuildObbBounds( const Vec3 & iCenter, const Vec3 iAxis[3], const Vec3 & iHalfExtents )
{
  AABB<Vec3> bounds;
  for ( int x = -1; x <= 1; x += 2 )
  {
    for ( int y = -1; y <= 1; y += 2 )
    {
      for ( int z = -1; z <= 1; z += 2 )
      {
        const Vec3 corner = iCenter
                          + iAxis[0] * iHalfExtents.x * static_cast<float>(x)
                          + iAxis[1] * iHalfExtents.y * static_cast<float>(y)
                          + iAxis[2] * iHalfExtents.z * static_cast<float>(z);
        bounds.Insert(corner);
      }
    }
  }
  return bounds;
}

// ----------------------------------------------------------------------------
// MakeObb
// ----------------------------------------------------------------------------
FpsCollisionObb FpsCollision::MakeObb( int iPropIndex, int iColliderIndex, const Vec3 & iLocalCenter, const Vec3 & iLocalHalfExtents, const Mat4x4 & iLocalToWorld )
{
  FpsCollisionObb obb;
  obb._PropIndex = iPropIndex;
  obb._ColliderIndex = iColliderIndex;
  obb._Center = MathUtil::TransformPoint(iLocalCenter, iLocalToWorld);

  const Vec3 columnX = Vec3(iLocalToWorld[0]);
  const Vec3 columnY = Vec3(iLocalToWorld[1]);
  const Vec3 columnZ = Vec3(iLocalToWorld[2]);
  const float scaleX = std::max(glm::length(columnX), 0.0001f);
  const float scaleY = std::max(glm::length(columnY), 0.0001f);
  const float scaleZ = std::max(glm::length(columnZ), 0.0001f);

  obb._Axis[0] = SafeNormalize(columnX, Vec3(1.f, 0.f, 0.f));
  obb._Axis[1] = SafeNormalize(columnY, Vec3(0.f, 1.f, 0.f));
  obb._Axis[2] = SafeNormalize(columnZ, Vec3(0.f, 0.f, 1.f));
  obb._HalfExtents = MathUtil::Max(iLocalHalfExtents, Vec3(0.001f)) * Vec3(scaleX, scaleY, scaleZ);
  obb._Bounds = BuildObbBounds(obb._Center, obb._Axis, obb._HalfExtents);
  return obb;
}

FpsCollisionObb FpsCollision::MakeObb( int iPropIndex, int iColliderIndex, const AABB<Vec3> & iLocalBounds, const Mat4x4 & iLocalToWorld )
{
  const Vec3 center = 0.5f * ( iLocalBounds._Low + iLocalBounds._High );
  const Vec3 halfExtents = 0.5f * ( iLocalBounds._High - iLocalBounds._Low );
  return MakeObb(iPropIndex, iColliderIndex, center, halfExtents, iLocalToWorld);
}

FpsCollisionObb FpsCollision::MakeObb( int iPropIndex, int iColliderIndex, const Vec3 & iWorldCenter, const Vec3 iWorldAxes[3], const Vec3 & iWorldHalfExtents )
{
  FpsCollisionObb obb;
  obb._PropIndex = iPropIndex;
  obb._ColliderIndex = iColliderIndex;
  obb._Center = iWorldCenter;
  obb._Axis[0] = SafeNormalize(iWorldAxes[0], Vec3(1.f, 0.f, 0.f));
  obb._Axis[1] = SafeNormalize(iWorldAxes[1], Vec3(0.f, 1.f, 0.f));
  obb._Axis[2] = SafeNormalize(iWorldAxes[2], Vec3(0.f, 0.f, 1.f));
  obb._HalfExtents = MathUtil::Max(iWorldHalfExtents, Vec3(0.001f));
  obb._Bounds = BuildObbBounds(obb._Center, obb._Axis, obb._HalfExtents);
  return obb;
}

// ----------------------------------------------------------------------------
// OverlapAABB
// ----------------------------------------------------------------------------
bool FpsCollision::OverlapAABB( const AABB<Vec3> & iA, const AABB<Vec3> & iB )
{
  return ( iA._Low.x <= iB._High.x ) && ( iA._High.x >= iB._Low.x )
      && ( iA._Low.y <= iB._High.y ) && ( iA._High.y >= iB._Low.y )
      && ( iA._Low.z <= iB._High.z ) && ( iA._High.z >= iB._Low.z );
}

// ----------------------------------------------------------------------------
// OverlapAabbObb
// ----------------------------------------------------------------------------
bool FpsCollision::OverlapAabbObb( const Vec3 & iAabbCenter, const Vec3 & iAabbHalfExtents, const FpsCollisionObb & iObb )
{
  if ( !TestAxis(iAabbCenter, iAabbHalfExtents, iObb, Vec3(1.f, 0.f, 0.f)) )
    return false;
  if ( !TestAxis(iAabbCenter, iAabbHalfExtents, iObb, Vec3(0.f, 1.f, 0.f)) )
    return false;
  if ( !TestAxis(iAabbCenter, iAabbHalfExtents, iObb, Vec3(0.f, 0.f, 1.f)) )
    return false;

  for ( int i = 0; i < 3; ++i )
  {
    if ( !TestAxis(iAabbCenter, iAabbHalfExtents, iObb, iObb._Axis[i]) )
      return false;
  }

  const Vec3 worldAxes[3] = { Vec3(1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, 0.f, 1.f) };
  for ( int i = 0; i < 3; ++i )
  {
    for ( int j = 0; j < 3; ++j )
    {
      if ( !TestAxis(iAabbCenter, iAabbHalfExtents, iObb, glm::cross(worldAxes[i], iObb._Axis[j])) )
        return false;
    }
  }

  return true;
}

// ----------------------------------------------------------------------------
// ResolveAabbObbAxis
// ----------------------------------------------------------------------------
bool FpsCollision::ResolveAabbObbAxis( const Vec3 & iAabbCenter, const Vec3 & iAabbHalfExtents, const FpsCollisionObb & iObb, int iAxis, float iDelta, FpsCollisionAxisResult & oResult )
{
  oResult = FpsCollisionAxisResult();
  if ( ( iAxis < 0 ) || ( iAxis > 2 ) )
    return false;
  if ( !OverlapAabbObb(iAabbCenter, iAabbHalfExtents, iObb) )
    return false;

  const Vec3 axis = ( 0 == iAxis ) ? Vec3(1.f, 0.f, 0.f) : ( 1 == iAxis ? Vec3(0.f, 1.f, 0.f) : Vec3(0.f, 0.f, 1.f) );
  Vec3 previousCenter = iAabbCenter;
  previousCenter[iAxis] -= iDelta;
  const float previousMin = previousCenter[iAxis] - iAabbHalfExtents[iAxis];
  const float previousMax = previousCenter[iAxis] + iAabbHalfExtents[iAxis];
  const float aMin = iAabbCenter[iAxis] - iAabbHalfExtents[iAxis];
  const float aMax = iAabbCenter[iAxis] + iAabbHalfExtents[iAxis];
  const float bRadius = ProjectObbRadius(iObb, axis);
  const float bCenter = iObb._Center[iAxis];
  const float bMin = bCenter - bRadius;
  const float bMax = bCenter + bRadius;

  const float tolerance = 0.002f;
  float correction = 0.f;
  if ( iDelta > 0.f )
  {
    if ( previousMax > bMin + tolerance )
      return false;
    correction = bMin - aMax;
  }
  else
  {
    if ( previousMin < bMax - tolerance )
      return false;
    correction = bMax - aMin;
  }

  if ( std::abs(correction) <= EPSILON )
    return false;

  oResult._Hit = true;
  oResult._Correction = correction;
  oResult._Normal = ( iDelta > 0.f ) ? -axis : axis;
  return true;
}

// ----------------------------------------------------------------------------
// ResolveSphereObb
// ----------------------------------------------------------------------------
bool FpsCollision::ResolveSphereObb( const Vec3 & iSphereCenter, float iSphereRadius, const FpsCollisionObb & iObb, FpsCollisionSphereResult & oResult )
{
  oResult = FpsCollisionSphereResult();

  AABB<Vec3> sphereBounds;
  sphereBounds.Insert(iSphereCenter - Vec3(iSphereRadius));
  sphereBounds.Insert(iSphereCenter + Vec3(iSphereRadius));
  if ( !OverlapAABB(sphereBounds, iObb._Bounds) )
    return false;

  const Vec3 delta = iSphereCenter - iObb._Center;
  Vec3 local(0.f);
  Vec3 closest(0.f);
  for ( int i = 0; i < 3; ++i )
  {
    local[i] = glm::dot(delta, iObb._Axis[i]);
    closest[i] = MathUtil::Clamp(local[i], -iObb._HalfExtents[i], iObb._HalfExtents[i]);
  }

  const Vec3 localDelta = local - closest;
  const float distSq = glm::dot(localDelta, localDelta);
  const float radius = std::max(0.001f, iSphereRadius);
  if ( distSq > radius * radius )
    return false;

  if ( distSq > 1e-8f )
  {
    const float dist = std::sqrt(distSq);
    Vec3 normal = Vec3(0.f);
    for ( int i = 0; i < 3; ++i )
      normal += iObb._Axis[i] * ( localDelta[i] / dist );

    oResult._Hit = true;
    oResult._Normal = SafeNormalize(normal, Vec3(0.f, 1.f, 0.f));
    oResult._Penetration = radius - dist;
    oResult._Correction = oResult._Normal * oResult._Penetration;
    return true;
  }

  int faceAxis = 0;
  float minFaceDistance = iObb._HalfExtents.x - std::abs(local.x);
  for ( int i = 1; i < 3; ++i )
  {
    const float faceDistance = iObb._HalfExtents[i] - std::abs(local[i]);
    if ( faceDistance < minFaceDistance )
    {
      minFaceDistance = faceDistance;
      faceAxis = i;
    }
  }

  const float sign = ( local[faceAxis] >= 0.f ) ? 1.f : -1.f;
  oResult._Hit = true;
  oResult._Normal = iObb._Axis[faceAxis] * sign;
  oResult._Penetration = radius + std::max(0.f, minFaceDistance);
  oResult._Correction = oResult._Normal * oResult._Penetration;
  return true;
}

// ----------------------------------------------------------------------------
// EulerTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsCollision::EulerTransform( const Vec3 & iPosition, const Vec3 & iRotation, const Vec3 & iScale )
{
  const Vec3 rotRad(MathUtil::ToRadians(iRotation.x),
                    MathUtil::ToRadians(iRotation.y),
                    MathUtil::ToRadians(iRotation.z));

  Mat4x4 transform = glm::translate(iPosition);
  transform = transform * glm::rotate(rotRad.x, Vec3(1.f, 0.f, 0.f));
  transform = transform * glm::rotate(rotRad.y, Vec3(0.f, 1.f, 0.f));
  transform = transform * glm::rotate(rotRad.z, Vec3(0.f, 0.f, 1.f));
  transform = transform * glm::scale(iScale);
  return transform;
}

}
