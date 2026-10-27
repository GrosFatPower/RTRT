#include "TetrisMatrix.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
TetrisMatrix::TetrisMatrix()
{
  Clear();
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisMatrix::~TetrisMatrix()
{
}

// ----------------------------------------------------------------------------
// Clear
// ----------------------------------------------------------------------------
void TetrisMatrix::Clear()
{
  _Grid.fill(TetrisPieceType::None);
}

// ----------------------------------------------------------------------------
// IsInside
// ----------------------------------------------------------------------------
bool TetrisMatrix::IsInside( int iX, int iY ) const
{
  return ( iX >= 0 ) && ( iX < S_Width ) && ( iY >= 0 ) && ( iY < S_TotalHeight );
}

// ----------------------------------------------------------------------------
// IsCellOccupied
// ----------------------------------------------------------------------------
bool TetrisMatrix::IsCellOccupied( int iX, int iY ) const
{
  // Boundary check: left, right, and floor boundaries are considered occupied
  if ( ( iX < 0 ) || ( iX >= S_Width ) || ( iY < 0 ) )
    return true;

  if ( iY >= S_TotalHeight )
    return false; // Above ceiling is open space

  return ( TetrisPieceType::None != _Grid[CellIndex(iX, iY)] );
}

// ----------------------------------------------------------------------------
// GetCell
// ----------------------------------------------------------------------------
TetrisPieceType TetrisMatrix::GetCell( int iX, int iY ) const
{
  if ( !IsInside(iX, iY) )
    return TetrisPieceType::None;

  return _Grid[CellIndex(iX, iY)];
}

// ----------------------------------------------------------------------------
// SetCell
// ----------------------------------------------------------------------------
void TetrisMatrix::SetCell( int iX, int iY, TetrisPieceType iType )
{
  if ( IsInside(iX, iY) )
    _Grid[CellIndex(iX, iY)] = iType;
}

// ----------------------------------------------------------------------------
// CanPieceFit
// ----------------------------------------------------------------------------
bool TetrisMatrix::CanPieceFit( TetrisPieceType iType, TetrisRotation iRotation, const Vec2i & iOrigin ) const
{
  if ( ( TetrisPieceType::None == iType ) || ( TetrisPieceType::Count == iType ) )
    return false;

  const std::array<Vec2i, 4> & minos = TetrisPiece::GetMinos(iType, iRotation);
  for ( const Vec2i & mino : minos )
  {
    const int x = iOrigin.x + mino.x;
    const int y = iOrigin.y + mino.y;

    if ( ( x < 0 ) || ( x >= S_Width ) || ( y < 0 ) )
      return false;

    if ( ( y < S_TotalHeight ) && ( TetrisPieceType::None != _Grid[CellIndex(x, y)] ) )
      return false;
  }

  return true;
}

// ----------------------------------------------------------------------------
// LockPiece
// ----------------------------------------------------------------------------
bool TetrisMatrix::LockPiece( TetrisPieceType iType, TetrisRotation iRotation, const Vec2i & iOrigin, bool & oAboveSkyline )
{
  if ( !CanPieceFit(iType, iRotation, iOrigin) )
    return false;

  const std::array<Vec2i, 4> & minos = TetrisPiece::GetMinos(iType, iRotation);
  int minosAboveSkyline = 0;

  for ( const Vec2i & mino : minos )
  {
    const int x = iOrigin.x + mino.x;
    const int y = iOrigin.y + mino.y;

    if ( y >= S_Skyline )
      minosAboveSkyline++;

    if ( IsInside(x, y) )
      _Grid[CellIndex(x, y)] = iType;
  }

  // Guideline lock-out: if all minos locked above visible skyline row (y >= 20)
  oAboveSkyline = ( 4 == minosAboveSkyline );
  return true;
}

// ----------------------------------------------------------------------------
// ClearFullLines
// ----------------------------------------------------------------------------
int TetrisMatrix::ClearFullLines( std::vector<int> & oClearedRows )
{
  oClearedRows.clear();

  int targetY = 0;
  for ( int srcY = 0; srcY < S_TotalHeight; ++srcY )
  {
    bool isRowFull = true;
    for ( int x = 0; x < S_Width; ++x )
    {
      if ( TetrisPieceType::None == _Grid[CellIndex(x, srcY)] )
      {
        isRowFull = false;
        break;
      }
    }

    if ( isRowFull )
    {
      oClearedRows.push_back(srcY);
    }
    else
    {
      if ( targetY != srcY )
      {
        for ( int x = 0; x < S_Width; ++x )
          _Grid[CellIndex(x, targetY)] = _Grid[CellIndex(x, srcY)];
      }
      targetY++;
    }
  }

  // Clear remaining top rows
  for ( int y = targetY; y < S_TotalHeight; ++y )
  {
    for ( int x = 0; x < S_Width; ++x )
      _Grid[CellIndex(x, y)] = TetrisPieceType::None;
  }

  return static_cast<int>(oClearedRows.size());
}

// ----------------------------------------------------------------------------
// IsEmpty
// ----------------------------------------------------------------------------
bool TetrisMatrix::IsEmpty() const
{
  for ( TetrisPieceType cell : _Grid )
  {
    if ( TetrisPieceType::None != cell )
      return false;
  }
  return true;
}

}
