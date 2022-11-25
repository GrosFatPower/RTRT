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

  float RenderScale() const { return ( _Settings._RenderScale / 100.f ); }
  int RenderWidth()   const { return int( _Settings._RenderResolution.x * RenderScale() ); }
  int RenderHeight()  const { return int( _Settings._RenderResolution.y * RenderScale() ); }

  void RenderToTexture();
  void RenderToSceen();

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  GLFWwindow    * _MainWindow;

  QuadMesh      * _Quad      = nullptr;
  ShaderProgram * _RTTShader = nullptr;
  ShaderProgram * _RTSShader = nullptr;

  GLuint          _FrameBufferID   = 0;
  GLuint          _ScreenTextureID = 0;
  GLuint          _SkyboxTextureID = 0;

  long            _FrameNum          = 0;
  int             _AccumulatedFrames = 0;
  float           _CPULoopTime       = 0.f;
  float           _FrameRate         = 0.f;
  float           _TimeDelta         = 0.f;
  float           _AverageDelta      = 0.f;
  std::deque<float> _LastDeltas;

  double          _MouseX       = 0.;
  double          _MouseY       = 0.;
  double          _OldMouseX    = 0.;
  double          _OldMouseY    = 0.;
  bool            _LeftClick    = false;
  bool            _RightClick   = false;
  bool            _MiddleClick  = false;

  bool            _KeyUp       = false;
  bool            _KeyDown     = false;
  bool            _KeyLeft     = false;
  bool            _KeyRight    = false;

  Scene         * _Scene = nullptr;
  RenderSettings  _Settings;
  bool            _SceneCameraModified    = false;
  bool            _SceneLightsModified    = false;
  bool            _SceneMaterialsModified = false;
  bool            _SceneInstancesModified = false;
  bool            _RenderSettingsModified = false;
};

}

#endif /* _Test3_ */
