#version 410 core

#include Material.glsl
#include Lights.glsl
#include Sampling.glsl

// Inputs from vertex shader (expected to provide world-space position, normal and uv)
in vec3 fragWorldPos;
in vec3 fragNormal;
in vec2 fragUV;
flat in int v_MaterialID; // optional: present if vertex shader provides it

// G-buffer MRT outputs
layout(location = 0) out vec4 gAlbedo;   // RGB: albedo, A: unused / opacity
layout(location = 1) out vec4 gNormal;   // RGB: normal encoded in 0..1, A: unused
layout(location = 2) out vec4 gPosition; // RGB: world position, A: unused
layout(location = 3) out vec4 gMaterial; // R: perceptual roughness, G: metallic, B: reflectance, A: unused
layout(location = 4) out vec4 gEmission; // RGB: emission, A: unused

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
  if ( !gl_FrontFacing )
    hitPoint._Normal *= -1.0;
  hitPoint._UV         = fragUV;
  hitPoint._MaterialID = v_MaterialID;
  ComputeTangentFrame(hitPoint._Normal, hitPoint._Pos, hitPoint._UV, hitPoint._Tangent, hitPoint._Bitangent);

  vec3 albedo = u_DefaultAlbedo;
  float roughness = 1.0;
  float metallic = 0.0;
  float reflectance = 0.5;
  vec3 emission = vec3(0.0);
  if ( v_MaterialID >= 0 )
  {
    Material mat;
    LoadMaterial(hitPoint, mat);

    if ( ( mat._AlphaMode == ALPHA_MODE_MASK ) && ( mat._Opacity < mat._AlphaCutoff ) )
      discard;

    albedo = mat._Albedo;
    roughness = mat._Roughness;
    metallic = mat._Metallic;
    reflectance = mat._Reflectance;
    emission = mat._Emission;
  }

  gAlbedo = vec4(albedo, 1.0);
  gNormal = vec4(hitPoint._Normal * 0.5 + 0.5, 1.0);
  gPosition = vec4(fragWorldPos, 1.0);
  gMaterial = vec4(roughness, metallic, reflectance, 1.0);
  gEmission = vec4(emission, 1.0);
}
