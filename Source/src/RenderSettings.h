#ifndef _RenderSettings_
#define _RenderSettings_

#include "MathUtil.h"

namespace RTRT
{

enum class ShadingType
{
  Flat = 0,
  Phong,
  PBR
};

enum class SamplingMode
{
  Nearest = 0,
  Bilinear,
  Trilinear
};

struct RenderSettings
{
  Vec2i        _RenderResolution      = { 0, 0 };
  Vec2i        _WindowResolution      = { 1920, 1080 };
  Vec2i        _TileResolution        = { -1, -1 };
  Vec3         _BackgroundColor       = { 0.f, 0.f, 0.f };
  Vec3         _UniformLightCol       = { .3f, .3f, .3f };
  Vec2i        _TextureSize           = { 2048, 2048 };
  bool         _ShowLights            = false;                  // PathTracer
  bool         _EnableBackGround      = true;
  bool         _EnableSkybox          = true;
  bool         _EnableUniformLight    = true;
  bool         _AutoScale             = false;                  // PathTracer
  bool         _RussianRoulette       = true;                   // PathTracer
  bool         _ToneMapping           = true;
  bool         _FXAA                  = false;
  bool         _Accumulate            = true;                   // PathTracer
  bool         _Denoise               = false;                  // PathTracer
  bool         _TiledRendering        = false;
  bool         _ShadowMapping         = true;                   // Deferred renderer
  bool         _ShowShadowMap         = false;                  // Deferred renderer debug
  bool         _SSAO                  = true;                   // Deferred renderer
  bool         _SSAOBlur              = true;                   // Deferred renderer
  bool         _SSR                   = false;                  // Deferred renderer
  bool         _SpecularIBL           = true;                   // Deferred renderer
  bool         _PBRDirectLighting     = true;                   // Deferred renderer
  bool         _Transparency          = true;                   // Deferred renderer
  SamplingMode _Sampling              = SamplingMode::Bilinear; // Raster
  bool         _WBuffer               = true;                   // Raster
  ShadingType  _ShadingType           = ShadingType::Phong;     // Raster
  int          _Bounces               = 1;                      // PathTracer
  int          _NbSamplesPerPixel     = 1;                      // PathTracer
  int          _RenderScale           = 100;
  int          _ShadowMapResolution   = 1024;                   // Deferred renderer
  int          _MaxShadowCastingLights = 4;                     // Deferred renderer
  int          _SSAOKernelSize        = 16;                     // Deferred renderer
  int          _SSRMaxSteps           = 48;                     // Deferred renderer
  float        _LowResRatio           = 0.1f;                   // PathTracer
  float        _TargetFPS             = 60.f;
  float        _Gamma                 = 2.f;
  float        _Exposure              = 1.5f;
  float        _SkyBoxRotation        = 0.f;
  float        _ShadowBias            = 0.02f;                  // Deferred renderer
  float        _ShadowFar             = 0.f;                    // Deferred renderer. Auto-fit when <= 0.
  float        _SSAORadius            = 0.5f;                   // Deferred renderer
  float        _SSAOBias              = 0.025f;                 // Deferred renderer
  float        _SSAOIntensity         = 1.0f;                   // Deferred renderer
  float        _SSRStepSize           = 0.18f;                  // Deferred renderer
  float        _SSRMaxDistance        = 35.f;                   // Deferred renderer
  float        _SSRThickness          = 0.25f;                  // Deferred renderer
  float        _SSRIntensity          = 0.6f;                   // Deferred renderer
  float        _SSRMaxRoughness       = 0.55f;                  // Deferred renderer
  float        _SSRFade               = 0.18f;                  // Deferred renderer
  float        _SpecularIBLIntensity  = 0.5f;                   // Deferred renderer
  float        _DirectLightIntensity  = 1.0f;                   // Deferred renderer
  float        _DenoiserThreshold     = 0.05f;                  // PathTracer
  float        _DenoiserSigmaSpatial  = 2.0f;                   // PathTracer
  float        _DenoiserSigmaRange    = 0.1f;                   // PathTracer
  float        _DenoiserColorPhi      = 0.9f;                   // PathTracer
  float        _DenoiserNormalPhi     = 0.3f;                   // PathTracer
  float        _DenoiserPositionPhi   = 0.6f;                   // PathTracer
  unsigned int _DenoisingWaveletScale = 1;                      // PathTracer
  unsigned int _DenoisingMethod       = 0;                      // PathTracer. 0: Bilateral, 1: Wavelet, 2: Edge-aware
  unsigned int _NbThreads             = 1;                      // Raster

};

}

#endif /* _RenderSettings_ */
