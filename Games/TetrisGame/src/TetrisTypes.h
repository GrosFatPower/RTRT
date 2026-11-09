#ifndef _TetrisTypes_
#define _TetrisTypes_

#include "MathUtil.h"
#include <array>
#include <string>
#include <vector>

namespace Tetris
{

enum class TetrisPieceType : int
{
  None    = 0,
  I       = 1, // Cyan
  J       = 2, // Blue
  L       = 3, // Orange
  O       = 4, // Yellow
  S       = 5, // Green
  T       = 6, // Magenta / Purple
  Z       = 7, // Red
  Garbage = 8, // Metallic Grey
  Count   = 9
};

enum class TetrisRotation : int
{
  Spawn = 0, // 0
  Right = 1, // R (Clockwise 90)
  Flip  = 2, // 2 (180)
  Left  = 3  // L (Counter-Clockwise 270 / 90 CCW)
};

enum class TetrisGameMode : int
{
  Marathon = 0,
  Sprint   = 1, // 40 Lines
  Ultra    = 2, // 2-minute score attack
  Versus   = 3  // 2-Player Local Battle (Garbage Wars)
};

enum class TetrisControlDevice : int
{
  None          = -1,
  KeyboardLeft  = 0, // WASD, Space, Shift, C, Q, E
  KeyboardRight = 1, // Arrows, Numpad, RShift
  KeyboardFull  = 2, // All keyboard keys (1P or 2P when paired with Gamepad)
  Gamepad1      = 3, // 1st connected gamepad (Slot 0)
  Gamepad2      = 4  // 2nd connected gamepad (Slot 1)
};

enum class TetrisMultiplayerPreset : int
{
  Auto              = 0, // Auto-assign based on connected gamepads
  Gamepad1_Gamepad2 = 1, // P1: Gamepad 1,       P2: Gamepad 2
  Gamepad1_Keyboard = 2, // P1: Gamepad 1,       P2: Keyboard (Full)
  Keyboard_Gamepad1 = 3, // P1: Keyboard (Full), P2: Gamepad 1
  Keyboard_Keyboard = 4  // P1: Keyboard (WASD), P2: Keyboard (Arrows)
};

enum class TetrisRendererMode : int
{
  Deferred   = 0,
  PathTracer = 1,
  Software   = 2
};

enum class TetrisMaterialPreset : int
{
  GlossyPlastic = 0,
  CrystalGlass  = 1,
  CyberpunkNeon = 2,
  Metallic      = 3
};

enum class TetrisCameraPreset : int
{
  Frontal3D = 0,
  Isometric = 1,
  FreeOrbit = 2
};

enum class TetrisLineClearAnimMode : int
{
  Dynamic      = 0, // Scales with clear type (1=Blink, 2=Wave, 3=EdgeCascade, 4/TSpin/PC=Burst)
  ClassicBlink = 1, // Blinks 3 times white/color before vanishing
  BlockBurst   = 2, // Explosive 3D shatter outward with gravity & spin
  EdgeCascade  = 3, // Peels from outer edges and falls off the sides
  CenterWave   = 4, // Center-out ripple spin & shrink
  Random       = 5  // Randomly picks one of the 4 animations on each clear
};

struct TetrisClearedRow
{
  int                             _RowY  = 0;
  std::array<TetrisPieceType, 10> _Cells = {};
};

enum class TetrisTSpinType : int
{
  None = 0,
  Mini = 1,
  Full = 2
};

enum class TetrisAppState : int
{
  TitleScreen = 0,
  InGame      = 1
};

enum class TetrisMenuPage : int
{
  MainMenu         = 0,
  ModeSelect       = 1,
  HighScores       = 2,
  Options          = 3,
  MultiplayerLobby = 4
};

struct TetrisSessionStats
{
  long long _BestMarathonScore = 0;
  int       _BestMarathonLines = 0;
  int       _BestMarathonLevel = 1;

  double    _BestSprint40Time  = 0.0; // 0.0 = not yet completed
  long long _BestSprintScore   = 0;

  long long _BestUltraScore    = 0;
  int       _BestUltraLines    = 0;

  int       _TotalGamesPlayed  = 0;
  int       _TotalLinesCleared = 0;
  int       _TotalTetrises     = 0;
  int       _TotalTSpins       = 0;

  void Reset()
  {
    *this = TetrisSessionStats();
  }
};

struct TetrisHandlingConfig
{
  float _DAS = 0.167f;              // Delayed Auto Shift (seconds)
  float _ARR = 0.033f;              // Auto Repeat Rate (seconds)
  float _SoftDropFactor = 20.0f;    // Gravity multiplier during soft drop
  float _LockDelay = 0.5f;          // Lock delay timer (seconds)
  int   _MaxLockResets = 15;        // Extended placement manipulation cap
};

struct TetrisInputState
{
  bool _LeftHeld        = false;
  bool _RightHeld       = false;
  bool _SoftDropHeld    = false;

  bool _MoveLeftPressed = false;
  bool _MoveRightPressed= false;
  bool _HardDropPressed = false;
  bool _RotateCWPressed = false;
  bool _RotateCCWPressed= false;
  bool _Rotate180Pressed= false;
  bool _HoldPressed     = false;

  bool _PausePressed    = false;
  bool _ResetPressed    = false;
};

struct TetrisLineClearResult
{
  int            _LinesCleared = 0;
  TetrisTSpinType _TSpin = TetrisTSpinType::None;
  bool           _BackToBack = false;
  int            _Combo = 0;
  bool           _PerfectClear = false;
  int            _ScoreAwarded = 0;
  std::string    _ActionText = "";
};

}

#endif /* _TetrisTypes_ */
