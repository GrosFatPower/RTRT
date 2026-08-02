#include "RendererFactory.h"

#include "DeferredRenderer.h"
#include "PathTracer.h"
#include "SoftwareRasterizer.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// CreateRenderer
// ----------------------------------------------------------------------------
std::unique_ptr<Renderer> CreateRenderer( RendererBackend iBackend, Scene & iScene, RenderSettings & iSettings )
{
  if ( RendererBackend::PathTracer == iBackend )
    return std::unique_ptr<Renderer>(new PathTracer(iScene, iSettings));
  if ( RendererBackend::SoftwareRasterizer == iBackend )
    return std::unique_ptr<Renderer>(new SoftwareRasterizer(iScene, iSettings));
  if ( RendererBackend::DeferredRenderer == iBackend )
    return std::unique_ptr<Renderer>(new DeferredRenderer(iScene, iSettings));

  return nullptr;
}

}
