#ifndef _Light_
#define _Light_

#include "Math.h"

namespace RTRT
{

enum class LightType
{
  RectLight = 0,
  SphereLight,
  DistantLight
};

struct Light
{
  Vec3       _Pos      = Vec3(0.f, 0.f, 0.f);
  Vec3       _Emission = Vec3(1.f, 1.f, 1.f);
  Vec3       _DirU     = Vec3(1.f, 0.f, 0.f);
  Vec3       _DirV     = Vec3(0.f, 1.f, 0.f);

  float      _Radius   = 1.f;
  float      _Area     = 1.f;
  float      _Type     = (float)LightType::SphereLight;
};

}

#endif /* _Light_ */
