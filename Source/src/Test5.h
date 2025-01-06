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
  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

protected:

  int InitializeUI();
  int DrawUI();

  int ProcessInput();

  int InitializeScene();
  int InitializeRenderer();

protected:

  std::unique_ptr<Scene>    _Scene;
  RenderSettings            _Settings;

  RendererType              _RendererType = RendererType::PathTracerRenderer;
  std::unique_ptr<Renderer> _Renderer;

  KeyInput   _KeyInput;
  MouseInput _MouseInput;
};

}

#endif /* _Test5_ */
