/*
 *
 */

#ifndef _STRUCTURES_GLSL_
#define _STRUCTURES_GLSL_

// SCATTER_TYPE
#define SCATTER_NONE     0 // No scattering, the path stops here
#define SCATTER_EXPLICIT 1 // Explicit scatter direction
#define SCATTER_RANDOM   2 // Random scatter direction

struct Ray
{
  vec3 _Orig;
  vec3 _Dir;
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

struct ScatterRecord
{
  uint  _Type;
  vec3  _Dir;
  float _P;            // The probability of choosing the direction
  vec3  _Attenuation;  // The attenuation for the direction: brdf * cosTheta
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

void InitializeScatterRecord( inout ScatterRecord ioSR )
{
  ioSR = ScatterRecord(SCATTER_NONE, vec3(0.f), 0.f, vec3(0.f));
}

#endif
