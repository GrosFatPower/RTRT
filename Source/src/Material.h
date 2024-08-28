#ifndef _Material_
#define _Material_

#include "MathUtil.h"

namespace RTRT
{

enum class AlphaMode
{
  Opaque = 0,
  Blend,
  Mask
};

enum class MediumType
{
  None,
  Absorb,
  Scatter,
  Emissive
};

struct Material
{
  // Params1
  Vec3  _Albedo                 = { 1.0f, 1.0f, 1.0f };
  float _ID                     = -1.f;

  // Params2
  Vec3  _Emission               = { 0.0f, 0.0f, 0.0f };
  float _Anisotropic            = 0.f;

  // Params3
  Vec3  _MediumColor            = { 1.0f, 1.0f, 1.0f };
  float _MediumAnisotropy       = 0.f;

  // Params4
  float _Metallic               = 0.f;
  float _Roughness              = 0.5f;
  float _Subsurface             = 0.f;
  float _SpecularTint           = 0.f;
 
  // Params5
  float _Sheen                  = 0.f;
  float _SheenTint              = 0.f;
  float _Clearcoat              = 0.f;
  float _ClearcoatGloss         = 0.f;
 
  // Params6
  float _SpecTrans              = 0.f;
  float _IOR                    = 1.5f;
  float _MediumType             = 0.f;
  float _MediumDensity          = 0.f;
 
  // Params7
  float _BaseColorTexId         = -1.f;
  float _MetallicRoughnessTexID = -1.f;
  float _NormalMapTexID         = -1.f;
  float _EmissionMapTexID       = -1.f;
 
  // Params8
  float _Opacity                = 1.f;
  float _AlphaMode              = 0.f;
  float _AlphaCutoff            = 0.f;
  float _Reflectance            = 0.5f;
};

}

#endif /* _Material_ */
