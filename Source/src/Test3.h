#ifndef _Test3_
#define _Test3_

#include "RenderSettings.h"
#include <vector>
#include <string>

struct GLFWwindow;

namespace RTRT
{

class Test3
{
public:
  Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test3();

  int Run();

private:

  void InitializeSceneFile();

  GLFWwindow     * _MainWindow;
  int             _ScreenWitdh;
  int             _ScreenHeight;
};

}

#endif /* _Test3_ */
