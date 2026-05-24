#include "RenderStatsUI.h"

#include "PathTracer.h"
#include "Renderer.h"
#include "RenderSettings.h"
#include "Scene.h"

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace RTRT
{

namespace RenderStatsUI
{

void DrawFrameRateGraph( RenderStatsUIState & ioState, double iFrameRate, double iDeltaTime, unsigned int iNbRenderedFrames )
{
  if ( ioState._FrameRateHistory.empty() )
  {
    ioState._FrameRateHistory.assign(120, 0.f);
    ioState._LastFrameRateIndex = 0;
    ioState._LastFrameRateFrame = iNbRenderedFrames;
    ioState._FrameRateHistory[ioState._LastFrameRateIndex] = (float)iFrameRate;
  }

  if ( ioState._LastFrameRateFrame != iNbRenderedFrames )
  {
    ioState._LastFrameRateFrame = iNbRenderedFrames;
    ioState._FrameRateAccumTime += iDeltaTime;
    while ( ioState._FrameRateAccumTime > ( 1. / 60. ) )
    {
      ioState._FrameRateAccumTime -= 0.1;
      ioState._MaxFrameRate = std::max(1.f, *std::max_element(ioState._FrameRateHistory.begin(), ioState._FrameRateHistory.end()));
      ioState._LastFrameRateIndex++;
      if ( ioState._LastFrameRateIndex >= 120 )
        ioState._LastFrameRateIndex = 0;
      ioState._FrameRateHistory[ioState._LastFrameRateIndex] = (float)iFrameRate;
    }
  }

  const int offset = ( ioState._LastFrameRateIndex >= 119 ) ? 0 : ioState._LastFrameRateIndex + 1;
  char overlay[32];
  std::snprintf(overlay, 32, "%.1f FPS", iFrameRate);
  ImGui::PlotLines("Frame rate", &ioState._FrameRateHistory[0], static_cast<int>(ioState._FrameRateHistory.size()), offset, overlay, -0.1f, ioState._MaxFrameRate, ImVec2(0.f, 80.f));
}

void DrawRenderOverview( const RenderSettings & iSettings, double iFrameTime, unsigned int iNbRenderedFrames )
{
  ImGui::Text("Window width : %d height : %d", iSettings._WindowResolution.x, iSettings._WindowResolution.y);
  ImGui::Text("Render width : %d height : %d", iSettings._RenderResolution.x, iSettings._RenderResolution.y);
  ImGui::Text("Render time           : %.3f ms/frame", iFrameTime * 1000.);
  ImGui::Text("Rendered frames       : %u", iNbRenderedFrames);
}

void DrawRenderPassTimings( Renderer * ioRenderer )
{
  if ( !ioRenderer )
    return;

  std::vector<RenderPassTiming> timings;
  ioRenderer -> GetRenderPassTimings(timings);
  if ( timings.empty() )
    return;

  double cpuTotal = 0.;
  double gpuTotal = 0.;
  ImGui::Separator();
  ImGui::Text("Render passes");

  for ( const RenderPassTiming & timing : timings )
  {
    if ( timing._Enabled )
    {
      ImGui::Text("%-24s : %.3f ms [%s]", timing._Name, timing._Seconds * 1000., timing._GPU ? "GPU" : "CPU");
      if ( timing._GPU )
        gpuTotal += timing._Seconds;
      else
        cpuTotal += timing._Seconds;
    }
    else
      ImGui::Text("%-24s : -- [%s]", timing._Name, timing._GPU ? "GPU" : "CPU");
  }

  ImGui::Text("CPU pass total        : %.3f ms", cpuTotal * 1000.);
  ImGui::Text("GPU pass total        : %.3f ms", gpuTotal * 1000.);
}

void DrawPathTracerStats( Renderer * ioRenderer )
{
  if ( !ioRenderer )
    return;

  PathTracer * pathTracer = ioRenderer -> AsPathTracer();
  if ( !pathTracer )
    return;

  ImGui::Separator();
  ImGui::Text("Frame number          : %d", pathTracer -> GetFrameNum());
  ImGui::Text("Nb complete frames    : %d", pathTracer -> GetNbCompleteFrames());
}

void DrawSceneStats( Scene * ioScene )
{
  if ( !ioScene )
    return;

  ImGui::Separator();
  ImGui::Text("Nb vertices           : %d", (int)ioScene -> GetVertices().size());
  ImGui::Text("Nb triangles          : %d", (int)ioScene -> GetIndices().size() / 3);
  ImGui::Text("Nb meshes instances   : %d", ioScene -> GetNbMeshInstances());
}

}

}
