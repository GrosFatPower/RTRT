/*
 *
 */

#ifndef _SCENE_GLSL_
#define _SCENE_GLSL_

#include Lights.glsl

#define MAX_SPHERE_COUNT   32
#define MAX_PLANES_COUNT   32
#define MAX_BOX_COUNT      32

uniform int            u_NbSpheres;
uniform Sphere         u_Spheres[MAX_SPHERE_COUNT];
uniform int            u_NbPlanes;
uniform Plane          u_Planes[MAX_PLANES_COUNT];
uniform int            u_NbBoxes;
uniform Box            u_Boxes[MAX_BOX_COUNT];
uniform int            u_NbTriangles;
uniform int            u_NbMeshInstances;
uniform sampler2D      u_ScreenTexture;
uniform samplerBuffer  u_VtxTexture;
uniform samplerBuffer  u_VtxNormTexture;
uniform samplerBuffer  u_VtxUVTexture;
uniform samplerBuffer  u_MeshBBoxTexture;
uniform isamplerBuffer u_VtxIndTexture;
uniform isamplerBuffer u_MeshIDRangeTexture;

#endif
