/*
 *
 */

#ifndef _MATERIAL_GLSL_
#define _MATERIAL_GLSL_

#include Structures.glsl
#include Textures.glsl

uniform sampler2D      u_MaterialsTexture;

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

// ----------------------------------------------------------------------------
// LoadMaterial
// ----------------------------------------------------------------------------
void LoadMaterial( inout HitPoint ioClosestHit, out Material oMat )
{
  if ( ioClosestHit._MaterialID < 0 )
    return;

  int index = ioClosestHit._MaterialID * 8;

  vec4 Params1 = texelFetch(u_MaterialsTexture, ivec2(index + 0, 0), 0); // _Albedo.r       | _Albedo.g               | _Albedo.b       | _ID
  vec4 Params2 = texelFetch(u_MaterialsTexture, ivec2(index + 1, 0), 0); // _Emission.r     | _Emission.g             | _Emission.b     | _Anisotropic
  vec4 Params3 = texelFetch(u_MaterialsTexture, ivec2(index + 2, 0), 0); // _MediumColor.r  | _MediumColor.g          | _MediumColor.b  | _MediumAnisotropy
  vec4 Params4 = texelFetch(u_MaterialsTexture, ivec2(index + 3, 0), 0); // _Metallic       | _Roughness              | _Subsurface     | _SpecularTint
  vec4 Params5 = texelFetch(u_MaterialsTexture, ivec2(index + 4, 0), 0); // _Sheen          | _SheenTint              | _Clearcoat      | _ClearcoatGloss
  vec4 Params6 = texelFetch(u_MaterialsTexture, ivec2(index + 5, 0), 0); // _SpecTrans      | _IOR                    | _MediumType     | _MediumDensity
  vec4 Params7 = texelFetch(u_MaterialsTexture, ivec2(index + 6, 0), 0); // _BaseColorTexId | _MetallicRoughnessTexID | _NormalMapTexID | _EmissionMapTexID
  vec4 Params8 = texelFetch(u_MaterialsTexture, ivec2(index + 7, 0), 0); // _Opacity        | _AlphaMode              | _AlphaCutoff    | _Reflectance

  oMat._Albedo                 = Params1.rgb;
  oMat._ID                     = int(Params1.w);

  oMat._Emission               = Params2.rgb;
  oMat._Anisotropic            = Params2.w;

  // oMat._MediumColor           = Params3.xyz;
  // oMat._MediumAnisotropy      = Params3.w;

  oMat._Metallic               = Params4.x;
  oMat._Roughness              = Params4.y;
  oMat._Subsurface             = Params4.z;
  oMat._SpecularTint           = Params4.w;

  oMat._Sheen                  = Params5.x;
  oMat._SheenTint              = Params5.y;
  oMat._Clearcoat              = Params5.z;
  oMat._ClearcoatGloss         = Params5.w;

  oMat._SpecularTrans          = Params6.x;
  oMat._IOR                    = Params6.y;
  // oMat._MediumType            = Params6.z;
  // oMat._MediumDensity         = Params6.w;

  int baseColorTexID           = int(Params7.x);
  int metallicRoughnessTexID   = int(Params7.y);
  int normalMapTexID           = int(Params7.z);
  int emissionMapTexID         = int(Params7.w);

  oMat._Opacity                = Params8.x;
  // oMat._AlphaMode             = Params8.y;
  // oMat._AlphaCutoff           = Params8.z;
  oMat._Reflectance            = Params8.w;

  if ( baseColorTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, baseColorTexID).x;
    if ( texArrayID >= 0 )
      oMat._Albedo = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).rgb * oMat._Albedo;
  }

  if ( normalMapTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, normalMapTexID).x;
    if ( texArrayID >= 0 )
    {  
      vec3 texNormal = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).xyz;
      texNormal = normalize(texNormal * 2.0 - 1.0);
      ioClosestHit._Normal = normalize(ioClosestHit._Tangent * texNormal.x + ioClosestHit._Bitangent * texNormal.y + ioClosestHit._Normal * texNormal.z);
    }
  }

  if ( metallicRoughnessTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, metallicRoughnessTexID).x;
    if ( texArrayID >= 0 )
    {  
      vec2 metalRoughness = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).rg;
      oMat._Metallic = metalRoughness.x;
      oMat._Roughness = metalRoughness.y;
    }
  }

  if ( emissionMapTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, emissionMapTexID).x;
    if ( texArrayID >= 0 )
      oMat._Emission = texture(u_TexArrayTexture, vec3(ioClosestHit._UV, float(texArrayID))).rgb;
  }
}

#endif
