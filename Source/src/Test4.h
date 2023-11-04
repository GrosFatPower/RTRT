#ifndef _Test4_
#define _Test4_

#include "RenderSettings.h"
#include <vector>
#include <string>
#include <deque>

struct GLFWwindow;

namespace RTRT
{

class Test4
{
public:
  Test4( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test4();

  int Run();

private:

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  int UpdateCPUTime();

  GLFWwindow    * _MainWindow;

  RenderSettings   _Settings;

  long            _FrameNum             = 0;
  int             _AccumulatedFrames    = 0;
  float           _CPULoopTime          = 0.f;
  float           _FrameRate            = 0.f;
  float           _TimeDelta            = 0.f;
  float           _AverageDelta         = 0.f;
  std::deque<float> _LastDeltas;
};

}

#endif /* _Test4_ */
