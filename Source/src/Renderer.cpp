#include "Renderer.h"

#include "Scene.h"
#include "RenderSettings.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Renderer::Renderer( Scene & iScene, RenderSettings & iSettings )
: _Scene(iScene)
, _Settings(iSettings)
{
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Renderer::~Renderer()
{
}

}