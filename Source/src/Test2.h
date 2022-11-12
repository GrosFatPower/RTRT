#ifndef _Test2_
#define _Test2_

#include "RenderSettings.h"
#include <vector>
#include <string>

struct GLFWwindow;

namespace RTRT
{

class Scene;

class Test2
{
public:
  Test2( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test2();

  int Run();

private:

  void InitializeSceneFile();

  GLFWwindow     * _MainWindow;

  Scene          * _Scene = nullptr;
  RenderSettings   _Settings;

  int _ScreenWitdh;
  int _ScreenHeight;
  std::vector<std::string> _SceneFiles;
};

}

#endif /* _Test2_ */
