#include "Test1.h"
#include "Test2.h"

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main(int, char**)
{
  //RTRT::Test1 test1(1920, 1080);
  //int failure = test1.Run();

  RTRT::Test2 test2(1920, 1080);
  int failure = test2.Run();

  // Exit program
  return failure;
}
