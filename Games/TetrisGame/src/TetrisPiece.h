#ifndef _TetrisPiece_
#define _TetrisPiece_

#include "TetrisTypes.h"
#include <array>
#include <string>

namespace RTRT
{

class TetrisPiece
{
public:
  static const std::array<Vec2i, 4> & GetMinos( TetrisPieceType iType, TetrisRotation iRotation );
  static Vec3 GetColor( TetrisPieceType iType );
  static const char * GetName( TetrisPieceType iType );
  static Vec2i GetSpawnOrigin( TetrisPieceType iType );
  static int GetBoundingBoxSize( TetrisPieceType iType );

  static TetrisRotation RotateCW( TetrisRotation iRotation );
  static TetrisRotation RotateCCW( TetrisRotation iRotation );
  static TetrisRotation Rotate180( TetrisRotation iRotation );

  static bool GetSRSKickOffsets( TetrisPieceType iType,
                                 TetrisRotation iFrom,
                                 TetrisRotation iTo,
                                 std::array<Vec2i, 5> & oKicks );
};

}

#endif /* _TetrisPiece_ */
