#ifndef _Object_
#define _Object_

#include "Math.h"

namespace RTRT
{

struct Object
{
  int  _ObjectID = -1;
  Vec3 _Origin   = { 0.f, 0.f, 0.f };
};

struct Sphere : Object
{
  float _Radius = 1.f;
};

struct Plane : Object
{
  Vec3 _Normal  = { 0.f, 1.f, 0.f };
};

struct Box : Object
{
  Vec3 _Size  = { 1.f, 1.f, 1.f };
};

}

#endif /* _Object_ */
