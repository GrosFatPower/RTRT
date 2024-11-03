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

struct Material
{
  int   _ID;
  vec3  _Emission;
  vec3  _Albedo;         // Albedo for dialectrics, F0 for metals
  float _Roughness;
  float _Metallic;       // Metallic parameter. 0.0 for dialectrics, 1.0 for metals
  float _Reflectance;    // Fresnel reflectance for dialectircs between [0.0, 1.0]
  float _Subsurface;     // Disney BRDF
  float _Sheen;          // Disney BRDF
  float _SheenTint;      // Disney BRDF
  float _Anisotropic;    // Disney BRDF
  float _SpecularTrans;  // Disney BRDF
  float _SpecularTint;   // Disney BRDF
  float _Clearcoat;      // Disney BRDF
  float _ClearcoatGloss; // Disney BRDF
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
};

void InitializeHitPoint( inout HitPoint ioHitPoint )
{
  ioHitPoint = HitPoint(-1.f, vec3(0.f), vec3(0.f), vec3(0.f), vec3(0.f), vec2(0.f), -1, 0, true, false);
}

#endif
