#include "TetrisGameLogic.h"
#include <algorithm>
#include <cmath>

namespace RTRT
{

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
TetrisGameLogic::TetrisGameLogic()
{
  StartNewGame(TetrisGameMode::Marathon, 1, 0);
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisGameLogic::~TetrisGameLogic()
{
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void TetrisGameLogic::Reset()
{
  StartNewGame(_Mode, _StartLevel, 0);
}

// ----------------------------------------------------------------------------
// StartNewGame
// ----------------------------------------------------------------------------
void TetrisGameLogic::StartNewGame( TetrisGameMode iMode, int iStartLevel, unsigned int iSeed )
{
  _Mode = iMode;
  _StartLevel = MathUtil::Clamp(iStartLevel, 1, 15);
  _Level = _StartLevel;

  _Matrix.Clear();
  _Randomizer.Reset(iSeed);

  _ActivePiece = TetrisPieceType::None;
  _ActiveRotation = TetrisRotation::Spawn;
  _ActiveOrigin = Vec2i(0, 0);
  _GhostY = 0;

  _HoldPiece = TetrisPieceType::None;
  _CanHold = true;

  _GravityTimer = 0.f;
  _LockTimer = 0.f;
  _LockResetCount = 0;
  _LowestYReached = 0;

  _DASTimer = 0.f;
  _ARRTimer = 0.f;
  _HeldDirection = 0;

  _LastMoveWasRotation = false;
  _LastKickIndex = 0;

  _Score = 0;
  _LinesCleared = 0;
  _ComboCount = -1;
  _BackToBackActive = false;

  _ElapsedTime = 0.;
  _RemainingTime = 120.; // 2 minutes for Ultra
  _GameStarted = false;

  _GameOver = false;
  _Victory = false;
  _Paused = false;
  _GameOverReason = "";

  _ActionBannerText = "";
  _ActionBannerTimer = 0.f;
  _ActionBannerAlpha = 0.f;
  _ScreenShakeImpulse = 0.f;
  _LastClearedRows.clear();

  SpawnNextPiece();
}

// ----------------------------------------------------------------------------
// ComputeGravityInterval
// ----------------------------------------------------------------------------
float TetrisGameLogic::ComputeGravityInterval( int iLevel ) const
{
  const int level = MathUtil::Clamp(iLevel, 1, 20);
  // Official Guideline formula: (0.8 - ((L - 1) * 0.007))^(L - 1)
  const double base = 0.8 - ( static_cast<double>(level - 1) * 0.007 );
  const double seconds = std::pow(base, static_cast<double>(level - 1));
  return static_cast<float>(std::max(0.003, seconds));
}

// ----------------------------------------------------------------------------
// UpdateGhost
// ----------------------------------------------------------------------------
void TetrisGameLogic::UpdateGhost()
{
  if ( TetrisPieceType::None == _ActivePiece )
  {
    _GhostY = 0;
    return;
  }

  int testY = _ActiveOrigin.y;
  while ( _Matrix.CanPieceFit(_ActivePiece, _ActiveRotation, Vec2i(_ActiveOrigin.x, testY - 1)) )
    testY--;

  _GhostY = testY;
}

// ----------------------------------------------------------------------------
// IsActivePieceOnSurface
// ----------------------------------------------------------------------------
bool TetrisGameLogic::IsActivePieceOnSurface() const
{
  if ( TetrisPieceType::None == _ActivePiece )
    return false;

  return !_Matrix.CanPieceFit(_ActivePiece, _ActiveRotation, Vec2i(_ActiveOrigin.x, _ActiveOrigin.y - 1));
}

// ----------------------------------------------------------------------------
// OnFloorManipulation
// ----------------------------------------------------------------------------
void TetrisGameLogic::OnFloorManipulation()
{
  if ( !IsActivePieceOnSurface() )
    return;

  // Extended placement rule: if the piece reaches a new lowest row, reset count
  if ( _ActiveOrigin.y < _LowestYReached )
  {
    _LowestYReached = _ActiveOrigin.y;
    _LockResetCount = 0;
    _LockTimer = 0.f;
  }
  else if ( _LockResetCount < 15 )
  {
    _LockResetCount++;
    _LockTimer = 0.f;
  }
}

// ----------------------------------------------------------------------------
// SpawnNextPiece
// ----------------------------------------------------------------------------
void TetrisGameLogic::SpawnNextPiece()
{
  _ActivePiece = _Randomizer.Next();
  _ActiveRotation = TetrisRotation::Spawn;
  _ActiveOrigin = TetrisPiece::GetSpawnOrigin(_ActivePiece);

  _GravityTimer = 0.f;
  _LockTimer = 0.f;
  _LockResetCount = 0;
  _LowestYReached = _ActiveOrigin.y;

  _LastMoveWasRotation = false;
  _LastKickIndex = 0;
  _CanHold = true;

  if ( !_Matrix.CanPieceFit(_ActivePiece, _ActiveRotation, _ActiveOrigin) )
  {
    _GameOver = true;
    _GameOverReason = "Block Out";
    _ActionBannerText = "GAME OVER";
    _ActionBannerTimer = 3.f;
    return;
  }

  UpdateGhost();
}

// ----------------------------------------------------------------------------
// TryMove
// ----------------------------------------------------------------------------
bool TetrisGameLogic::TryMove( int iDx, int iDy )
{
  if ( _GameOver || _Victory || _Paused || ( TetrisPieceType::None == _ActivePiece ) )
    return false;

  const Vec2i newOrigin(_ActiveOrigin.x + iDx, _ActiveOrigin.y + iDy);
  if ( !_Matrix.CanPieceFit(_ActivePiece, _ActiveRotation, newOrigin) )
    return false;

  _ActiveOrigin = newOrigin;
  _LastMoveWasRotation = false;
  _LastKickIndex = 0;
  _GameStarted = true;

  UpdateGhost();
  OnFloorManipulation();
  return true;
}

// ----------------------------------------------------------------------------
// TryRotate
// ----------------------------------------------------------------------------
bool TetrisGameLogic::TryRotate( TetrisRotation iTargetRotation )
{
  if ( _GameOver || _Victory || _Paused || ( TetrisPieceType::None == _ActivePiece ) )
    return false;

  if ( _ActivePiece == TetrisPieceType::O )
  {
    _LastMoveWasRotation = true;
    _LastKickIndex = 0;
    return true;
  }

  std::array<Vec2i, 5> kicks;
  TetrisPiece::GetSRSKickOffsets(_ActivePiece, _ActiveRotation, iTargetRotation, kicks);

  for ( int kickIdx = 0; kickIdx < 5; ++kickIdx )
  {
    const Vec2i candidateOrigin(_ActiveOrigin.x + kicks[kickIdx].x, _ActiveOrigin.y + kicks[kickIdx].y);
    if ( _Matrix.CanPieceFit(_ActivePiece, iTargetRotation, candidateOrigin) )
    {
      _ActiveOrigin = candidateOrigin;
      _ActiveRotation = iTargetRotation;
      _LastMoveWasRotation = true;
      _LastKickIndex = kickIdx;
      _GameStarted = true;

      UpdateGhost();
      OnFloorManipulation();
      return true;
    }
  }

  return false;
}

// ----------------------------------------------------------------------------
// PerformHardDrop
// ----------------------------------------------------------------------------
void TetrisGameLogic::PerformHardDrop()
{
  if ( _GameOver || _Victory || _Paused || ( TetrisPieceType::None == _ActivePiece ) )
    return;

  UpdateGhost();
  const int droppedRows = _ActiveOrigin.y - _GhostY;
  _ActiveOrigin.y = _GhostY;
  _Score += droppedRows * 2;
  _GameStarted = true;

  _ScreenShakeImpulse = std::min(1.f, _ScreenShakeImpulse + 0.35f);
  LockActivePiece(true);
}

// ----------------------------------------------------------------------------
// PerformHold
// ----------------------------------------------------------------------------
void TetrisGameLogic::PerformHold()
{
  if ( _GameOver || _Victory || _Paused || !_CanHold || ( TetrisPieceType::None == _ActivePiece ) )
    return;

  const TetrisPieceType currentPiece = _ActivePiece;
  if ( TetrisPieceType::None == _HoldPiece )
  {
    _HoldPiece = currentPiece;
    SpawnNextPiece();
  }
  else
  {
    _ActivePiece = _HoldPiece;
    _HoldPiece = currentPiece;
    _ActiveRotation = TetrisRotation::Spawn;
    _ActiveOrigin = TetrisPiece::GetSpawnOrigin(_ActivePiece);

    _GravityTimer = 0.f;
    _LockTimer = 0.f;
    _LockResetCount = 0;
    _LowestYReached = _ActiveOrigin.y;
    _LastMoveWasRotation = false;
    _LastKickIndex = 0;

    if ( !_Matrix.CanPieceFit(_ActivePiece, _ActiveRotation, _ActiveOrigin) )
    {
      _GameOver = true;
      _GameOverReason = "Block Out";
      _ActionBannerText = "GAME OVER";
      _ActionBannerTimer = 3.f;
      return;
    }

    UpdateGhost();
  }

  _CanHold = false;
  _GameStarted = true;
}

// ----------------------------------------------------------------------------
// DetectTSpin
// ----------------------------------------------------------------------------
TetrisTSpinType TetrisGameLogic::DetectTSpin() const
{
  if ( ( TetrisPieceType::T != _ActivePiece ) || !_LastMoveWasRotation )
    return TetrisTSpinType::None;

  // Center of T in 3x3 local box is (1, 1)
  const Vec2i center(_ActiveOrigin.x + 1, _ActiveOrigin.y + 1);

  // 4 diagonal corners around center
  const bool tl = _Matrix.IsCellOccupied(center.x - 1, center.y + 1);
  const bool tr = _Matrix.IsCellOccupied(center.x + 1, center.y + 1);
  const bool bl = _Matrix.IsCellOccupied(center.x - 1, center.y - 1);
  const bool br = _Matrix.IsCellOccupied(center.x + 1, center.y - 1);

  const int occupiedCorners = ( tl ? 1 : 0 ) + ( tr ? 1 : 0 ) + ( bl ? 1 : 0 ) + ( br ? 1 : 0 );
  if ( occupiedCorners < 3 )
    return TetrisTSpinType::None;

  // Guideline 3-corner rule: test front corners facing the T protrusion
  bool frontA = false;
  bool frontB = false;

  switch ( _ActiveRotation )
  {
    case TetrisRotation::Spawn: // Pointing UP
      frontA = tl; frontB = tr;
      break;
    case TetrisRotation::Right: // Pointing RIGHT
      frontA = tr; frontB = br;
      break;
    case TetrisRotation::Flip:  // Pointing DOWN
      frontA = bl; frontB = br;
      break;
    case TetrisRotation::Left:  // Pointing LEFT
      frontA = tl; frontB = bl;
      break;
  }

  // If both front corners are occupied -> Full T-Spin
  // If kick test 4 (5th test in SRS) was used -> promoted to Full T-Spin
  if ( ( frontA && frontB ) || ( 4 == _LastKickIndex ) )
    return TetrisTSpinType::Full;

  return TetrisTSpinType::Mini;
}

// ----------------------------------------------------------------------------
// LockActivePiece
// ----------------------------------------------------------------------------
void TetrisGameLogic::LockActivePiece( bool )
{
  if ( ( TetrisPieceType::None == _ActivePiece ) || _GameOver )
    return;

  // Evaluate T-Spin before committing to matrix
  const TetrisTSpinType tSpin = DetectTSpin();

  bool lockedAboveSkyline = false;
  if ( !_Matrix.LockPiece(_ActivePiece, _ActiveRotation, _ActiveOrigin, lockedAboveSkyline) )
  {
    _GameOver = true;
    _GameOverReason = "Lock Collision";
    _ActionBannerText = "GAME OVER";
    _ActionBannerTimer = 3.f;
    return;
  }

  if ( lockedAboveSkyline )
  {
    _GameOver = true;
    _GameOverReason = "Lock Out";
    _ActionBannerText = "LOCK OUT";
    _ActionBannerTimer = 3.f;
    return;
  }

  // Clear full lines
  _LastClearedRows.clear();
  const int linesCleared = _Matrix.ClearFullLines(_LastClearedRows);
  const bool perfectClear = ( linesCleared > 0 ) && _Matrix.IsEmpty();

  // Evaluate combo & difficult line clear
  const bool isDifficult = ( 4 == linesCleared ) || ( ( TetrisTSpinType::None != tSpin ) && ( linesCleared > 0 ) );
  int awardScore = 0;
  std::string banner = "";

  if ( linesCleared > 0 )
  {
    _ComboCount++;
    _LinesCleared += linesCleared;

    if ( TetrisTSpinType::Full == tSpin )
    {
      if ( 1 == linesCleared )
      {
        awardScore = ( _BackToBackActive ? 1200 : 800 ) * _Level;
        banner = _BackToBackActive ? "B2B T-SPIN SINGLE!" : "T-SPIN SINGLE!";
      }
      else if ( 2 == linesCleared )
      {
        awardScore = ( _BackToBackActive ? 1800 : 1200 ) * _Level;
        banner = _BackToBackActive ? "B2B T-SPIN DOUBLE!" : "T-SPIN DOUBLE!";
      }
      else if ( 3 == linesCleared )
      {
        awardScore = ( _BackToBackActive ? 2400 : 1600 ) * _Level;
        banner = _BackToBackActive ? "B2B T-SPIN TRIPLE!" : "T-SPIN TRIPLE!";
      }
    }
    else if ( TetrisTSpinType::Mini == tSpin )
    {
      if ( 1 == linesCleared )
      {
        awardScore = ( _BackToBackActive ? 300 : 200 ) * _Level;
        banner = _BackToBackActive ? "B2B T-SPIN MINI!" : "T-SPIN MINI!";
      }
      else if ( 2 == linesCleared )
      {
        awardScore = ( _BackToBackActive ? 600 : 400 ) * _Level;
        banner = _BackToBackActive ? "B2B T-SPIN MINI DOUBLE!" : "T-SPIN MINI DOUBLE!";
      }
    }
    else
    {
      if ( 1 == linesCleared )
      {
        awardScore = 100 * _Level;
        banner = "SINGLE";
      }
      else if ( 2 == linesCleared )
      {
        awardScore = 300 * _Level;
        banner = "DOUBLE";
      }
      else if ( 3 == linesCleared )
      {
        awardScore = 500 * _Level;
        banner = "TRIPLE";
      }
      else if ( 4 == linesCleared )
      {
        awardScore = ( _BackToBackActive ? 1200 : 800 ) * _Level;
        banner = _BackToBackActive ? "B2B TETRIS!" : "TETRIS!";
        _ScreenShakeImpulse = std::min(1.f, _ScreenShakeImpulse + 0.65f);
      }
    }

    if ( _ComboCount > 0 )
      awardScore += 50 * _ComboCount * _Level;

    if ( perfectClear )
    {
      banner = "PERFECT CLEAR!";
      if ( 1 == linesCleared ) awardScore += 800 * _Level;
      else if ( 2 == linesCleared ) awardScore += 1200 * _Level;
      else if ( 3 == linesCleared ) awardScore += 1800 * _Level;
      else if ( 4 == linesCleared ) awardScore += ( _BackToBackActive ? 3200 : 2000 ) * _Level;
      _ScreenShakeImpulse = 1.0f;
    }

    _BackToBackActive = isDifficult;
  }
  else
  {
    _ComboCount = -1;
    if ( TetrisTSpinType::Full == tSpin )
    {
      awardScore = 100 * _Level;
      banner = "T-SPIN";
    }
    else if ( TetrisTSpinType::Mini == tSpin )
    {
      awardScore = 50 * _Level;
      banner = "T-SPIN MINI";
    }
  }

  _Score += awardScore;
  if ( _Score > _HighScore )
    _HighScore = _Score;

  if ( !banner.empty() )
  {
    _ActionBannerText = banner;
    _ActionBannerTimer = 1.8f;
  }

  // Mode goals
  if ( TetrisGameMode::Marathon == _Mode )
  {
    const int targetLevel = _StartLevel + ( _LinesCleared / 10 );
    _Level = MathUtil::Clamp(targetLevel, 1, 15);

    if ( !_EndlessMarathon && ( _LinesCleared >= 150 ) )
    {
      _Victory = true;
      _ActionBannerText = "VICTORY!";
      _ActionBannerTimer = 4.f;
      return;
    }
  }
  else if ( TetrisGameMode::Sprint == _Mode )
  {
    if ( _LinesCleared >= 40 )
    {
      _Victory = true;
      _ActionBannerText = "40 LINES COMPLETE!";
      _ActionBannerTimer = 4.f;
      return;
    }
  }

  SpawnNextPiece();
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
void TetrisGameLogic::Update( float iDeltaTime, const TetrisInputState & iInput, const TetrisHandlingConfig & iHandling )
{
  if ( iInput._ResetPressed )
  {
    Reset();
    return;
  }

  if ( iInput._PausePressed )
    TogglePaused();

  if ( _GameOver || _Victory || _Paused )
    return;

  // Timers update
  if ( _GameStarted )
  {
    _ElapsedTime += iDeltaTime;
    if ( TetrisGameMode::Ultra == _Mode )
    {
      _RemainingTime = std::max(0., _RemainingTime - static_cast<double>(iDeltaTime));
      if ( _RemainingTime <= 0. )
      {
        _Victory = true;
        _ActionBannerText = "TIME'S UP!";
        _ActionBannerTimer = 4.f;
        return;
      }
    }
  }

  // Action banner fade
  if ( _ActionBannerTimer > 0.f )
  {
    _ActionBannerTimer = std::max(0.f, _ActionBannerTimer - iDeltaTime);
    _ActionBannerAlpha = MathUtil::Clamp(_ActionBannerTimer, 0.f, 1.f);
  }
  else
    _ActionBannerAlpha = 0.f;

  // Screen shake decay
  _ScreenShakeImpulse = std::max(0.f, _ScreenShakeImpulse - iDeltaTime * 4.0f);

  // 1. Edge-triggered actions
  if ( iInput._HoldPressed )
    PerformHold();

  if ( iInput._RotateCWPressed )
    TryRotate(TetrisPiece::RotateCW(_ActiveRotation));

  if ( iInput._RotateCCWPressed )
    TryRotate(TetrisPiece::RotateCCW(_ActiveRotation));

  if ( iInput._Rotate180Pressed )
    TryRotate(TetrisPiece::Rotate180(_ActiveRotation));

  if ( iInput._HardDropPressed )
  {
    PerformHardDrop();
    return;
  }

  // 2. DAS / ARR Lateral Movement
  int activeDir = 0;
  if ( iInput._LeftHeld && !iInput._RightHeld )
    activeDir = -1;
  else if ( iInput._RightHeld && !iInput._LeftHeld )
    activeDir = 1;

  if ( iInput._MoveLeftPressed )
  {
    TryMove(-1, 0);
    _HeldDirection = -1;
    _DASTimer = 0.f;
    _ARRTimer = 0.f;
  }
  else if ( iInput._MoveRightPressed )
  {
    TryMove(1, 0);
    _HeldDirection = 1;
    _DASTimer = 0.f;
    _ARRTimer = 0.f;
  }
  else if ( activeDir != 0 )
  {
    if ( activeDir != _HeldDirection )
    {
      _HeldDirection = activeDir;
      _DASTimer = 0.f;
      _ARRTimer = 0.f;
      TryMove(_HeldDirection, 0);
    }
    else
    {
      _DASTimer += iDeltaTime;
      if ( _DASTimer >= iHandling._DAS )
      {
        if ( iHandling._ARR <= 0.0001f )
        {
          // Instant shift
          while ( TryMove(_HeldDirection, 0) ) {}
        }
        else
        {
          _ARRTimer += iDeltaTime;
          while ( _ARRTimer >= iHandling._ARR )
          {
            TryMove(_HeldDirection, 0);
            _ARRTimer -= iHandling._ARR;
          }
        }
      }
    }
  }
  else
  {
    _HeldDirection = 0;
    _DASTimer = 0.f;
    _ARRTimer = 0.f;
  }

  // 3. Gravity & Soft Drop
  float gravityInterval = ComputeGravityInterval(_Level);
  if ( iInput._SoftDropHeld )
  {
    gravityInterval = std::max(0.005f, gravityInterval / std::max(1.f, iHandling._SoftDropFactor));
  }

  _GravityTimer += iDeltaTime;
  while ( _GravityTimer >= gravityInterval )
  {
    _GravityTimer -= gravityInterval;
    if ( TryMove(0, -1) )
    {
      if ( iInput._SoftDropHeld )
        _Score += 1;
    }
    else
      break;
  }

  // 4. Lock Delay
  if ( IsActivePieceOnSurface() )
  {
    _LockTimer += iDeltaTime;
    if ( _LockTimer >= iHandling._LockDelay )
      LockActivePiece(false);
  }
  else
    _LockTimer = 0.f;
}

}
