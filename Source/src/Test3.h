#ifndef _Test3_
#define _Test3_

#include "RenderSettings.h"
#include <vector>
#include <string>
#include <deque>

struct GLFWwindow;

namespace RTRT
{

class QuadMesh;
class ShaderProgram;

class Test3
{
public:
  Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test3();

  int Run();

private:

  int InitializeFrameBuffer();
  int RecompileShaders();
  int UpdateUniforms();
  int DrawUI();
  int UpdateCPUTime();

  GLFWwindow     * _MainWindow;
  int             _ScreenWidth;
  int             _ScreenHeight;

  QuadMesh      * _Quad      = nullptr;
  ShaderProgram * _RTTShader = nullptr;
  ShaderProgram * _RTSShader = nullptr;

  long            _Frame        = 0;
  float           _CPULoopTime  = 0.f;
  float           _FrameRate    = 0.f;
  float           _TimeDelta    = 0.f;
  float           _AverageDelta = 0.f;
  std::deque<float> _LastDeltas;
};

}

#endif /* _Test3_ */
