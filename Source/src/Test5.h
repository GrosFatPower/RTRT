#ifndef _Test5_
#define _Test5_

#include "BaseTest.h"
#include "Scene.h"
#include "RenderSettings.h"
#include "Renderer.h"
#include "KeyInput.h"
#include "MouseInput.h"
#include "GL/glew.h"

#include <memory>

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
    PathTracerRenderer,
    RasterizerRenderer
  };

protected:

  // Callbacks
  static void KeyCallback(GLFWwindow* window, const int key, const int scancode, const int action, const int mods);
  static void MouseButtonCallback(GLFWwindow * window, const int button, const int action, const int mods);
  static void MouseScrollCallback(GLFWwindow * window, const double xoffset, const double yoffset);
  static void FramebufferSizeCallback(GLFWwindow* window, const int width, const int height);

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

  RendererType              _RendererType = RendererType::PathTracerRenderer;
  std::unique_ptr<Renderer> _Renderer;

  KeyInput                  _KeyInput;
  MouseInput                _MouseInput;

  // Frame rate
  double                     _CPUTime   = 0.;
  double                     _DeltaTime = 0.;
  double                     _FrameRate = 0.;
  double                     _FrameTime = 0.;

  // Scene selection
  std::vector<std::string>   _SceneFiles;
  std::vector<const char*>   _SceneNames;
  std::vector<std::string>   _BackgroundFiles;
  std::vector<const char*>   _BackgroundNames;
  int                        _CurSceneId       = -1;
  int                        _CurBackgroundId  = -1;
  bool                       _ReloadScene      = false;
  bool                       _ReloadBackground = false;
};

}

#endif /* _Test5_ */
