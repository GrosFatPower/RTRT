#ifndef _Light_
#define _Light_

#include "Math.h"

namespace RTRT
{


struct Light
{
  Vec3 _Position;
  Vec3 _Emission;

  Light() : _Position(Vec3(0.f, 0.f, 0.f)), _Emission(Vec3(1.f, 1.f, 1.f)) {}
};

}

#endif /* _Light_ */
