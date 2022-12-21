#ifndef _Math_
#define _Math_

#define _USE_MATH_DEFINES

#include <math.h>
#include <cmath>
#include <algorithm>

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtx/transform.hpp"
#include "glm/gtx/quaternion.hpp"


typedef glm::vec2   Vec2;
typedef glm::vec3   Vec3;
typedef glm::vec4   Vec4;
typedef glm::ivec2  Vec2i;
typedef glm::ivec3  Vec3i;
typedef glm::ivec4  Vec4i;
typedef glm::uvec2  Vec2ui;
typedef glm::uvec3  Vec3ui;
typedef glm::uvec4  Vec4ui;
typedef glm::mat4x4 Mat4x4;

struct MathUtil
{
public:

  template <typename T>
  static int sign(T val) { return (T(0) < val) - (val < T(0)); }

  static float ToDegrees(float radians) { return radians * (180.f / M_PI); };
  static float ToRadians(float degrees) { return degrees * (M_PI / 180.f); };
  static float Clamp(float x, float lower, float upper) { return std::min(upper, std::max(x, lower)); };

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
};

#endif /* _Math_ */
