#include "TetrisPiece.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local data tables
// ----------------------------------------------------------------------------
static const std::array<Vec2i, 4> s_EmptyMinos = { Vec2i(0), Vec2i(0), Vec2i(0), Vec2i(0) };

// 3x3 Minos for J, L, S, T, Z; 4x4 Minos for I, O
static const std::array<Vec2i, 4> s_PieceMinos[(int)TetrisPieceType::Count][4] =
{
  // None
  { s_EmptyMinos, s_EmptyMinos, s_EmptyMinos, s_EmptyMinos },

  // I
  {
    // Spawn (0)
    { Vec2i(0, 2), Vec2i(1, 2), Vec2i(2, 2), Vec2i(3, 2) },
    // Right (R)
    { Vec2i(2, 3), Vec2i(2, 2), Vec2i(2, 1), Vec2i(2, 0) },
    // Flip (2)
    { Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1), Vec2i(3, 1) },
    // Left (L)
    { Vec2i(1, 3), Vec2i(1, 2), Vec2i(1, 1), Vec2i(1, 0) }
  },

  // J
  {
    // Spawn (0)
    { Vec2i(0, 2), Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1) },
    // Right (R)
    { Vec2i(1, 2), Vec2i(1, 1), Vec2i(1, 0), Vec2i(2, 2) },
    // Flip (2)
    { Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1), Vec2i(2, 0) },
    // Left (L)
    { Vec2i(0, 0), Vec2i(1, 0), Vec2i(1, 1), Vec2i(1, 2) }
  },

  // L
  {
    // Spawn (0)
    { Vec2i(2, 2), Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1) },
    // Right (R)
    { Vec2i(1, 2), Vec2i(1, 1), Vec2i(1, 0), Vec2i(2, 0) },
    // Flip (2)
    { Vec2i(0, 0), Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1) },
    // Left (L)
    { Vec2i(0, 2), Vec2i(1, 2), Vec2i(1, 1), Vec2i(1, 0) }
  },

  // O
  {
    // Spawn (0)
    { Vec2i(1, 2), Vec2i(2, 2), Vec2i(1, 1), Vec2i(2, 1) },
    // Right (R)
    { Vec2i(1, 2), Vec2i(2, 2), Vec2i(1, 1), Vec2i(2, 1) },
    // Flip (2)
    { Vec2i(1, 2), Vec2i(2, 2), Vec2i(1, 1), Vec2i(2, 1) },
    // Left (L)
    { Vec2i(1, 2), Vec2i(2, 2), Vec2i(1, 1), Vec2i(2, 1) }
  },

  // S
  {
    // Spawn (0)
    { Vec2i(1, 2), Vec2i(2, 2), Vec2i(0, 1), Vec2i(1, 1) },
    // Right (R)
    { Vec2i(1, 2), Vec2i(1, 1), Vec2i(2, 1), Vec2i(2, 0) },
    // Flip (2)
    { Vec2i(1, 1), Vec2i(2, 1), Vec2i(0, 0), Vec2i(1, 0) },
    // Left (L)
    { Vec2i(0, 2), Vec2i(0, 1), Vec2i(1, 1), Vec2i(1, 0) }
  },

  // T
  {
    // Spawn (0)
    { Vec2i(1, 2), Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1) },
    // Right (R)
    { Vec2i(1, 2), Vec2i(1, 1), Vec2i(2, 1), Vec2i(1, 0) },
    // Flip (2)
    { Vec2i(0, 1), Vec2i(1, 1), Vec2i(2, 1), Vec2i(1, 0) },
    // Left (L)
    { Vec2i(1, 2), Vec2i(0, 1), Vec2i(1, 1), Vec2i(1, 0) }
  },

  // Z
  {
    // Spawn (0)
    { Vec2i(0, 2), Vec2i(1, 2), Vec2i(1, 1), Vec2i(2, 1) },
    // Right (R)
    { Vec2i(2, 2), Vec2i(1, 1), Vec2i(2, 1), Vec2i(1, 0) },
    // Flip (2)
    { Vec2i(0, 1), Vec2i(1, 1), Vec2i(1, 0), Vec2i(2, 0) },
    // Left (L)
    { Vec2i(1, 2), Vec2i(0, 1), Vec2i(1, 1), Vec2i(0, 0) }
  }
};

// ----------------------------------------------------------------------------
// GetMinos
// ----------------------------------------------------------------------------
const std::array<Vec2i, 4> & TetrisPiece::GetMinos( TetrisPieceType iType, TetrisRotation iRotation )
{
  const int typeIdx = (int)iType;
  if ( ( typeIdx <= 0 ) || ( typeIdx >= (int)TetrisPieceType::Count ) )
    return s_EmptyMinos;

  const int rotIdx = (int)iRotation & 3;
  return s_PieceMinos[typeIdx][rotIdx];
}

// ----------------------------------------------------------------------------
// GetColor
// ----------------------------------------------------------------------------
Vec3 TetrisPiece::GetColor( TetrisPieceType iType )
{
  switch ( iType )
  {
    case TetrisPieceType::I: return Vec3(0.00f, 0.94f, 0.94f); // Cyan
    case TetrisPieceType::J: return Vec3(0.08f, 0.28f, 0.95f); // Blue
    case TetrisPieceType::L: return Vec3(0.95f, 0.50f, 0.05f); // Orange
    case TetrisPieceType::O: return Vec3(0.95f, 0.90f, 0.05f); // Yellow
    case TetrisPieceType::S: return Vec3(0.12f, 0.88f, 0.16f); // Green
    case TetrisPieceType::T: return Vec3(0.65f, 0.10f, 0.90f); // Purple
    case TetrisPieceType::Z: return Vec3(0.95f, 0.10f, 0.12f); // Red
    default:                 return Vec3(0.50f, 0.50f, 0.50f);
  }
}

// ----------------------------------------------------------------------------
// GetName
// ----------------------------------------------------------------------------
const char * TetrisPiece::GetName( TetrisPieceType iType )
{
  switch ( iType )
  {
    case TetrisPieceType::I: return "I";
    case TetrisPieceType::J: return "J";
    case TetrisPieceType::L: return "L";
    case TetrisPieceType::O: return "O";
    case TetrisPieceType::S: return "S";
    case TetrisPieceType::T: return "T";
    case TetrisPieceType::Z: return "Z";
    default:                 return "None";
  }
}

// ----------------------------------------------------------------------------
// GetSpawnOrigin
// ----------------------------------------------------------------------------
Vec2i TetrisPiece::GetSpawnOrigin( TetrisPieceType iType )
{
  // Guideline spawn: pieces spawn centered horizontally and in rows 20-21 (skyline is at y = 20)
  if ( ( TetrisPieceType::I == iType ) || ( TetrisPieceType::O == iType ) )
    return Vec2i(3, 19);
  return Vec2i(3, 19);
}

// ----------------------------------------------------------------------------
// GetBoundingBoxSize
// ----------------------------------------------------------------------------
int TetrisPiece::GetBoundingBoxSize( TetrisPieceType iType )
{
  if ( ( TetrisPieceType::I == iType ) || ( TetrisPieceType::O == iType ) )
    return 4;
  return 3;
}

// ----------------------------------------------------------------------------
// RotateCW / CCW / 180
// ----------------------------------------------------------------------------
TetrisRotation TetrisPiece::RotateCW( TetrisRotation iRotation )
{
  return static_cast<TetrisRotation>(( (int)iRotation + 1 ) & 3);
}

TetrisRotation TetrisPiece::RotateCCW( TetrisRotation iRotation )
{
  return static_cast<TetrisRotation>(( (int)iRotation + 3 ) & 3);
}

TetrisRotation TetrisPiece::Rotate180( TetrisRotation iRotation )
{
  return static_cast<TetrisRotation>(( (int)iRotation + 2 ) & 3);
}

// ----------------------------------------------------------------------------
// GetSRSKickOffsets
// ----------------------------------------------------------------------------
bool TetrisPiece::GetSRSKickOffsets( TetrisPieceType iType,
                                    TetrisRotation iFrom,
                                    TetrisRotation iTo,
                                    std::array<Vec2i, 5> & oKicks )
{
  if ( ( TetrisPieceType::O == iType ) || ( TetrisPieceType::None == iType ) )
  {
    for ( int i = 0; i < 5; ++i )
      oKicks[i] = Vec2i(0, 0);
    return true;
  }

  const int fromIdx = (int)iFrom & 3;
  const int toIdx = (int)iTo & 3;

  if ( TetrisPieceType::I == iType )
  {
    // Standard I wall-kick tables (+x is right, +y is UP)
    if ( ( 0 == fromIdx ) && ( 1 == toIdx ) ) // 0 -> R
      oKicks = { Vec2i(0, 0), Vec2i(-2, 0), Vec2i( 1, 0), Vec2i(-2,-1), Vec2i( 1, 2) };
    else if ( ( 1 == fromIdx ) && ( 0 == toIdx ) ) // R -> 0
      oKicks = { Vec2i(0, 0), Vec2i( 2, 0), Vec2i(-1, 0), Vec2i( 2, 1), Vec2i(-1,-2) };
    else if ( ( 1 == fromIdx ) && ( 2 == toIdx ) ) // R -> 2
      oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i( 2, 0), Vec2i(-1, 2), Vec2i( 2,-1) };
    else if ( ( 2 == fromIdx ) && ( 1 == toIdx ) ) // 2 -> R
      oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i(-2, 0), Vec2i( 1,-2), Vec2i(-2, 1) };
    else if ( ( 2 == fromIdx ) && ( 3 == toIdx ) ) // 2 -> L
      oKicks = { Vec2i(0, 0), Vec2i( 2, 0), Vec2i(-1, 0), Vec2i( 2, 1), Vec2i(-1,-2) };
    else if ( ( 3 == fromIdx ) && ( 2 == toIdx ) ) // L -> 2
      oKicks = { Vec2i(0, 0), Vec2i(-2, 0), Vec2i( 1, 0), Vec2i(-2,-1), Vec2i( 1, 2) };
    else if ( ( 3 == fromIdx ) && ( 0 == toIdx ) ) // L -> 0
      oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i(-2, 0), Vec2i( 1,-2), Vec2i(-2, 1) };
    else if ( ( 0 == fromIdx ) && ( 3 == toIdx ) ) // 0 -> L
      oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i( 2, 0), Vec2i(-1, 2), Vec2i( 2,-1) };
    // 180 rotations
    else if ( ( 0 == fromIdx ) && ( 2 == toIdx ) ) // 0 -> 2
      oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i( 2, 0), Vec2i(-1, 1), Vec2i( 2,-2) };
    else if ( ( 2 == fromIdx ) && ( 0 == toIdx ) ) // 2 -> 0
      oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i(-2, 0), Vec2i( 1,-1), Vec2i(-2, 2) };
    else if ( ( 1 == fromIdx ) && ( 3 == toIdx ) ) // R -> L
      oKicks = { Vec2i(0, 0), Vec2i( 0, 1), Vec2i( 0,-2), Vec2i( 1, 1), Vec2i(-2,-2) };
    else if ( ( 3 == fromIdx ) && ( 1 == toIdx ) ) // L -> R
      oKicks = { Vec2i(0, 0), Vec2i( 0,-1), Vec2i( 0, 2), Vec2i(-1,-1), Vec2i( 2, 2) };
    else
    {
      for ( int i = 0; i < 5; ++i ) oKicks[i] = Vec2i(0, 0);
    }
    return true;
  }

  // Standard JLSTZ wall-kick tables
  if ( ( 0 == fromIdx ) && ( 1 == toIdx ) ) // 0 -> R
    oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i(-1, 1), Vec2i( 0,-2), Vec2i(-1,-2) };
  else if ( ( 1 == fromIdx ) && ( 0 == toIdx ) ) // R -> 0
    oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i( 1,-1), Vec2i( 0, 2), Vec2i( 1, 2) };
  else if ( ( 1 == fromIdx ) && ( 2 == toIdx ) ) // R -> 2
    oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i( 1,-1), Vec2i( 0, 2), Vec2i( 1, 2) };
  else if ( ( 2 == fromIdx ) && ( 1 == toIdx ) ) // 2 -> R
    oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i(-1, 1), Vec2i( 0,-2), Vec2i(-1,-2) };
  else if ( ( 2 == fromIdx ) && ( 3 == toIdx ) ) // 2 -> L
    oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i( 1, 1), Vec2i( 0,-2), Vec2i( 1,-2) };
  else if ( ( 3 == fromIdx ) && ( 2 == toIdx ) ) // L -> 2
    oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i(-1,-1), Vec2i( 0, 2), Vec2i(-1, 2) };
  else if ( ( 3 == fromIdx ) && ( 0 == toIdx ) ) // L -> 0
    oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i(-1,-1), Vec2i( 0, 2), Vec2i(-1, 2) };
  else if ( ( 0 == fromIdx ) && ( 3 == toIdx ) ) // 0 -> L
    oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i( 1, 1), Vec2i( 0,-2), Vec2i( 1,-2) };
  // 180 rotations
  else if ( ( 0 == fromIdx ) && ( 2 == toIdx ) ) // 0 -> 2
    oKicks = { Vec2i(0, 0), Vec2i( 0, 1), Vec2i( 1, 1), Vec2i(-1, 1), Vec2i( 0,-1) };
  else if ( ( 2 == fromIdx ) && ( 0 == toIdx ) ) // 2 -> 0
    oKicks = { Vec2i(0, 0), Vec2i( 0,-1), Vec2i(-1,-1), Vec2i( 1,-1), Vec2i( 0, 1) };
  else if ( ( 1 == fromIdx ) && ( 3 == toIdx ) ) // R -> L
    oKicks = { Vec2i(0, 0), Vec2i( 1, 0), Vec2i( 1, 2), Vec2i( 1, 1), Vec2i( 0, 2) };
  else if ( ( 3 == fromIdx ) && ( 1 == toIdx ) ) // L -> R
    oKicks = { Vec2i(0, 0), Vec2i(-1, 0), Vec2i(-1, 2), Vec2i(-1, 1), Vec2i( 0, 2) };
  else
  {
    for ( int i = 0; i < 5; ++i ) oKicks[i] = Vec2i(0, 0);
  }

  return true;
}

}
