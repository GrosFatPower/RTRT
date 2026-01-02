/*
 *
 */

#ifndef _LIGHTS_GLSL_
#define _LIGHTS_GLSL_

#include Structures.glsl

#define MAX_LIGHT_COUNT    32

uniform Light          u_Lights[MAX_LIGHT_COUNT];
uniform int            u_NbLights;
uniform int            u_ShowLights;

#endif // _LIGHTS_GLSL_
