#ifndef _Test1_
#define _Test1_

struct GLFWwindow;

namespace RTRT
{

class Test1
{
public:
  Test1( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test1();

  int Run();

private:

  GLFWwindow * _MainWindow;
};

}

#endif /* _Test1_ */
