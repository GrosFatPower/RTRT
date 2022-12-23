#ifndef _Primitive_
#define _Primitive_

#include "Math.h"

namespace RTRT
{

enum class PrimitiveType
{
  Unknown = 0,
  Plane,
  Box,
  Sphere
};

struct Primitive
{
  int        _PrimID  = -1;
  Vec3       _Origin  = { 0.f, 0.f, 0.f };
  PrimitiveType _Type = PrimitiveType::Unknown;
};

struct Sphere : public Primitive
{
  float _Radius = 1.f;

  Sphere() { _Type = PrimitiveType::Sphere; }
};

struct Plane : public Primitive
{
  Vec3 _Normal  = { 0.f, 1.f, 0.f };

  Plane() { _Type = PrimitiveType::Plane; }
};

struct Box : public  Primitive
{
  Vec3 _Low   = { 0.f, 0.f, 0.f };
  Vec3 _High  = { 1.f, 1.f, 1.f };

  Box() { _Type = PrimitiveType::Box; }
};

}

#endif /* _Primitive_ */
