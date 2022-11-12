#ifndef _Object_
#define _Object_

#include "Math.h"

namespace RTRT
{

struct Object
{
  int _ObjectID = -1;
};

struct Sphere : Object
{
  float _Radius = 1.f;
};

struct Box : Object
{
  Vec3 _Low  = { -1.f, -1.f, -1.f };
  Vec3 _High = {  1.f,  1.f,  1.f };
};

}

#endif /* _Object_ */
