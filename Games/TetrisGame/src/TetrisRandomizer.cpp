#include "TetrisRandomizer.h"
#include <algorithm>
#include <chrono>

namespace RTRT
{

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
TetrisRandomizer::TetrisRandomizer( unsigned int iSeed )
{
  Reset(iSeed);
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
TetrisRandomizer::~TetrisRandomizer()
{
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void TetrisRandomizer::Reset( unsigned int iSeed )
{
  if ( 0 == iSeed )
  {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    iSeed = static_cast<unsigned int>(now & 0xFFFFFFFF);
  }

  _RNG.seed(iSeed);
  _Queue.clear();
  RefillBag();
  RefillBag(); // Ensure at least 14 pieces for lookahead
}

// ----------------------------------------------------------------------------
// Next
// ----------------------------------------------------------------------------
TetrisPieceType TetrisRandomizer::Next()
{
  if ( _Queue.size() < 7 )
    RefillBag();

  if ( _Queue.empty() )
    return TetrisPieceType::None;

  const TetrisPieceType nextPiece = _Queue.front();
  _Queue.pop_front();
  return nextPiece;
}

// ----------------------------------------------------------------------------
// Peek
// ----------------------------------------------------------------------------
TetrisPieceType TetrisRandomizer::Peek( int iIndex ) const
{
  if ( ( iIndex < 0 ) || ( iIndex >= static_cast<int>(_Queue.size()) ) )
    return TetrisPieceType::None;

  return _Queue[iIndex];
}

// ----------------------------------------------------------------------------
// RefillBag
// ----------------------------------------------------------------------------
void TetrisRandomizer::RefillBag()
{
  std::vector<TetrisPieceType> bag =
  {
    TetrisPieceType::I,
    TetrisPieceType::J,
    TetrisPieceType::L,
    TetrisPieceType::O,
    TetrisPieceType::S,
    TetrisPieceType::T,
    TetrisPieceType::Z
  };

  std::shuffle(bag.begin(), bag.end(), _RNG);
  for ( TetrisPieceType piece : bag )
    _Queue.push_back(piece);
}

}
