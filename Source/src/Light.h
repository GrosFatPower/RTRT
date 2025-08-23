#ifndef _Light_
#define _Light_

#include "MathUtil.h"

namespace RTRT
{

enum struct LightType
{
  RectLight = 0,
  SphereLight,
  DistantLight
};

struct Light
{
  Vec3       _Pos       = { 0.f, 0.f, 0.f };
  Vec3       _Emission  = { 1.f, 1.f, 1.f };
  Vec3       _DirU      = { 1.f, 0.f, 0.f };
  Vec3       _DirV      = { 0.f, 0.f, 1.f };
  float      _Intensity = 1.f;
  float      _Radius    = 1.f;
  float      _Area      = 4.f * static_cast<float>(M_PI);
  float      _Type      = (float)LightType::SphereLight;
};

}

#endif /* _Light_ */
