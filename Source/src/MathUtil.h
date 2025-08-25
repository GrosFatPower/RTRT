#ifndef _MathUtil_
#define _MathUtil_

#define _USE_MATH_DEFINES

#include <math.h>
#include <cmath>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/extended_min_max.hpp"
#include <glm/gtx/hash.hpp>

typedef glm::vec2   Vec2;
typedef glm::vec3   Vec3;
typedef glm::vec4   Vec4;
typedef glm::ivec2  Vec2i;
typedef glm::ivec3  Vec3i;
typedef glm::ivec4  Vec4i;
typedef glm::uvec2  Vec2ui;
typedef glm::uvec3  Vec3ui;
typedef glm::uvec4  Vec4ui;
typedef glm::vec<4, uint8_t, glm::defaultp> Vec4b;
typedef glm::mat4x4 Mat4x4;

namespace RTRT
{

#define DeletePtr(ioPtr) { if ( ioPtr ) delete ioPtr; ioPtr = nullptr; }
#define DeleteTab(ioPtr) { if ( ioPtr ) delete[] ioPtr; ioPtr = nullptr; }

class MathUtil
{
public:

  template <typename T>
  static int Sign(T val) { return (T(0) < val) - (val < T(0)); }

  static float ToDegrees(float radians) { return radians * (180.f / static_cast<float>(M_PI)); };
  static float ToRadians(float degrees) { return degrees * (static_cast<float>(M_PI) / 180.f); };

  template <typename T>
  static T Clamp(T x, T lower, T upper) { return std::min(upper, std::max(x, lower)); };

  static Vec2 Min(const Vec2 & a, const Vec2 & b)
  {
    return Vec2(std::min(a.x, b.x), std::min(a.y, b.y));
  };

  static Vec3 Min(const Vec3 & a, const Vec3 & b)
  {
    return Vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
  };

  static Vec4 Min(const Vec4 & a, const Vec4 & b)
  {
    return Vec4(std::min(a.x, b.x),
                std::min(a.y, b.y),
                std::min(a.z, b.z),
                std::min(a.w, b.w));
  };

  static Vec2 Max(const Vec2 & a, const Vec2 & b)
  {
    return Vec2(std::max(a.x, b.x), std::max(a.y, b.y));
  };

  static Vec3 Max(const Vec3 & a, const Vec3 & b)
  {
    return Vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
  };

  static Vec4 Max(const Vec4 & a, const Vec4 & b)
  {
    return Vec4(std::max(a.x, b.x),
                std::max(a.y, b.y),
                std::max(a.z, b.z),
                std::max(a.w, b.w));
  };

  static float Dot(const Vec3 & a, const Vec3 & b)
  {
    return (a.x * b.x + a.y * b.y + a.z * b.z);
  };

  static Vec3 Clamp(const Vec3 & a, const Vec3 & lower, const Vec3 & upper)
  {
    return Vec3(
        Clamp(a.x, lower.x, upper.x),
        Clamp(a.y, lower.y, upper.y),
        Clamp(a.z, lower.z, upper.z)
    );
  }

  static float Lerp( float iStart, float iEnd, float iT )
  {
    return ( iStart + ( iEnd - iStart ) * iT );
  }

  static Vec3 Lerp( Vec3 iStart, Vec3 iEnd, float iT )
  {
    return Vec3(
        Lerp(iStart.x, iEnd.x, iT),
        Lerp(iStart.y, iEnd.y, iT),
        Lerp(iStart.z, iEnd.z, iT)
    );
  }

  static Vec4 Lerp( Vec4 iStart, Vec4 iEnd, float iT )
  {
    return Vec4(
        Lerp(iStart.x, iEnd.x, iT),
        Lerp(iStart.y, iEnd.y, iT),
        Lerp(iStart.z, iEnd.z, iT),
        Lerp(iStart.w, iEnd.w, iT)
    );
  }

  static Vec3 TransformPoint( const Vec3 & iPt, const Mat4x4 & iTransfoMat )
  {
    Vec3 res(0.);

    Vec4 transfoP = iTransfoMat * Vec4(iPt, 1.f);

    res = transfoP;
    if ( ( transfoP.w != 1.f ) && ( transfoP.w != 0.f ) )
      res /= transfoP.w;

    return res;
  }

  static Mat4x4 QuatToMatrix( float x, float y, float z, float s ) // q = s + ix + jy + kz
  {
    Mat4x4 out;

    const float x2 = x + x;
    const float y2 = y + y;
    const float z2 = z + z;

    const float xx = x * x2;
    const float xy = x * y2;
    const float xz = x * z2;

    const float yy = y * y2;
    const float yz = y * z2;
    const float zz = z * z2;

    const float sx = s * x2;
    const float sy = s * y2;
    const float sz = s * z2;

    out[0][0] = 1.0f - (yy + zz);
    out[0][1] = xy + sz;
    out[0][2] = xz - sy;
    out[0][3] = 0.0f;

    out[1][0] = xy - sz;
    out[1][1] = 1.0f - (xx + zz);
    out[1][2] = yz + sx;
    out[1][3] = 0.0f;

    out[2][0] = xz + sy;
    out[2][1] = yz - sx;
    out[2][2] = 1.0f - (xx + yy);
    out[2][3] = 0.0f;

    out[3][0] = 0;
    out[3][1] = 0;
    out[3][2] = 0;
    out[3][3] = 1.0f;

    return out;
  }

  static Mat4x4 QuatToMatrix(double x, double y, double z, double s) // q = s + ix + jy + kz
  {
    Mat4x4 out;

    const double x2 = x + x;
    const double y2 = y + y;
    const double z2 = z + z;

    const double xx = x * x2;
    const double xy = x * y2;
    const double xz = x * z2;

    const double yy = y * y2;
    const double yz = y * z2;
    const double zz = z * z2;

    const double sx = s * x2;
    const double sy = s * y2;
    const double sz = s * z2;

    out[0][0] = static_cast<float>(1.0f - (yy + zz));
    out[0][1] = static_cast<float>(xy + sz);
    out[0][2] = static_cast<float>(xz - sy);
    out[0][3] = 0.0f;

    out[1][0] = static_cast<float>(xy - sz);
    out[1][1] = static_cast<float>(1.0f - (xx + zz));
    out[1][2] = static_cast<float>(yz + sx);
    out[1][3] = 0.0f;

    out[2][0] = static_cast<float>(xz + sy);
    out[2][1] = static_cast<float>(yz - sx);
    out[2][2] = static_cast<float>(1.0f - (xx + yy));
    out[2][3] = 0.0f;

    out[3][0] = 0.0f;
    out[3][1] = 0.0f;
    out[3][2] = 0.0f;
    out[3][3] = 1.0f;

    return out;
  }

  static void Decompose(const Mat4x4 & iMat, Vec3 & oTranslation, glm::quat & oRotation, Vec3 & oScale)
  {
    oTranslation = iMat[3];
    for (int i = 0; i < 3; i++)
      oScale[i] = glm::length(Vec3(iMat[i]));
    const glm::mat3 rotMtx(
        Vec3(iMat[0]) / oScale[0],
        Vec3(iMat[1]) / oScale[1],
        Vec3(iMat[2]) / oScale[2]);
    oRotation = glm::quat_cast(rotMtx);
  }

  static void Decompose(const Mat4x4 & iMat, Vec3 & oRight, Vec3 & oUp, Vec3 & oForward, Vec3 & oPos)
  {
    oRight   = Vec3(iMat[0][0], iMat[0][1], iMat[0][2]);
    oUp      = Vec3(iMat[1][0], iMat[1][1], iMat[1][2]);
    oForward = Vec3(iMat[2][0], iMat[2][1], iMat[2][2]);
    oPos     = Vec3(iMat[3][0], iMat[3][1], iMat[3][2]);
  }

  static float DistanceToSegment( const Vec2 iA, const Vec2 iB, const Vec2 iP )
  {
    float dist = 0.f;

    Vec2 AP(iP - iA);
    Vec2 AB(iB - iA);

    float normAB = glm::length(AB);
    if ( normAB > 0.f )
    {
      float t = glm::dot(AP, AB) / ( normAB * normAB );
      t = glm::clamp(t, 0.f, 1.f);

      dist = glm::length(AP - t * AB);
    }
    else
      dist = glm::length(AP);

    return dist;
  }

  static float EdgeFunction(const Vec2 & iV0, const Vec2 & iV1, const Vec2 & iV2)
  {
    return (iV1.x - iV0.x) * (iV2.y - iV0.y) - (iV1.y - iV0.y) * (iV2.x - iV0.x);   // Counter-Clockwise edge function
    //return (iV2.x - iV0.x) * (iV1.y - iV0.y) - (iV2.y - iV0.y) * (iV1.x - iV0.x); // Clockwise edge function
  } 

  static float EdgeFunction(const Vec3 & iV0, const Vec3 & iV1, const Vec3 & iV2)
  {
    return (iV1.x - iV0.x) * (iV2.y - iV0.y) - (iV1.y - iV0.y) * (iV2.x - iV0.x);     // Counter-Clockwise edge function
    //  return (iV2.x - iV0.x) * (iV1.y - iV0.y) - (iV2.y - iV0.y) * (iV1.x - iV0.x); // Clockwise edge function
  }

  static bool EdgeFunctionCoefficients(const Vec3 & iV0, const Vec3 & iV1, const Vec3 & iV2, float oEdgeA[3], float oEdgeB[3], float oEdgeC[3], float & oInvArea)
  {
    oEdgeA[0] = iV1.y - iV2.y;
    oEdgeA[1] = iV2.y - iV0.y;
    oEdgeA[2] = iV0.y - iV1.y;

    oEdgeB[0] = iV2.x - iV1.x;
    oEdgeB[1] = iV0.x - iV2.x;
    oEdgeB[2] = iV1.x - iV0.x;

    oEdgeC[0] = iV1.x * iV2.y - iV2.x * iV1.y;
    oEdgeC[1] = iV2.x * iV0.y - iV0.x * iV2.y;
    oEdgeC[2] = iV0.x * iV1.y - iV1.x * iV0.y;

    float area = oEdgeC[0] + oEdgeC[1] + oEdgeC[2];

    if ( 0.f == area )
    {
      oInvArea = 0.f;
      return false;
    }

    oInvArea = 1.f / area;

    for (int i = 0; i < 3; ++i)
    {
      oEdgeA[i] *= oInvArea;
      oEdgeB[i] *= oInvArea;
      oEdgeC[i] *= oInvArea;
    }

    return true;
  }

  static bool EvalBarycentricCoordinates( const Vec3 & iFragCoord, const float iEdgeA[3], const float iEdgeB[3], const float iEdgeC[3], const float iInvZ[3], Vec3& oBaryCoord)
  {
    oBaryCoord.x = iEdgeA[0] * iFragCoord.x + iEdgeB[0] * iFragCoord.y + iEdgeC[0];
    oBaryCoord.y = iEdgeA[1] * iFragCoord.x + iEdgeB[1] * iFragCoord.y + iEdgeC[1];
    oBaryCoord.z = iEdgeA[2] * iFragCoord.x + iEdgeB[2] * iFragCoord.y + iEdgeC[2];
    if ( (oBaryCoord.x < 0.f)
      || (oBaryCoord.y < 0.f)
      || (oBaryCoord.z < 0.f))
      return false;

    // Perspective correction
    oBaryCoord.x *= iInvZ[0];
    oBaryCoord.y *= iInvZ[1];
    oBaryCoord.z *= iInvZ[2];

    return true;
  }

  static float Luminance( const Vec3 iRGBColor )
  {
    const Vec3 Luma = Vec3( 0.299, 0.587, 0.114 ); // UIT-R BT 601
    //const Vec3 Luma = Vec3(0.2126f, 0.7152f, 0.0722f); // UIT-R BT 709
    return glm::dot(Luma, iRGBColor);
  }
};

template <typename T>
struct AABB
{
  T _Low  =  T(std::numeric_limits<float>::infinity());
  T _High = -T(std::numeric_limits<float>::infinity());

  void Insert(T iP)
  {
    _Low  = MathUtil::Min(_Low, iP);
    _High = MathUtil::Max(_High, iP);
  }
};

}

#endif /* _MathUtil_ */
