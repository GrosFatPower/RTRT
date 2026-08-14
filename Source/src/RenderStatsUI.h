#ifndef _RenderStatsUI_
#define _RenderStatsUI_

#include <vector>

namespace RTRT
{

class Renderer;
class Scene;
struct RenderSettings;

struct RenderStatsUIState
{
  std::vector<float> _FrameRateHistory;
  int                _LastFrameRateIndex = -1;
  unsigned int       _LastFrameRateFrame = 0;
  double             _FrameRateAccumTime = 0.;
  float              _MaxFrameRate = 300.f;
};

namespace RenderStatsUI
{

void DrawFrameRateGraph( RenderStatsUIState & ioState, double iFrameRate, double iDeltaTime, unsigned int iNbRenderedFrames );
void DrawRenderOverview( const RenderSettings & iSettings, double iFrameTime, unsigned int iNbRenderedFrames );
void DrawRenderPassTimings( Renderer * ioRenderer );
void DrawPathTracerStats( Renderer * ioRenderer );
void DrawSceneStats( Scene * ioScene );

}

}

#endif /* _RenderStatsUI_ */
