#ifndef _BaseTest_
#define _BaseTest_

struct GLFWwindow;

namespace RTRT
{

class BaseTest
{
public:
  BaseTest( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
    : _MainWindow(iMainWindow), _ScreenWitdh(iScreenWidth), _ScreenHeight(iScreenHeight) {}

  virtual ~BaseTest() {}

  virtual int Run() = 0;

protected:

  GLFWwindow * _MainWindow;
  int          _ScreenWitdh;
  int          _ScreenHeight;
};

}

#endif /* _BaseTest_ */
