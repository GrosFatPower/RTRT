#ifndef _Math_
#define _Math_

#define _USE_MATH_DEFINES

#include <math.h>
#include <cmath>
#include <algorithm>

#include "glm\vec2.hpp"
#include "glm\vec3.hpp"
#include "glm\vec4.hpp"
#include "glm\mat4x4.hpp"

typedef glm::vec2   Vec2;
typedef glm::vec3   Vec3;
typedef glm::vec4   Vec4;
typedef glm::uvec2  Vec2i;
typedef glm::uvec3  Vec3i;
typedef glm::uvec4  Vec4i;
typedef glm::mat4x4 Mat4x4;

struct MathUtil
{
public:

  static inline float ToDegrees(float radians) { return radians * (180.f / M_PI); };
  static inline float ToRadians(float degrees) { return degrees * (M_PI / 180.f); };
  static inline float Clamp(float x, float lower, float upper) { return std::min(upper, std::max(x, lower)); };
};

#endif /* _Math_ */
