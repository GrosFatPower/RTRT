#ifndef _Test5_
#define _Test5_

#include "BaseTest.h"
#include "Scene.h"
#include "Camera.h"
#include "RenderSettings.h"
#include "Renderer.h"
#include "KeyInput.h"
#include "MouseInput.h"
#include "GLUtil.h"
#include "GL/glew.h"

#include <memory>
#include <filesystem> // C++17

struct GLFWwindow;

namespace RTRT
{

class Test5 : public BaseTest
{
public:

  Test5( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test5();

  int Run();

  static const char * GetTestHeader();

public:

  enum class RendererType
  {
    PathTracer = 0,
    SoftwareRasterizer,
    OpenGLRasterizer
  };

protected:

  // Callbacks

  static void KeyCallback( GLFWwindow * iWindow, const int iKey, const int iScancode, const int iAction, const int iMods );
  static void MouseButtonCallback( GLFWwindow * iWindow, const int iButton, const int iAction, const int iMods );
  static void MouseScrollCallback( GLFWwindow * iWindow, const double iOffsetX, const double iOffsetY );
  static void FramebufferSizeCallback( GLFWwindow* iWindow, const int iWidth, const int iHeight );

protected:

  int InitializeUI();
  int DrawUI();

  int ProcessInput();

  int InitializeScene();
  int InitializeRenderer();
  int UpdateScene();

  int InitializeSceneFiles();
  int InitializeBackgroundFiles();

  int UpdateCPUTime();

protected:

  std::unique_ptr<Scene>    _Scene;
  RenderSettings            _Settings;
  Camera                    _DefaultCam;

  RendererType              _RendererType = RendererType::PathTracer;
  std::unique_ptr<Renderer> _Renderer;
  bool                      _ReloadRenderer = false;

  KeyInput                  _KeyInput;
  MouseInput                _MouseInput;

  // Frame rate
  double                     _CPUTime   = 0.;
  double                     _DeltaTime = 0.;
  double                     _FrameRate = 0.;
  double                     _FrameTime = 0.;
  unsigned int               _NbRenderedFrames = 0;

  // Scene selection
  std::vector<std::string>   _SceneFiles;
  std::vector<const char*>   _SceneNames;
  std::vector<std::string>   _BackgroundFiles;
  std::vector<const char*>   _BackgroundNames;
  std::vector<std::string>   _MaterialNames;
  int                        _CurSceneId       = -1;
  int                        _CurBackgroundId  = -1;
  bool                       _ReloadScene      = false;
  bool                       _ReloadBackground = false;
  GLTexture                  _AlbedoTEX        = { 0, GL_TEXTURE_2D, 31, GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture                  _MetalRoughTEX    = { 0, GL_TEXTURE_2D, 31, GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture                  _NormalMapTEX     = { 0, GL_TEXTURE_2D, 31, GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture                  _EmissionMapTEX   = { 0, GL_TEXTURE_2D, 31, GL_RGBA32F, GL_RGBA, GL_FLOAT };

  bool                       _RenderToFile = false;
  std::filesystem::path      _CaptureOutputPath;
};

}

#endif /* _Test5_ */
