#ifndef _ImGuizmoImGuiCompat_
#define _ImGuizmoImGuiCompat_

#include "imgui.h"

namespace ImGui
{

inline void CaptureMouseFromApp( bool iWantCaptureMouse = true )
{
  SetNextFrameWantCaptureMouse(iWantCaptureMouse);
}

}

#endif /* _ImGuizmoImGuiCompat_ */
