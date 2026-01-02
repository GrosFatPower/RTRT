/*
 *
 */

#ifndef _LIGHTS_GLSL_
#define _LIGHTS_GLSL_

// LIGHT_TYPE
#define QUAD_LIGHT    0
#define SPHERE_LIGHT  1
#define DISTANT_LIGHT 2

#define MAX_LIGHT_COUNT    32

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

uniform Light          u_Lights[MAX_LIGHT_COUNT];
uniform int            u_NbLights;
uniform int            u_ShowLights;

#endif // _LIGHTS_GLSL_
