#include "SutherlandHodgman.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// ClipPlane
// ----------------------------------------------------------------------------
Polygon SutherlandHodgman::ClipPlane( uint32_t iClipPlane, const Polygon & iInPolygon, const Predicate & iIsInside, const Clip & iClip )
{
  Polygon outPolygon;

  // For each edge/segment
  for ( uint32_t i = 0, j = 1; i < iInPolygon.Size(); ++i, ++j )
  {
    if ( iInPolygon.Size() == j )
      j = 0;

    const Polygon::Point pA = iInPolygon[i]; // Point A on the segment
    const Polygon::Point pB = iInPolygon[j]; // Point B on the segment

    float t = Point2PlaneDistance(iClipPlane, pA._Pos, pB._Pos);

    Polygon::Point pNew;
    pNew._Pos       = glm::mix(pA._Pos, pB._Pos, t);             // aPos * (1 - t) + bPos * t
    pNew._Distances = glm::mix(pA._Distances, pB._Distances, t); // aDist * (1 - t) + bDist * t

    iClip(pNew._Pos);

    if ( iIsInside(pA._Pos) )
    {
      if ( iIsInside(pB._Pos) )
        outPolygon.Add(pB);
      else
        outPolygon.Add(pNew);
    }
    else if ( iIsInside(pB._Pos) )
    {
      outPolygon.Add(pNew);
      outPolygon.Add(pB);
    }
  }

  return outPolygon;
}

// ----------------------------------------------------------------------------
// ClipTriangle
// ----------------------------------------------------------------------------
Polygon SutherlandHodgman::ClipTriangle( const Vec4 & iV0, const Vec4 & iV1, const Vec4 & iV2, uint32_t iClipCode )
{
  Polygon polygon;
  polygon.SetFromTriangle(iV0, iV1, iV2);

  // ===================================================================== //

  if ( iClipCode & LEFT_BIT )
    polygon = ClipPlane(
      LEFT_BIT, polygon,
      [](const glm::vec4& v) { return v.x >= -v.w; },
      [](glm::vec4& v) { v.x = -v.w; });

  if ( iClipCode & RIGHT_BIT )
    polygon = ClipPlane(
      RIGHT_BIT, polygon,
      [](const glm::vec4& v) { return v.x <= v.w; },
      [](glm::vec4& v) { v.x = v.w; });

  // ===================================================================== //

  if ( iClipCode & BOTTOM_BIT )
    polygon = ClipPlane(
      BOTTOM_BIT, polygon,
      [](const glm::vec4& v) { return v.y >= -v.w; },
      [](glm::vec4& v) { v.y = -v.w; });

  if ( iClipCode & TOP_BIT )
    polygon = ClipPlane(
      TOP_BIT, polygon,
      [](const glm::vec4& v) { return v.y <= v.w; },
      [](glm::vec4& v) { v.y = v.w; });

  // ===================================================================== //

  if ( iClipCode & NEAR_BIT )
    polygon = ClipPlane(
      NEAR_BIT, polygon,
      [](const glm::vec4& v) { return v.z >= -v.w; },
      [](glm::vec4& v) { v.z = -v.w; });

  if ( iClipCode & FAR_BIT )
    polygon = ClipPlane(
      FAR_BIT, polygon,
      [](const glm::vec4& v) { return v.z <= v.w; },
      [](glm::vec4& v) { v.z = v.w; });

  // ===================================================================== //

  for ( int i = 0; i < polygon.Size(); ++i )
  {
    if ( polygon[i]._Pos.w <= 0.f ) // ? -1.f
    {
      polygon.Clear();
      break;
    }
  }

  return polygon;
}

}
