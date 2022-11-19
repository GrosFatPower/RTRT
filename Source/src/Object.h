#ifndef _Object_
#define _Object_

#include "Math.h"

namespace RTRT
{

enum class ObjectType
{
  Plane = 0,
  Box,
  Sphere
};

struct Object
{
  int        _ObjectID = -1;
  Vec3       _Origin   = { 0.f, 0.f, 0.f };

protected:
  ObjectType _Type;
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
  Vec3 _Size  = { 1.f, 1.f, 1.f };

  Box() { _Type = ObjectType::Box; }
};

}

#endif /* _Object_ */
