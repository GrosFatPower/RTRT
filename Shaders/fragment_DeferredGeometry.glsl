#version 410 core

#include Material.glsl

// Inputs from vertex shader (expected to provide world-space position, normal and uv)
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec2 fragUV;
flat in int v_MaterialID; // optional: present if vertex shader provides it

// G-buffer MRT outputs
layout(location = 0) out vec4 gAlbedo;   // RGB: albedo, A: unused / opacity
layout(location = 1) out vec4 gNormal;   // RGB: normal encoded in 0..1, A: unused
layout(location = 2) out vec4 gPosition; // RGB: world position, A: unused

// Optional fallback uniform (simple default albedo if no material sampling)
uniform vec3 u_DefaultAlbedo = vec3(0.8, 0.8, 0.8);
uniform vec3 u_CameraPos;

void main()
{
  HitPoint hitPoint;
  InitializeHitPoint(hitPoint);

  hitPoint._Pos        = fragWorldPos;
  hitPoint._Dist       = length(hitPoint._Pos - u_CameraPos);
  hitPoint._Normal     = normalize(fragNormal);
  hitPoint._UV         = fragUV;
  hitPoint._MaterialID = v_MaterialID;

  vec3 albedo = u_DefaultAlbedo;
  if ( v_MaterialID >= 0 )
  {
    Material mat;
    LoadMaterial(hitPoint, mat);
    albedo = mat._Albedo;
  }

  gAlbedo = vec4(albedo, 1.0);
  gNormal = vec4(hitPoint._Normal * 0.5 + 0.5, 1.0);
  gPosition = vec4(fragWorldPos, 1.0);
}
