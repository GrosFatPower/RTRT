#ifndef _RendererFactory_
#define _RendererFactory_

#include <memory>

namespace RTRT
{

class Renderer;
class Scene;
struct RenderSettings;

enum class RendererBackend
{
  PathTracer,
  SoftwareRasterizer,
  DeferredRenderer
};

std::unique_ptr<Renderer> CreateRenderer( RendererBackend iBackend, Scene & iScene, RenderSettings & iSettings );

}

#endif /* _RendererFactory_ */
