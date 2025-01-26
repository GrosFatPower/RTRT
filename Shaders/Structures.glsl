/*
 *
 */

#ifndef _STRUCTURES_GLSL_
#define _STRUCTURES_GLSL_

#define QUAD_LIGHT    0
#define SPHERE_LIGHT  1
#define DISTANT_LIGHT 2

struct Ray
{
  vec3 _Orig;
  vec3 _Dir;
};

struct Light
{
  vec3  _Pos;
  vec3  _Emission;
  vec3  _DirU;
  vec3  _DirV;
  float _Radius;
  float _Area;
  float _Type;
};

struct HitPoint
{
  float _Dist;
  vec3  _Pos;
  vec3  _Normal;
  vec3  _Tangent;
  vec3  _Bitangent;
  vec2  _UV;
  int   _MaterialID;
  int   _LightID;
  bool  _FrontFace;
  bool  _IsEmitter;
};

struct Sphere
{
  vec4  _CenterRad; // Center.xyz, radius.w
  int   _MaterialID;
};

struct Plane
{
  vec3  _Orig;
  vec3  _Normal;
  int   _MaterialID;
};

struct Box
{
  vec3  _Low;
  vec3  _High;
  mat4  _Transfom;
  int   _MaterialID;
};

struct Camera
{
  vec3  _Up;
  vec3  _Right;
  vec3  _Forward;
  vec3  _Pos;
  float _FOV;
  float _FocalDist;
  float _LensRadius;
};

void InitializeHitPoint( inout HitPoint ioHitPoint )
{
  ioHitPoint = HitPoint(-1.f, vec3(0.f), vec3(0.f), vec3(0.f), vec3(0.f), vec2(0.f), -1, 0, true, false);
}

#endif
