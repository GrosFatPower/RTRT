#ifndef _Test3_
#define _Test3_

#include "RenderSettings.h"
#include <vector>
#include <string>
#include <deque>
#include "GL/glew.h"

struct GLFWwindow;

namespace RTRT
{

class QuadMesh;
class ShaderProgram;
class Scene;


class Test3
{
public:
  Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test3();

  int Run();

private:

  int InitializeScene();
  int UpdateScene();

  int InitializeUI();
  int DrawUI();

  int InitializeFrameBuffer();
  int RecompileShaders();
  int UpdateUniforms();

  int UpdateCPUTime();

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  GLFWwindow    * _MainWindow;
  int             _ScreenWidth;
  int             _ScreenHeight;

  QuadMesh      * _Quad      = nullptr;
  ShaderProgram * _RTTShader = nullptr;
  ShaderProgram * _RTSShader = nullptr;

  GLuint          _FrameBufferID   = 0;
  GLuint          _ScreenTextureID = 0;

  long            _Frame        = 0;
  float           _CPULoopTime  = 0.f;
  float           _FrameRate    = 0.f;
  float           _TimeDelta    = 0.f;
  float           _AverageDelta = 0.f;
  std::deque<float> _LastDeltas;

  double          _MouseX       = 0.;
  double          _MouseY       = 0.;
  double          _OldMouseX    = 0.;
  double          _OldMouseY    = 0.;
  bool            _LeftClick    = false;
  bool            _RightClick   = false;
  bool            _MiddleClick  = false;

  Scene         * _Scene = nullptr;
  RenderSettings  _Settings;
};

}

#endif /* _Test3_ */
