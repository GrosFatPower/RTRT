/*
 *
 */

struct Ray
{
  vec3 _Orig;
  vec3 _Dir;
};

struct Material
{
  int   _ID;
  vec3  _Albedo;
  vec3  _Emission;
  vec3  _Reflectance;
  float _Metallic;
  float _Roughness;
  float _IOR;
  float _Opacity;
};

struct SphereLight
{
  vec3  _Pos;
  vec3  _Emission;
  float _Radius;
};

struct HitPoint
{
  float _Dist;
  vec3  _Pos;
  vec3  _Normal;
  vec2  _UV;
  int   _MaterialID;
  bool  _FrontFace;
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
