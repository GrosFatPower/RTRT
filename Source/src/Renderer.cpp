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

// ----------------------------------------------------------------------------
// SetDebugMode
// ----------------------------------------------------------------------------
void Renderer::SetDebugMode( const int iDebugMode )
{
  _DebugMode = iDebugMode;
}

// ----------------------------------------------------------------------------
// GetRenderPassTimings
// ----------------------------------------------------------------------------
int Renderer::GetRenderPassTimings( std::vector<RenderPassTiming> & oTimings ) const
{
  oTimings.clear();
  return 0;
}

}
