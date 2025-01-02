#ifndef _Test5_
#define _Test5_

#include "BaseTest.h"
#include "GL/glew.h"

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

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
};

}

#endif /* _Test5_ */
