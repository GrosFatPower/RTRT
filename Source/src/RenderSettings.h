#ifndef _RenderSettings_
#define _RenderSettings_

#include "Math.h"

namespace RTRT
{

struct RenderSettings
{
  Vec2i _RenderResolution   = { 0, 0 };
  Vec2i _WindowResolution   = { 1920, 1080 };
  Vec3  _BackgroundColor    = { 0.f, 0.f, 0.f };
  Vec3  _UniformLightCol    = { .3f, .3f, .3f };
  bool  _EnableBackGround   = true;
  bool  _EnableSkybox       = true;
  bool  _EnableUniformLight = true;
  int   _Bounces            = 1;
  int   _RenderScale        = 100;
  float _Gamma              = 2.f;
  float _SkyBoxRotation     = 0.f;
};

}

#endif /* _RenderSettings_ */
