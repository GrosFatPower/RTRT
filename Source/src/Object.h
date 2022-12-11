#ifndef _Object_
#define _Object_

#include "Math.h"

namespace RTRT
{

enum class ObjectType
{
  Unknown = 0,
  Plane,
  Box,
  Sphere
};

struct Object
{
  int        _ObjectID = -1;
  Vec3       _Origin   = { 0.f, 0.f, 0.f };
  ObjectType _Type     = ObjectType::Unknown;
};

struct Sphere : public Object
{
  float _Radius = 1.f;

  Sphere() { _Type = ObjectType::Sphere; }
};

struct Plane : public Object
{
  Vec3 _Normal  = { 0.f, 1.f, 0.f };

  Plane() { _Type = ObjectType::Plane; }
};

struct Box : public  Object
{
  Vec3 _Low   = { 0.f, 0.f, 0.f };
  Vec3 _High  = { 1.f, 1.f, 1.f };

  Box() { _Type = ObjectType::Box; }
};

}

#endif /* _Object_ */
