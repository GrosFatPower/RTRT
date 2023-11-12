#ifndef _Test4_
#define _Test4_

#include "RenderSettings.h"
#include "MathUtil.h"
#include <vector>
#include <string>
#include <deque>
#include <memory>
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

  struct KeyState
  {
    bool _KeyUp    = false;
    bool _KeyDown  = false;
    bool _KeyLeft  = false;
    bool _KeyRight = false;
  };

  struct MouseState
  {
    double _MouseX      = 0.;
    double _MouseY      = 0.;
    double _OldMouseX   = 0.;
    double _OldMouseY   = 0.;
    bool   _LeftClick   = false;
    bool   _RightClick  = false;
    bool   _MiddleClick = false;
  };

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

  GLFWwindow       * _MainWindow;

  std::unique_ptr<QuadMesh> _Quad;
  std::unique_ptr<Scene>    _Scene;

  std::unique_ptr<ShaderProgram> _RTTShader;
  std::unique_ptr<ShaderProgram> _RTSShader;

  GLuint             _FrameBufferID   = 0;
  GLuint             _ImageTextureID  = 0;
  GLuint             _ScreenTextureID = 0;

  RenderSettings     _Settings;

  bool               _UpdateImageTex = true;
  std::vector<Vec4>  _Image;

  KeyState          _KeyState;
  MouseState        _MouseState;

  // Frame rate
  float             _CPULoopTime          = 0.f;
  float             _FrameRate            = 0.f;
  float             _TimeDelta            = 0.f;
  float             _AverageDelta         = 0.f;
  std::deque<float> _LastDeltas;
};

}

#endif /* _Test4_ */
