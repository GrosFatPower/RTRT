#ifndef _Test2_
#define _Test2_

#include "BaseTest.h"
#include "RenderSettings.h"
#include <vector>
#include <string>

struct GLFWwindow;

namespace RTRT
{

class Scene;

class Test2 : public BaseTest
{
public:
  Test2( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test2();

  int Run();

  static const char * GetTestHeader();

private:

  void InitializeSceneFile();

  Scene          * _Scene = nullptr;
  RenderSettings   _Settings;

  std::vector<std::string> _SceneFiles;
};

}

#endif /* _Test2_ */
