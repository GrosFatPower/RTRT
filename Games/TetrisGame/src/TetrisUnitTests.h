#ifndef _TetrisUnitTests_
#define _TetrisUnitTests_

namespace Tetris
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
  static bool TestAudioEngineSynthesis();
  static bool TestMatrixAddGarbageLines();
  static bool TestGarbageCalculationAndCancellation();
  static bool TestMultiplayerInputMapping();
  static bool TestNetSnapshotSerialization();
  static bool TestOnlineLoopbackSession();
};

}

#endif /* _TetrisUnitTests_ */
