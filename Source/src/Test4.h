#ifndef _Test4_
#define _Test4_

#include "RenderSettings.h"
#include "MathUtil.h"
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
  int InitializeUI();
  int InitializeFrameBuffer();
  int InitializeScene();

  int RecompileShaders();
  int UpdateUniforms();
  int UpdateImage();
  int UpdateTextures();
  int UpdateScene();

  void RenderToTexture();
  void RenderToSceen();
  void DrawUI();

  GLFWwindow    * _MainWindow;

  QuadMesh      * _Quad      = nullptr;
  ShaderProgram * _RTTShader = nullptr;
  ShaderProgram * _RTSShader = nullptr;

  GLuint          _FrameBufferID   = 0;
  GLuint          _ImageTextureID  = 0;
  GLuint          _ScreenTextureID = 0;

  RenderSettings  _Settings;

  bool              _UpdateImageTex = true;
  std::vector<Vec4> _Image;

  Scene         * _Scene = nullptr;

  // Frame rate
  float           _CPULoopTime          = 0.f;
  float           _FrameRate            = 0.f;
  float           _TimeDelta            = 0.f;
  float           _AverageDelta         = 0.f;
  std::deque<float> _LastDeltas;
};

}

#endif /* _Test4_ */
