#include "Renderer.h"

#include "Scene.h"
#include "RenderSettings.h"

#include <algorithm>

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
// FlipImageVertically
// ----------------------------------------------------------------------------
void Renderer::FlipImageVertically( RenderImage & ioImage )
{
  const size_t rowSize = (size_t)ioImage._Width * 4u;
  for ( int y = 0; y < ( ioImage._Height / 2 ); ++y )
  {
    const size_t topRow = (size_t)y * rowSize;
    const size_t bottomRow = (size_t)(ioImage._Height - 1 - y) * rowSize;
    std::swap_ranges(ioImage._Pixels.begin() + topRow, ioImage._Pixels.begin() + topRow + rowSize, ioImage._Pixels.begin() + bottomRow);
  }
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
