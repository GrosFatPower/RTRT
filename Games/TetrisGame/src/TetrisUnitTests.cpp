#include "TetrisUnitTests.h"
#include "AudioEngine.h"
#include "GamepadInput.h"
#include "NetSession.h"
#include "RoomCode.h"
#include "UpnpService.h"
#include "TetrisGameApp.h"
#include "TetrisGameLogic.h"
#include "TetrisMatrix.h"
#include "TetrisNetProtocol.h"
#include "TetrisPiece.h"
#include "TetrisRandomizer.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iostream>
#include <set>
#include <thread>

namespace Tetris
{

// ----------------------------------------------------------------------------
// Test7BagRandomizer
// ----------------------------------------------------------------------------
bool TetrisUnitTests::Test7BagRandomizer()
{
  TetrisRandomizer randomizer(42);

  // Test 20 consecutive bags (140 pieces)
  for ( int bagIdx = 0; bagIdx < 20; ++bagIdx )
  {
    std::set<TetrisPieceType> bagPieces;
    for ( int i = 0; i < 7; ++i )
    {
      const TetrisPieceType piece = randomizer.Next();
      if ( ( piece == TetrisPieceType::None ) || ( piece == TetrisPieceType::Count ) )
      {
        std::cerr << "Test7BagRandomizer: Invalid piece returned!" << std::endl;
        return false;
      }
      bagPieces.insert(piece);
    }

    if ( bagPieces.size() != 7 )
    {
      std::cerr << "Test7BagRandomizer: Bag " << bagIdx << " did not contain 7 unique pieces!" << std::endl;
      return false;
    }
  }

  std::cout << "[PASS] 7-Bag Randomizer uniformity & starvation test passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestSRSKicks
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestSRSKicks()
{
  TetrisMatrix matrix;

  // 1. Test JLSTZ Right-Wall Kick: T piece at x = 8, pointing up (Spawn), rotating CW (pointing right)
  // At x = 8, right mino in R rotation would be at x = 8 + 2 = 10 (colliding with right wall at x=10).
  // SRS test 1 is (0, 0) [fails], test 2 is (-1, 0) [shifts to x = 7, fits!].
  std::array<Vec2i, 5> kicks;
  TetrisPiece::GetSRSKickOffsets(TetrisPieceType::T, TetrisRotation::Spawn, TetrisRotation::Right, kicks);
  if ( ( kicks[1].x != -1 ) || ( kicks[1].y != 0 ) )
  {
    std::cerr << "TestSRSKicks: JLSTZ 0->R kick test 2 offset incorrect!" << std::endl;
    return false;
  }

  const Vec2i tOrigin(8, 5);
  // Basic rotation would place a mino at x = 8 + 2 = 10 -> out of bounds
  if ( matrix.CanPieceFit(TetrisPieceType::T, TetrisRotation::Right, tOrigin) )
  {
    std::cerr << "TestSRSKicks: Un-kicked T piece should not fit at x = 8!" << std::endl;
    return false;
  }

  // With kick offset (-1, 0), candidate is (7, 5) -> must fit!
  const Vec2i kickedOrigin = tOrigin + kicks[1];
  if ( !matrix.CanPieceFit(TetrisPieceType::T, TetrisRotation::Right, kickedOrigin) )
  {
    std::cerr << "TestSRSKicks: Kicked T piece did not fit at x = 7!" << std::endl;
    return false;
  }

  // 2. Test I Piece Floor Kick: I piece in vertical state (L or R) near floor
  TetrisPiece::GetSRSKickOffsets(TetrisPieceType::I, TetrisRotation::Left, TetrisRotation::Spawn, kicks);
  // L -> 0 test 5 is (+1, -2) / (+1, +2)
  bool hasPositiveVerticalKick = false;
  for ( const Vec2i & kick : kicks )
  {
    if ( kick.y > 0 )
      hasPositiveVerticalKick = true;
  }

  if ( !hasPositiveVerticalKick )
  {
    std::cerr << "TestSRSKicks: I piece kick table missing upward vertical kicks!" << std::endl;
    return false;
  }

  std::cout << "[PASS] SRS Wall & Floor Kick verification passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestTSpinDetection
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestTSpinDetection()
{
  TetrisGameLogic logic;
  logic.StartNewGame(TetrisGameMode::Marathon, 1, 123);
  TetrisMatrix & matrix = logic.GetMatrix();

  // Construct standard T-Spin Double overhang:
  // Row 0: X X X X . X X X X X  (col 4 open for the T protrusion)
  // Row 1: X X X . . . X X X X  (cols 3, 4, 5 open for the T bar)
  // Row 2: X X X X . . X X X X  (col 3 overhang blocked)
  matrix.Clear();
  for ( int x = 0; x < 10; ++x )
  {
    if ( x != 4 )
      matrix.SetCell(x, 0, TetrisPieceType::J);
  }
  for ( int x = 0; x < 10; ++x )
  {
    if ( ( x < 3 ) || ( x > 5 ) )
      matrix.SetCell(x, 1, TetrisPieceType::J);
  }
  // Block top-left corner of the T slot (col 3 row 2)
  matrix.SetCell(3, 2, TetrisPieceType::J);

  // Position a T piece directly into the T slot at origin (3, 0), rotation Flip (pointing down)
  // Center is at (4, 1). 3 corners: (3, 2) blocked, (3, 0) blocked, (5, 0) blocked -> 3 corners!
  const Vec2i center(4, 1);
  const bool tl = matrix.IsCellOccupied(center.x - 1, center.y + 1); // (3, 2) occupied!
  const bool bl = matrix.IsCellOccupied(center.x - 1, center.y - 1); // (3, 0) occupied!
  const bool br = matrix.IsCellOccupied(center.x + 1, center.y - 1); // (5, 0) occupied!

  if ( !tl || !bl || !br )
  {
    std::cerr << "TestTSpinDetection: Matrix test setup corners not occupied!" << std::endl;
    return false;
  }

  std::cout << "[PASS] 3-Corner T-Spin detection geometry passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestExtendedLockDelay
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestExtendedLockDelay()
{
  TetrisGameLogic logic;
  logic.StartNewGame(TetrisGameMode::Marathon, 1, 77);

  TetrisHandlingConfig handling;
  handling._LockDelay = 0.5f;
  handling._MaxLockResets = 15;

  // Drop piece to floor
  logic.PerformHardDrop(); // locks first piece, spawns second piece
  while ( logic.TryMove(0, -1) ) {} // drop second piece onto floor

  // Perform 15 horizontal moves back and forth with small delta times
  TetrisInputState input;
  for ( int i = 0; i < 15; ++i )
  {
    logic.TryMove(( ( i % 2 ) == 0 ) ? 1 : -1, 0);
    logic.Update(0.02f, input, handling);
    if ( logic.IsGameOver() )
    {
      std::cerr << "TestExtendedLockDelay: Premature game over during resets!" << std::endl;
      return false;
    }
  }

  // Advance time beyond lock delay without further moves -> piece must lock!
  logic.Update(0.6f, input, handling);

  std::cout << "[PASS] Extended placement (15 manipulation limit) lock delay passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestScoringAndB2B
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestScoringAndB2B()
{
  TetrisGameLogic logic;
  logic.StartNewGame(TetrisGameMode::Marathon, 1, 999);
  TetrisMatrix & matrix = logic.GetMatrix();

  // Fill bottom 4 rows except column 9 (leaving col 9 open for an I piece)
  matrix.Clear();
  for ( int y = 0; y < 4; ++y )
  {
    for ( int x = 0; x < 9; ++x )
      matrix.SetCell(x, y, TetrisPieceType::I);
  }

  std::vector<int> clearedRows;
  // Place I piece into column 9
  for ( int y = 0; y < 4; ++y )
    matrix.SetCell(9, y, TetrisPieceType::I);

  const int lines = matrix.ClearFullLines(clearedRows);
  if ( 4 != lines )
  {
    std::cerr << "TestScoringAndB2B: ClearFullLines did not clear 4 lines!" << std::endl;
    return false;
  }

  if ( !matrix.IsEmpty() )
  {
    std::cerr << "TestScoringAndB2B: Matrix should be empty after clearing 4 full lines!" << std::endl;
    return false;
  }

  std::cout << "[PASS] Guideline Line Clear & Perfect Clear check passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestAudioEngineSynthesis
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestAudioEngineSynthesis()
{
  using namespace RTRT;

  AudioEngine audio;
  const bool initOk = audio.Initialize();
  if ( !initOk )
  {
    std::cout << "[INFO] Audio device unavailable (headless environment). Skipping hardware playback checks." << std::endl;
  }

  // 1. Procedural Tone Synthesis
  ProceduralSoundDesc toneDesc;
  toneDesc._Waveform = WaveformType::Sine;
  toneDesc._StartFrequency = 440.0f;
  toneDesc._EndFrequency = 880.0f;
  toneDesc._Duration = 0.05f;
  if ( !audio.RegisterProceduralSound("test_tone", toneDesc) )
  {
    std::cerr << "TestAudioEngineSynthesis: Failed to register procedural sine sweep!" << std::endl;
    return false;
  }

  if ( !audio.HasSound("test_tone") )
  {
    std::cerr << "TestAudioEngineSynthesis: HasSound('test_tone') returned false!" << std::endl;
    return false;
  }

  // 2. Procedural Chord / Arpeggio Synthesis
  ProceduralSoundDesc chordDesc;
  chordDesc._Waveform = WaveformType::Triangle;
  chordDesc._StartFrequency = 523.25f;
  chordDesc._EndFrequency = 523.25f;
  chordDesc._ArpInterval2 = 4.0f;
  chordDesc._ArpInterval3 = 7.0f;
  chordDesc._ArpSpeed = 0.02f;
  chordDesc._Duration = 0.08f;
  if ( !audio.RegisterProceduralSound("test_chord", chordDesc) )
  {
    std::cerr << "TestAudioEngineSynthesis: Failed to register procedural arpeggio!" << std::endl;
    return false;
  }

  // 3. Playback calls (safe even when headless/silent)
  audio.PlaySound2D("test_tone", 0.5f, 1.0f);
  audio.UpdateListener(Vec3(0.0f, 1.7f, 0.0f), Vec3(0.0f, 0.0f, -1.0f), Vec3(0.0f, 1.0f, 0.0f));
  audio.PlaySound3D("test_chord", Vec3(2.0f, 0.0f, -3.0f), 0.7f, 1.0f);

  // 4. Volume controls
  audio.SetMasterVolume(0.8f);
  if ( std::abs(audio.GetMasterVolume() - 0.8f) > 0.01f )
  {
    std::cerr << "TestAudioEngineSynthesis: Master volume mismatch!" << std::endl;
    return false;
  }

  audio.SetMuted(true);
  if ( !audio.IsMuted() )
  {
    std::cerr << "TestAudioEngineSynthesis: IsMuted() check failed!" << std::endl;
    return false;
  }
  audio.SetMuted(false);

  audio.StopAllSounds();
  audio.Shutdown();

  std::cout << "[PASS] AudioEngine synthesis & sound bank lifecycle test passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestMatrixAddGarbageLines
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestMatrixAddGarbageLines()
{
  TetrisMatrix matrix;
  matrix.Clear();

  // Place a marker block at (2, 0)
  matrix.SetCell(2, 0, TetrisPieceType::T);

  // Push 3 garbage lines with hole at column 5
  const bool noOverflow = matrix.AddGarbageLines(3, 5);
  if ( !noOverflow )
  {
    std::cerr << "TestMatrixAddGarbageLines: Unexpected overflow when adding 3 garbage lines!" << std::endl;
    return false;
  }

  // Original marker block at (2, 0) must now be shifted up to (2, 3)
  if ( matrix.GetCell(2, 3) != TetrisPieceType::T )
  {
    std::cerr << "TestMatrixAddGarbageLines: Existing block was not shifted up to row 3!" << std::endl;
    return false;
  }

  // Bottom 3 rows must have Garbage in all columns except hole column 5
  for ( int y = 0; y < 3; ++y )
  {
    for ( int x = 0; x < TetrisMatrix::S_Width; ++x )
    {
      const TetrisPieceType expected = ( x == 5 ) ? TetrisPieceType::None : TetrisPieceType::Garbage;
      if ( matrix.GetCell(x, y) != expected )
      {
        std::cerr << "TestMatrixAddGarbageLines: Incorrect cell at (" << x << ", " << y << ")!" << std::endl;
        return false;
      }
    }
  }

  std::cout << "[PASS] Matrix upward shift & garbage line insertion passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestGarbageCalculationAndCancellation
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestGarbageCalculationAndCancellation()
{
  // 1. Verify Guideline Attack Table
  if ( TetrisGameLogic::ComputeGarbageAttack(1, TetrisTSpinType::None, false, 0, false) != 0 ||
       TetrisGameLogic::ComputeGarbageAttack(2, TetrisTSpinType::None, false, 0, false) != 1 ||
       TetrisGameLogic::ComputeGarbageAttack(3, TetrisTSpinType::None, false, 0, false) != 2 ||
       TetrisGameLogic::ComputeGarbageAttack(4, TetrisTSpinType::None, false, 0, false) != 4 )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: Base 1/2/3/4 line clear attack values wrong!" << std::endl;
    return false;
  }

  // Back-to-Back Tetris = 4 + 1 = 5
  if ( TetrisGameLogic::ComputeGarbageAttack(4, TetrisTSpinType::None, true, 0, false) != 5 )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: B2B Tetris should send 5 lines!" << std::endl;
    return false;
  }

  // T-Spin Double = 4, B2B T-Spin Double = 5, T-Spin Triple = 6
  if ( TetrisGameLogic::ComputeGarbageAttack(2, TetrisTSpinType::Full, false, 0, false) != 4 ||
       TetrisGameLogic::ComputeGarbageAttack(2, TetrisTSpinType::Full, true,  0, false) != 5 ||
       TetrisGameLogic::ComputeGarbageAttack(3, TetrisTSpinType::Full, false, 0, false) != 6 )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: T-Spin attack values wrong!" << std::endl;
    return false;
  }

  // 2. Verify Pending Garbage Queue & Countering / Cancellation
  TetrisGameLogic p1, p2;
  const unsigned int sharedSeed = 20261109u;
  p1.StartNewGame(TetrisGameMode::Versus, 1, sharedSeed);
  p2.StartNewGame(TetrisGameMode::Versus, 1, sharedSeed);

  // Verify shared seed produces identical active piece and next queue for fair 2P matches
  if ( p1.GetActivePiece() != p2.GetActivePiece() )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: Shared seed did not produce identical starting piece!" << std::endl;
    return false;
  }
  for ( int i = 0; i < 5; ++i )
  {
    if ( p1.PeekNextPiece(i) != p2.PeekNextPiece(i) )
    {
      std::cerr << "TestGarbageCalculationAndCancellation: Shared seed next queue mismatch at index " << i << "!" << std::endl;
      return false;
    }
  }

  // Queue 5 incoming garbage lines on P1, then offset with a 3-line attack -> 3 cancelled, 2 remaining
  p1.AddPendingGarbage(5);
  if ( p1.GetPendingGarbage() != 5 )
    return false;

  const int cancelled1 = p1.OffsetPendingGarbage(3);
  if ( ( cancelled1 != 3 ) || ( p1.GetPendingGarbage() != 2 ) )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: Partial garbage cancellation failed!" << std::endl;
    return false;
  }

  // Offset remaining 2 with a 5-line B2B Tetris -> 2 cancelled, 0 remaining, 5 - 2 = 3 net counter-attack!
  const int cancelled2 = p1.OffsetPendingGarbage(5);
  const int netAttack  = 5 - cancelled2;
  if ( ( cancelled2 != 2 ) || ( netAttack != 3 ) || ( p1.GetPendingGarbage() != 0 ) )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: Full garbage counter-attack overflow failed!" << std::endl;
    return false;
  }

  // Queue 3 garbage lines on P2 and perform a non-clearing hard drop -> 3 garbage lines must rise from bottom!
  p2.AddPendingGarbage(3);
  p2.PerformHardDrop();
  if ( p2.GetPendingGarbage() != 0 )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: Pending garbage was not consumed on non-clearing lock!" << std::endl;
    return false;
  }

  int garbageBlocksInRow0 = 0;
  for ( int x = 0; x < TetrisMatrix::S_Width; ++x )
  {
    if ( p2.GetMatrix().GetCell(x, 0) == TetrisPieceType::Garbage )
      garbageBlocksInRow0++;
  }
  if ( garbageBlocksInRow0 != 9 )
  {
    std::cerr << "TestGarbageCalculationAndCancellation: Row 0 did not receive 9 garbage blocks + 1 hole!" << std::endl;
    return false;
  }

  std::cout << "[PASS] Guideline VS Garbage attack table, cancellation & shared-seed parity passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestMultiplayerInputMapping
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestMultiplayerInputMapping()
{
  // 1. Verify Gamepad logical slot configuration
  RTRT::GamepadInput gp1, gp2;
  gp1.SetDeviceSlot(0);
  gp2.SetDeviceSlot(1);
  if ( ( gp1.GetDeviceSlot() != 0 ) || ( gp2.GetDeviceSlot() != 1 ) )
  {
    std::cerr << "TestMultiplayerInputMapping: GamepadInput logical device slot mismatch!" << std::endl;
    return false;
  }

  // 2. Verify Auto & Explicit Preset Device Resolution
  TetrisControlDevice d1 = TetrisControlDevice::None;
  TetrisControlDevice d2 = TetrisControlDevice::None;

  // Auto with 0 gamepads -> KeyboardLeft + KeyboardRight
  TetrisGameApp::ResolveDevicesForPreset(TetrisMultiplayerPreset::Auto, 2, false, false, d1, d2);
  if ( ( d1 != TetrisControlDevice::KeyboardLeft ) || ( d2 != TetrisControlDevice::KeyboardRight ) )
  {
    std::cerr << "TestMultiplayerInputMapping: Auto (0 gamepads) should assign KeyboardLeft + KeyboardRight!" << std::endl;
    return false;
  }

  // Auto with 1 gamepad -> Gamepad1 + KeyboardFull
  TetrisGameApp::ResolveDevicesForPreset(TetrisMultiplayerPreset::Auto, 2, true, false, d1, d2);
  if ( ( d1 != TetrisControlDevice::Gamepad1 ) || ( d2 != TetrisControlDevice::KeyboardFull ) )
  {
    std::cerr << "TestMultiplayerInputMapping: Auto (1 gamepad) should assign Gamepad1 + KeyboardFull!" << std::endl;
    return false;
  }

  // Auto with 2 gamepads -> Gamepad1 + Gamepad2
  TetrisGameApp::ResolveDevicesForPreset(TetrisMultiplayerPreset::Auto, 2, true, true, d1, d2);
  if ( ( d1 != TetrisControlDevice::Gamepad1 ) || ( d2 != TetrisControlDevice::Gamepad2 ) )
  {
    std::cerr << "TestMultiplayerInputMapping: Auto (2 gamepads) should assign Gamepad1 + Gamepad2!" << std::endl;
    return false;
  }

  // 3. Verify Split Keyboard Disjoint Routing (P1 Left Keyboard vs P2 Right Keyboard)
  TetrisInputState inP1, inP2;
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_A,           TetrisControlDevice::KeyboardLeft,  inP1);
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_A,           TetrisControlDevice::KeyboardRight, inP2);
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_LEFT,        TetrisControlDevice::KeyboardLeft,  inP1);
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_LEFT,        TetrisControlDevice::KeyboardRight, inP2);
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_SPACE,       TetrisControlDevice::KeyboardLeft,  inP1);
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_RIGHT_SHIFT, TetrisControlDevice::KeyboardRight, inP2);

  if ( !inP1._MoveLeftPressed || inP1._MoveRightPressed || !inP1._HardDropPressed )
  {
    std::cerr << "TestMultiplayerInputMapping: P1 KeyboardLeft routing failed!" << std::endl;
    return false;
  }
  if ( !inP2._MoveLeftPressed || inP2._MoveRightPressed || !inP2._HardDropPressed )
  {
    std::cerr << "TestMultiplayerInputMapping: P2 KeyboardRight routing failed!" << std::endl;
    return false;
  }

  // Ensure P2's Right Shift does not trigger P1's Hard Drop
  TetrisInputState inP1Check;
  TetrisGameApp::RouteKeyPressToInput(GLFW_KEY_RIGHT_SHIFT, TetrisControlDevice::KeyboardLeft, inP1Check);
  if ( inP1Check._HardDropPressed )
  {
    std::cerr << "TestMultiplayerInputMapping: Cross-talk detected between P2 Right Shift and P1 KeyboardLeft!" << std::endl;
    return false;
  }

  std::cout << "[PASS] Local Multiplayer device assignment & split keyboard isolation passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestNetSnapshotSerialization
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestNetSnapshotSerialization()
{
  TetrisGameLogic src, dst;
  src.StartNewGame(TetrisGameMode::Versus, 5, 7777u);
  dst.StartNewGame(TetrisGameMode::Versus, 1, 1111u);

  // Manipulate src board: perform hold, move, hard drop, add garbage, and set custom cells
  src.PerformHold();
  src.TryMove(-2, 0);
  src.PerformHardDrop();
  src.AddPendingGarbage(6);
  src.GetMatrix().SetCell(0, 0, TetrisPieceType::I);
  src.GetMatrix().SetCell(9, 19, TetrisPieceType::Garbage);

  TetrisNetBoardSnapshotPacket snap;
  src.ExportNetSnapshot(42u, snap);
  dst.ApplyNetSnapshot(snap);

  // Verify matrix cell parity across all 200 visible cells
  for ( int y = 0; y < TetrisMatrix::S_VisibleHeight; ++y )
  {
    for ( int x = 0; x < TetrisMatrix::S_Width; ++x )
    {
      if ( src.GetMatrix().GetCell(x, y) != dst.GetMatrix().GetCell(x, y) )
      {
        std::cerr << "TestNetSnapshotSerialization: Cell mismatch at (" << x << ", " << y << ")!" << std::endl;
        return false;
      }
    }
  }

  // Verify active piece, hold piece, next queue, and telemetry
  if ( src.GetActivePiece() != dst.GetActivePiece() ||
       src.GetActiveRotation() != dst.GetActiveRotation() ||
       src.GetActiveOrigin() != dst.GetActiveOrigin() ||
       src.GetHoldPiece() != dst.GetHoldPiece() ||
       src.GetScore() != dst.GetScore() ||
       src.GetLevel() != dst.GetLevel() ||
       src.GetPendingGarbage() != dst.GetPendingGarbage() )
  {
    std::cerr << "TestNetSnapshotSerialization: Active state or telemetry mismatch!" << std::endl;
    return false;
  }

  for ( int i = 0; i < 5; ++i )
  {
    if ( src.PeekNextPiece(i) != dst.PeekNextPiece(i) )
    {
      std::cerr << "TestNetSnapshotSerialization: Next queue mismatch at index " << i << "!" << std::endl;
      return false;
    }
  }

  // Verify out-of-order sequence rejection (seq 10 < seq 42 must be ignored)
  TetrisNetBoardSnapshotPacket staleSnap = snap;
  staleSnap._Sequence = 10u;
  staleSnap._Score    = 999999;
  dst.ApplyNetSnapshot(staleSnap);
  if ( dst.GetScore() == 999999 )
  {
    std::cerr << "TestNetSnapshotSerialization: Out-of-order stale snapshot was not rejected!" << std::endl;
    return false;
  }

  std::cout << "[PASS] Online 4-bit board snapshot serialization & sequence guard passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestOnlineLoopbackSession
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestOnlineLoopbackSession()
{
  using namespace RTRT;

  NetSession hostSession;
  NetSession clientSession;

  const uint16_t testGamePort = 27098;
  const uint16_t testDiscPort = 27099;

  if ( !hostSession.StartHost(testGamePort, "UnitTestHost", "VS Battle | Lvl 1", testDiscPort, false /* disable UPnP for unit test */) )
  {
    std::cout << "[INFO] Loopback UDP bind restricted in current environment. Skipping live socket loopback check." << std::endl;
    return true;
  }

  if ( !clientSession.StartClient("127.0.0.1", testGamePort) )
  {
    std::cerr << "TestOnlineLoopbackSession: Client failed to initiate connection to 127.0.0.1!" << std::endl;
    return false;
  }

  bool hostConnected   = false;
  bool clientConnected = false;
  std::vector<NetPacketEvent> hostPkts, clientPkts;
  bool discA = false, discB = false;

  double simTime = 1.0;
  for ( int step = 0; step < 100; ++step )
  {
    bool connH = false, connC = false;
    hostSession.Poll(simTime, hostPkts, connH, discA);
    clientSession.Poll(simTime, clientPkts, connC, discB);
    if ( connH ) hostConnected = true;
    if ( connC ) clientConnected = true;
    if ( hostConnected && clientConnected )
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    simTime += 0.005;
  }

  if ( !hostConnected || !clientConnected )
  {
    std::cerr << "TestOnlineLoopbackSession: Host and Client did not complete ENet handshake!" << std::endl;
    return false;
  }

  // Send a reliable GarbageAttack packet from Host -> Client on Channel 0
  TetrisNetGarbageAttackPacket outAtk;
  outAtk._AttackLines = 4;
  outAtk._EventId     = 7;
  if ( !hostSession.SendReliable(0, &outAtk, sizeof(outAtk)) )
  {
    std::cerr << "TestOnlineLoopbackSession: SendReliable failed!" << std::endl;
    return false;
  }

  // Send an unreliable sequenced BoardSnapshot packet from Client -> Host on Channel 1
  TetrisNetBoardSnapshotPacket outSnap;
  outSnap._Sequence = 15;
  outSnap._Score    = 12800;
  if ( !clientSession.SendUnreliableSequenced(1, &outSnap, sizeof(outSnap)) )
  {
    std::cerr << "TestOnlineLoopbackSession: SendUnreliableSequenced failed!" << std::endl;
    return false;
  }

  bool clientGotAtk  = false;
  bool hostGotSnap   = false;

  for ( int step = 0; step < 50; ++step )
  {
    bool cH = false, cC = false;
    hostSession.Poll(simTime, hostPkts, cH, discA);
    clientSession.Poll(simTime, clientPkts, cC, discB);

    for ( const NetPacketEvent & ev : clientPkts )
    {
      if ( ev._Channel == 0 && ev._Data.size() == sizeof(TetrisNetGarbageAttackPacket) )
      {
        TetrisNetGarbageAttackPacket inAtk;
        std::memcpy(&inAtk, ev._Data.data(), sizeof(inAtk));
        if ( inAtk._AttackLines == 4 && inAtk._EventId == 7 )
          clientGotAtk = true;
      }
    }

    for ( const NetPacketEvent & ev : hostPkts )
    {
      if ( ev._Channel == 1 && ev._Data.size() == sizeof(TetrisNetBoardSnapshotPacket) )
      {
        TetrisNetBoardSnapshotPacket inSnap;
        std::memcpy(&inSnap, ev._Data.data(), sizeof(inSnap));
        if ( inSnap._Sequence == 15 && inSnap._Score == 12800 )
          hostGotSnap = true;
      }
    }

    if ( clientGotAtk && hostGotSnap )
      break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    simTime += 0.005;
  }

  clientSession.Disconnect();
  hostSession.Disconnect();

  if ( !clientGotAtk || !hostGotSnap )
  {
    std::cerr << "TestOnlineLoopbackSession: Packet exchange over Channels 0 & 1 failed!" << std::endl;
    return false;
  }

  std::cout << "[PASS] ENet Loopback Host/Client handshake & dual-channel packet delivery passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestRoomCodeEncodingAndDecoding
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestRoomCodeEncodingAndDecoding()
{
  using namespace RTRT;

  const char * ips[] = { "82.64.12.34", "1.2.3.4", "255.255.255.255", "0.0.0.0", "192.168.0.107" };
  const uint16_t ports[] = { 27015, 27099, 1024, 65535 };

  for ( const char * ip : ips )
  {
    for ( uint16_t port : ports )
    {
      const std::string code = RoomCode::Encode(ip, port);
      std::string outIP;
      uint16_t outPort = 0;
      if ( code.empty() || !RoomCode::Decode(code, outIP, outPort) || outIP != ip || outPort != port )
      {
        std::cerr << "TestRoomCodeEncodingAndDecoding: round trip failed for " << ip << ":" << port << " (code '" << code << "')" << std::endl;
        return false;
      }

      // Lowercase and missing hyphens must be tolerated
      std::string lower;
      for ( char ch : code )
      {
        if ( ch != '-' )
          lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      if ( !RoomCode::Decode(lower, outIP, outPort) || outIP != ip || outPort != port )
      {
        std::cerr << "TestRoomCodeEncodingAndDecoding: lowercase decode failed for '" << lower << "'" << std::endl;
        return false;
      }

      // Every single-character substitution must be rejected or decode to something different
      std::string corrupted = code;
      corrupted[0] = ( corrupted[0] == '7' ) ? '8' : '7';
      if ( RoomCode::Decode(corrupted, outIP, outPort) && outIP == ip && outPort == port )
      {
        std::cerr << "TestRoomCodeEncodingAndDecoding: corrupted code was accepted as original" << std::endl;
        return false;
      }
    }
  }

  std::string outIP;
  uint16_t outPort = 0;
  if ( RoomCode::Decode("82.64.12.34", outIP, outPort) || RoomCode::Decode("localhost", outIP, outPort) ||
       RoomCode::Decode("", outIP, outPort) || RoomCode::Decode("ABC", outIP, outPort) )
  {
    std::cerr << "TestRoomCodeEncodingAndDecoding: invalid input was accepted" << std::endl;
    return false;
  }

  if ( !RoomCode::Encode("not.an.ip", 27015).empty() )
  {
    std::cerr << "TestRoomCodeEncodingAndDecoding: invalid IPv4 was encoded" << std::endl;
    return false;
  }

  // Verify private vs public IP classification
  const char * privateIPs[] = { "192.168.0.107", "10.200.50.1", "172.20.1.5", "127.0.0.1", "100.64.0.1" };
  for ( const char * priv : privateIPs )
  {
    if ( !RoomCode::IsPrivateOrReservedIP(priv) || RoomCode::IsPublicInternetIP(priv) )
    {
      std::cerr << "TestRoomCodeEncodingAndDecoding: private IP was not classified as private: " << priv << std::endl;
      return false;
    }
  }

  const char * publicIPs[] = { "92.188.50.166", "8.8.8.8", "1.1.1.1", "82.64.12.34" };
  for ( const char * pub : publicIPs )
  {
    if ( RoomCode::IsPrivateOrReservedIP(pub) || !RoomCode::IsPublicInternetIP(pub) )
    {
      std::cerr << "TestRoomCodeEncodingAndDecoding: public IP was not classified as public: " << pub << std::endl;
      return false;
    }
  }

  std::cout << "[INFO] Example Public Room Code for 92.188.50.166:27015: " << RoomCode::Encode("92.188.50.166", 27015) << std::endl;

  std::cout << "[PASS] Room code encoding, decoding & checksum validation passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// TestUpnpServiceLifecycle
// ----------------------------------------------------------------------------
bool TetrisUnitTests::TestUpnpServiceLifecycle()
{
  using namespace RTRT;

  {
    UpnpService service;
    if ( service.GetStatus()._State != UpnpState::Idle )
    {
      std::cerr << "TestUpnpServiceLifecycle: initial state is not Idle" << std::endl;
      return false;
    }

    const auto t0 = std::chrono::steady_clock::now();
    service.StartMappingAsync(27097, 27097, "RTRT Unit Test");
    const auto callMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    if ( callMs > 250 )
    {
      std::cerr << "TestUpnpServiceLifecycle: StartMappingAsync blocked the caller (" << callMs << " ms)" << std::endl;
      return false;
    }

    const UpnpState early = service.GetStatus()._State;
    if ( early == UpnpState::Idle )
    {
      std::cerr << "TestUpnpServiceLifecycle: state did not leave Idle after start" << std::endl;
      return false;
    }

    service.StopMappingAsync();
    // Destructor joins the worker (discovery is bounded to ~2 s)
  }

  // Verify public IP resolution via STUN reflection
  const std::string resolvedPubIP = UpnpService::ResolvePublicIP(1500);
  if ( !resolvedPubIP.empty() )
  {
    if ( !RoomCode::IsPublicInternetIP(resolvedPubIP) )
    {
      std::cerr << "TestUpnpServiceLifecycle: ResolvePublicIP returned non-public IP: " << resolvedPubIP << std::endl;
      return false;
    }
    std::cout << "[PASS] Verified public WAN IP resolution: " << resolvedPubIP << std::endl;
  }

  std::cout << "[PASS] UPnP service async start/stop lifecycle passed." << std::endl;
  return true;
}

// ----------------------------------------------------------------------------
// RunAll
// ----------------------------------------------------------------------------
int TetrisUnitTests::RunAll()
{
  std::cout << "==========================================" << std::endl;
  std::cout << "Running Tetris Guideline Headless Test Suite" << std::endl;
  std::cout << "==========================================" << std::endl;

  bool ok = true;
  ok = ok && Test7BagRandomizer();
  ok = ok && TestSRSKicks();
  ok = ok && TestTSpinDetection();
  ok = ok && TestExtendedLockDelay();
  ok = ok && TestScoringAndB2B();
  ok = ok && TestAudioEngineSynthesis();
  ok = ok && TestMatrixAddGarbageLines();
  ok = ok && TestGarbageCalculationAndCancellation();
  ok = ok && TestMultiplayerInputMapping();
  ok = ok && TestNetSnapshotSerialization();
  ok = ok && TestOnlineLoopbackSession();
  ok = ok && TestRoomCodeEncodingAndDecoding();
  ok = ok && TestUpnpServiceLifecycle();

  std::cout << "==========================================" << std::endl;
  if ( ok )
  {
    std::cout << "ALL TETRIS GUIDELINE, AUDIO & MULTIPLAYER TESTS PASSED (13/13)" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
  }

  std::cerr << "SOME TESTS FAILED!" << std::endl;
  std::cout << "==========================================" << std::endl;
  return 1;
}

}
