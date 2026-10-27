#ifndef _TetrisMatrix_
#define _TetrisMatrix_

#include "TetrisPiece.h"
#include <array>
#include <vector>

namespace RTRT
{

class TetrisMatrix
{
public:
  static constexpr int S_Width         = 10;
  static constexpr int S_VisibleHeight = 20;
  static constexpr int S_TotalHeight   = 40;
  static constexpr int S_Skyline       = 20;

public:
  TetrisMatrix();
  virtual ~TetrisMatrix();

  void Clear();

  bool IsInside( int iX, int iY ) const;
  bool IsCellOccupied( int iX, int iY ) const;

  TetrisPieceType GetCell( int iX, int iY ) const;
  void SetCell( int iX, int iY, TetrisPieceType iType );

  bool CanPieceFit( TetrisPieceType iType, TetrisRotation iRotation, const Vec2i & iOrigin ) const;
  bool LockPiece( TetrisPieceType iType, TetrisRotation iRotation, const Vec2i & iOrigin, bool & oAboveSkyline );
  int ClearFullLines( std::vector<int> & oClearedRows );

  bool IsEmpty() const;

protected:
  int CellIndex( int iX, int iY ) const { return iY * S_Width + iX; }

protected:
  std::array<TetrisPieceType, S_Width * S_TotalHeight> _Grid;
};

}

#endif /* _TetrisMatrix_ */
