#ifndef _TetrisGameLogic_
#define _TetrisGameLogic_

#include "TetrisMatrix.h"
#include "TetrisPiece.h"
#include "TetrisRandomizer.h"
#include "TetrisTypes.h"
#include <string>
#include <vector>

namespace RTRT
{

class TetrisGameLogic
{
public:
  TetrisGameLogic();
  virtual ~TetrisGameLogic();

  void StartNewGame( TetrisGameMode iMode, int iStartLevel = 1, unsigned int iSeed = 0 );
  void Reset();

  void Update( float iDeltaTime, const TetrisInputState & iInput, const TetrisHandlingConfig & iHandling );

  bool TryMove( int iDx, int iDy );
  bool TryRotate( TetrisRotation iTargetRotation );
  void PerformHardDrop();
  void PerformHold();

  const TetrisMatrix & GetMatrix() const { return _Matrix; }
  TetrisMatrix & GetMatrix() { return _Matrix; }

  TetrisPieceType GetActivePiece() const { return _ActivePiece; }
  TetrisRotation GetActiveRotation() const { return _ActiveRotation; }
  const Vec2i & GetActiveOrigin() const { return _ActiveOrigin; }
  int GetGhostY() const { return _GhostY; }

  TetrisPieceType GetHoldPiece() const { return _HoldPiece; }
  bool CanHold() const { return _CanHold; }

  TetrisPieceType PeekNextPiece( int iIndex = 0 ) const { return _Randomizer.Peek(iIndex); }

  TetrisGameMode GetGameMode() const { return _Mode; }
  void SetGameMode( TetrisGameMode iMode ) { _Mode = iMode; }

  int GetLevel() const { return _Level; }
  int GetStartLevel() const { return _StartLevel; }
  void SetStartLevel( int iLevel ) { _StartLevel = MathUtil::Clamp(iLevel, 1, 15); }

  bool IsEndlessMarathon() const { return _EndlessMarathon; }
  void SetEndlessMarathon( bool iEndless ) { _EndlessMarathon = iEndless; }

  long long GetScore() const { return _Score; }
  long long GetHighScore() const { return _HighScore; }
  int GetLinesCleared() const { return _LinesCleared; }
  int GetComboCount() const { return _ComboCount; }
  bool IsBackToBackActive() const { return _BackToBackActive; }

  double GetElapsedTime() const { return _ElapsedTime; }
  double GetRemainingTime() const { return _RemainingTime; }

  bool IsGameOver() const { return _GameOver; }
  bool IsVictory() const { return _Victory; }
  bool IsPaused() const { return _Paused; }
  void SetPaused( bool iPaused ) { _Paused = iPaused; }
  void TogglePaused() { _Paused = !_Paused; }

  const std::string & GetGameOverReason() const { return _GameOverReason; }
  const std::string & GetActionBannerText() const { return _ActionBannerText; }
  float GetActionBannerAlpha() const { return _ActionBannerAlpha; }
  float GetScreenShakeImpulse() const { return _ScreenShakeImpulse; }

  const std::vector<int> & GetLastClearedRows() const { return _LastClearedRows; }

  TetrisTSpinType DetectTSpin() const;
  float ComputeGravityInterval( int iLevel ) const;

protected:
  void SpawnNextPiece();
  void UpdateGhost();
  void LockActivePiece( bool iFromHardDrop );
  void OnFloorManipulation();
  bool IsActivePieceOnSurface() const;

protected:
  TetrisMatrix     _Matrix;
  TetrisRandomizer _Randomizer;

  TetrisPieceType  _ActivePiece    = TetrisPieceType::None;
  TetrisRotation   _ActiveRotation = TetrisRotation::Spawn;
  Vec2i            _ActiveOrigin   = Vec2i(0, 0);
  int              _GhostY         = 0;

  TetrisPieceType  _HoldPiece      = TetrisPieceType::None;
  bool             _CanHold        = true;

  // Timers & handling state
  float            _GravityTimer   = 0.f;
  float            _LockTimer      = 0.f;
  int              _LockResetCount = 0;
  int              _LowestYReached = 0;

  float            _DASTimer       = 0.f;
  float            _ARRTimer       = 0.f;
  int              _HeldDirection  = 0; // -1 left, +1 right, 0 none

  bool             _LastMoveWasRotation = false;
  int              _LastKickIndex       = 0;

  // Mode and stats
  TetrisGameMode   _Mode           = TetrisGameMode::Marathon;
  int              _StartLevel     = 1;
  int              _Level          = 1;
  bool             _EndlessMarathon= false;

  long long        _Score          = 0;
  long long        _HighScore      = 0;
  int              _LinesCleared   = 0;
  int              _ComboCount     = -1;
  bool             _BackToBackActive = false;

  double           _ElapsedTime    = 0.;
  double           _RemainingTime  = 120.; // 2 minutes for Ultra
  bool             _GameStarted    = false;

  bool             _GameOver       = false;
  bool             _Victory        = false;
  bool             _Paused         = false;
  std::string      _GameOverReason = "";

  // Visual callout & feedback
  std::string      _ActionBannerText    = "";
  float            _ActionBannerTimer   = 0.f;
  float            _ActionBannerAlpha   = 0.f;
  float            _ScreenShakeImpulse  = 0.f;
  std::vector<int> _LastClearedRows;
};

}

#endif /* _TetrisGameLogic_ */
