#ifndef _Test1_
#define _Test1_

#include "BaseTest.h"

struct GLFWwindow;

namespace RTRT
{

class Test1 : public BaseTest
{
public:
  Test1( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test1();

  virtual int Run();

};

}

#endif /* _Test1_ */
