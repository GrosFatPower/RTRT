#ifndef _TetrisTypes_
#define _TetrisTypes_

#include "MathUtil.h"
#include <array>
#include <string>
#include <vector>

namespace RTRT
{

enum class TetrisPieceType : int
{
  None  = 0,
  I     = 1, // Cyan
  J     = 2, // Blue
  L     = 3, // Orange
  O     = 4, // Yellow
  S     = 5, // Green
  T     = 6, // Magenta / Purple
  Z     = 7, // Red
  Count = 8
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
  Ultra    = 2  // 2-minute score attack
};

enum class TetrisRendererMode : int
{
  Deferred        = 0,
  PhotoPathTracer = 1,
  Software        = 2
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

enum class TetrisTSpinType : int
{
  None = 0,
  Mini = 1,
  Full = 2
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
