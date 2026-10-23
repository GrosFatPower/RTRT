#ifndef _BaseTest_
#define _BaseTest_

#include <memory>

struct GLFWwindow;

namespace RTRT
{

class BaseTest
{
public:
  BaseTest( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
  : _MainWindow(iMainWindow), _ScreenWitdh(iScreenWidth), _ScreenHeight(iScreenHeight) {}

  virtual ~BaseTest() {}

  virtual int Run() = 0;

  static const char * GetTestHeader() { return ""; }

protected:

  std::shared_ptr<GLFWwindow> _MainWindow;
  int                         _ScreenWitdh;
  int                         _ScreenHeight;
};

}

#endif /* _BaseTest_ */
