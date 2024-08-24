/*
 *
 */

#define QUAD_LIGHT    0
#define SPHERE_LIGHT  1
#define DISTANT_LIGHT 2

struct Ray
{
  vec3 _Orig;
  vec3 _Dir;
};

struct Material
{
  int   _ID;
  int   _BaseColorTexID;
  vec3  _Albedo;
  vec3  _Emission;
  vec3  _Reflectance;
  float _Metallic;
  float _Roughness;
  float _IOR;
  float _Opacity;
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
};
