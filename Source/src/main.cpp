#include "Test1.h"
#include "Test2.h"

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main(int, char**)
{
  int failure = 0;

  if ( 1 )
  {
    RTRT::Test1 test1(1920, 1080);
    failure = test1.Run();
  }

  if ( 1 )
  {
    RTRT::Test2 test2(1920, 1080);
    failure |= test2.Run();
  }

  // Exit program
  return failure;
}
