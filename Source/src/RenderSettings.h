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
  bool  _EnableUniformLight = true;
};

}

#endif /* _RenderSettings_ */