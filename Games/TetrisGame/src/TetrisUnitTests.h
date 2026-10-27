#ifndef _TetrisUnitTests_
#define _TetrisUnitTests_

namespace RTRT
{

class TetrisUnitTests
{
public:
  static int RunAll();

protected:
  static bool Test7BagRandomizer();
  static bool TestSRSKicks();
  static bool TestTSpinDetection();
  static bool TestExtendedLockDelay();
  static bool TestScoringAndB2B();
};

}

#endif /* _TetrisUnitTests_ */
