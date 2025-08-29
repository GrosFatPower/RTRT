#ifndef _SutherlandHodgman_
#define _SutherlandHodgman_

/*
 * The SutherlandHodgman algorithm is an algorithm used for clipping polygons.
 * It works by extending each line of the convex clip polygon in turn and
 * selecting only vertices from the subject polygon that are on the visible side.
 * https://chaosinmotion.com/2016/05/22/3d-clipping-in-homogeneous-coordinates/
 * Implementation derived from: https://github.com/Zielon/CPURasterizer/tree/main/Rasterizer/Rasterizer/src/Engine
 */

#include "MathUtil.h"

#include <functional>
#include <vector>

namespace RTRT
{

using Predicate = std::function<bool(const Vec4&)>;
using Clip = std::function<void(Vec4&)>;

struct Polygon
{
  Polygon()
  {
    _Points.reserve(3);
  }

  struct Point
  {
    Vec4 _Pos;
    Vec3 _Distances; // Distances used for linear interpolation between segment and frustum plane
  };

  int Size() const { return static_cast<int>(_Points.size()); }
  void Clear() { _Points.clear(); }

  void Add( const Point & iP ) { _Points.emplace_back(iP); }

  Point & operator []( const size_t iInd ) { return _Points[iInd]; }
  const Point& operator [](const size_t iInd) const { return _Points[iInd]; }

  void SetFromTriangle( const Vec4 & iV0, const Vec4 & iV1, const Vec4 & iV2)
  {
    _Points.emplace_back(Point{ iV0, { 1.f, 0.f, 0.f } });
    _Points.emplace_back(Point{ iV1, { 0.f, 1.f, 0.f } });
    _Points.emplace_back(Point{ iV2, { 0.f, 0.f, 1.f } });
  }

private:

  std::vector<Point> _Points;
};

enum ClipCode
{
  INSIDE_BIT = 0,
  LEFT_BIT   = 1 << 0,
  RIGHT_BIT  = 1 << 1,
  BOTTOM_BIT = 1 << 2,
  TOP_BIT    = 1 << 3,
  NEAR_BIT   = 1 << 4,
  FAR_BIT    = 1 << 5
};

class SutherlandHodgman
{
public:

  static Polygon ClipTriangle( const Vec4 & iV0, const Vec4 & iV1, const Vec4 & iV2, uint32_t iClipCode );

  /**
    * Computers code for all planes which a given point fails
    * param iV Current point for clipping (after vertex shader)
    * return Code for planes to clip
    */
  static uint32_t ComputeClipCode(const Vec4 & iV);

private:

  /**
    * Clip polygon with given clip plane
    */
  static Polygon ClipPlane( uint32_t iClipPlane, const Polygon &, const Predicate &, const Clip & );

  /**
    * Clipping dot products
    * param iClipPlane Bit for the intersection plane
    * param v Point for which we check intersection
    * return Distance to the given plane
    */
  static float Dot( uint32_t iClipPlane, const Vec4 & iV );

  /**
    * Distance used for linear interpolation between segment and frustum plane
    */
  static float Point2PlaneDistance( uint32_t iClipPlane, const Vec4 & iV0, const Vec4 & iV1 );
};

inline float SutherlandHodgman::Dot( uint32_t iClipPlane, const Vec4 & iV )
{
  if (iClipPlane & LEFT_BIT  ) return iV.x + iV.w; /* iV * ( 1  0  0  1) */
  if (iClipPlane & RIGHT_BIT ) return iV.x - iV.w; /* iV * (-1  0  0  1) */
  if (iClipPlane & BOTTOM_BIT) return iV.y + iV.w; /* iV * ( 0  1  0  1) */
  if (iClipPlane & TOP_BIT   ) return iV.y - iV.w; /* iV * ( 0 -1  0  1) */
  if (iClipPlane & NEAR_BIT  ) return iV.z + iV.w; /* iV * ( 0  0  1  1) */
  if (iClipPlane & FAR_BIT   ) return iV.z - iV.w; /* iV * ( 0  0 -1  1) */

  return std::numeric_limits<float>::infinity();
}

inline uint32_t SutherlandHodgman::ComputeClipCode(const Vec4 & iV)
{
  uint32_t code = INSIDE_BIT;

  if (iV.x < -iV.w) code |= LEFT_BIT;
  if (iV.x >  iV.w) code |= RIGHT_BIT;
  if (iV.y < -iV.w) code |= BOTTOM_BIT;
  if (iV.y >  iV.w) code |= TOP_BIT;
  if (iV.z < -iV.w) code |= NEAR_BIT;
  if (iV.z >  iV.w) code |= FAR_BIT;

  return code;
}

inline float SutherlandHodgman::Point2PlaneDistance( uint32_t iClipPlane, const Vec4 & iV0, const Vec4 & iV1 )
{
  return Dot(iClipPlane, iV0) / (Dot(iClipPlane, iV0) - Dot(iClipPlane, iV1));
}

}

#endif /* _SutherlandHodgman_ */
