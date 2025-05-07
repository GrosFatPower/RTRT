/*
 *
 */

#ifndef _MATERIAL_GLSL_
#define _MATERIAL_GLSL_

#include Structures.glsl
#include Textures.glsl
#include RNG.glsl

uniform sampler2D u_MaterialsTexture;

#define ALPHA_MODE_OPAQUE 0
#define ALPHA_MODE_BLEND 1
#define ALPHA_MODE_MASK 2

struct Material
{
  int   _ID;
  int   _AlphaMode;          // 0: OPAQUE, 1: MASK, 2: BLEND
  vec3  _Emission;
  vec3  _Albedo;             // Albedo for dialectrics, F0 for metals
  vec3  _F0;                 // Base reflectance
  float _Roughness;
  float _Metallic;           // Metallic parameter. 0.0 for dialectrics, 1.0 for metals
  float _Reflectance;        // Fresnel reflectance for dialectircs between [0.0, 1.0]
  float _Subsurface;         // Disney BRDF
  float _Sheen;              // Disney BRDF
  float _SheenTint;          // Disney BRDF
  float _Anisotropic;        // Disney BRDF
  float _SpecTrans;          // Disney BRDF
  float _SpecTint;           // Disney BRDF
  float _Clearcoat;          // Disney BRDF
  float _ClearcoatRoughness; // Disney BRDF
  float _IOR;
  float _Opacity;
  float _AlphaCutoff;        // Alpha cutoff for MASK mode
  float _Ax;                 // Disney BRDF
  float _Ay;                 // Disney BRDF
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
  vec4 Params4 = texelFetch(u_MaterialsTexture, ivec2(index + 3, 0), 0); // _Metallic       | _Roughness              | _Subsurface     | _SpecTint
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
  oMat._SpecTint               = Params4.w;

  oMat._Sheen                  = Params5.x;
  oMat._SheenTint              = Params5.y;
  oMat._Clearcoat              = Params5.z;
  oMat._ClearcoatRoughness     = mix(0.1f, 0.001f, Params5.w); // Params5.w = _ClearcoatGloss

  oMat._SpecTrans              = Params6.x;
  oMat._IOR                    = Params6.y;
  // oMat._MediumType            = Params6.z;
  // oMat._MediumDensity         = Params6.w;

  int baseColorTexID           = int(Params7.x);
  int metallicRoughnessTexID   = int(Params7.y);
  int normalMapTexID           = int(Params7.z);
  int emissionMapTexID         = int(Params7.w);

  oMat._Opacity                = Params8.x;
  oMat._AlphaMode              = int(Params8.y);
  oMat._AlphaCutoff            = Params8.z;
  oMat._Reflectance            = Params8.w;

  if ( baseColorTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, baseColorTexID).x;
    if ( texArrayID >= 0 )
    {
      vec4 texColor = texture( u_TexArrayTexture, vec3( ioClosestHit._UV, float( texArrayID ) ) );
      oMat._Albedo *= texColor.rgb;
      oMat._Opacity *= texColor.a;
    }
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

  float aspect = sqrt(1.f - oMat._Anisotropic * .9f);
  oMat._Ax = max(0.001f, oMat._Roughness / aspect);
  oMat._Ay = max(0.001f, oMat._Roughness * aspect);

  // Base reflectance
  oMat._F0 = vec3(0.16f * pow(oMat._Reflectance, 2.));
  oMat._F0 = mix(oMat._F0, oMat._Albedo, oMat._Metallic);
}

// ----------------------------------------------------------------------------
// LoadOpacityValues
// ----------------------------------------------------------------------------
float LoadOpacityValues( in int iMatID, in vec2 iUV, out int oAlphaMode, out float oAlphaCutoff )
{
  int index = iMatID * 8;

  vec4 Params7 = texelFetch(u_MaterialsTexture, ivec2(index + 6, 0), 0); // _BaseColorTexId | _MetallicRoughnessTexID | _NormalMapTexID | _EmissionMapTexID
  vec4 Params8 = texelFetch(u_MaterialsTexture, ivec2(index + 7, 0), 0); // _Opacity        | _AlphaMode              | _AlphaCutoff    | _Reflectance

  float opacity = Params8.x;
  oAlphaMode   = int(Params8.y);
  oAlphaCutoff = Params8.z;

  int baseColorTexID = int(Params7.x);
  if ( baseColorTexID >= 0 )
  {
    int texArrayID = texelFetch(u_TexIndTexture, baseColorTexID).x;
    if ( texArrayID >= 0 )
    {
      vec4 texColor = texture( u_TexArrayTexture, vec3( iUV, float( texArrayID ) ) );
      opacity *= texColor.a;
    }
  }

  return opacity;
}

// ----------------------------------------------------------------------------
// IsOpaque
// ----------------------------------------------------------------------------
bool IsOpaque( in float iOpacity, in int iAlphaMode, in float iAlphaCutoff )
{
  // Opaque
  if ( ALPHA_MODE_OPAQUE == iAlphaMode )
  {
  }

  // Blend
  else if ( ALPHA_MODE_BLEND == iAlphaMode )
  {
    if ( iOpacity < 1.0f )
    {
      if ( rand() > iOpacity )
        return false;
    }
  }
  
  // Mask
  else if ( ALPHA_MODE_MASK == iAlphaMode )
  {
    if ( iOpacity < iAlphaCutoff )
      return false;
  } 

  return true;
}

// ----------------------------------------------------------------------------
// IsOpaque
// ----------------------------------------------------------------------------
bool IsOpaque( in int iMatID, in vec2 iUV )
{
  if ( iMatID >= 0 )
  {
    int alphaMode = 0;
    float alphaCutoff = 0;
    float opacity = LoadOpacityValues( iMatID, iUV, alphaMode, alphaCutoff);
    return IsOpaque( opacity, alphaMode, alphaCutoff);
  }

  return true;
}

// ----------------------------------------------------------------------------
// IsOpaque
// ----------------------------------------------------------------------------
bool IsOpaque( in Material iMat )
{
  return IsOpaque( iMat._Opacity, iMat._AlphaMode, iMat._AlphaCutoff );
}

#endif
