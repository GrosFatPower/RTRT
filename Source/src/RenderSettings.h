#ifndef _RenderSettings_
#define _RenderSettings_

#include "MathUtil.h"

namespace RTRT
{

struct RenderSettings
{
  Vec2i _RenderResolution   = { 0, 0 };
  Vec2i _WindowResolution   = { 1920, 1080 };
  Vec2i _TileResolution     = { -1, -1 };
  Vec3  _BackgroundColor    = { 0.f, 0.f, 0.f };
  Vec3  _UniformLightCol    = { .3f, .3f, .3f };
  Vec2i _TextureSize        = { 2048, 2048 };
  bool  _ShowLights         = false;
  bool  _EnableBackGround   = true;
  bool  _EnableSkybox       = true;
  bool  _EnableUniformLight = true;
  bool  _AutoScale          = false;
  bool  _ToneMapping        = true;
  bool  _FXAA               = false;
  bool  _Accumulate         = true;
  bool  _TiledRendering     = false;
  int   _Bounces            = 1;
  int   _RenderScale        = 100;
  float _LowResRatio        = 0.1f;
  float _TargetFPS          = 60.f;
  float _Gamma              = 2.f;
  float _Exposure           = 1.5f;
  float _SkyBoxRotation     = 0.f;
};

}

#endif /* _RenderSettings_ */
