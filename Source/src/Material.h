#ifndef _Material_
#define _Material_

#include "Math.h"

namespace RTRT
{


struct Material
{
  Vec3 _BaseColor;

  float _Metallic;
  float _Roughness;

  Material()
  : _BaseColor(Vec3(1.f, 1.f, 1.f))
  , _Metallic(0.f)
  , _Roughness(.5f)
  {}
};

}

#endif /* _Material_ */
