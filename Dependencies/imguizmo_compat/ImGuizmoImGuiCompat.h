#ifndef _ImGuizmoImGuiCompat_
#define _ImGuizmoImGuiCompat_

#include "imgui.h"

#if IMGUI_VERSION_NUM >= 19280
#define AddPolyline(points, num_points, col, closed, thickness) AddPolyline(points, num_points, col, static_cast<float>(thickness), (closed) ? ImDrawFlags_Closed : 0)
#endif

namespace ImGui
{

inline void CaptureMouseFromApp( bool iWantCaptureMouse = true )
{
  SetNextFrameWantCaptureMouse(iWantCaptureMouse);
}

}

#endif /* _ImGuizmoImGuiCompat_ */
