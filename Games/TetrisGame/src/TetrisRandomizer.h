#ifndef _TetrisRandomizer_
#define _TetrisRandomizer_

#include "TetrisTypes.h"
#include <deque>
#include <random>

namespace RTRT
{

class TetrisRandomizer
{
public:
  TetrisRandomizer( unsigned int iSeed = 0 );
  virtual ~TetrisRandomizer();

  void Reset( unsigned int iSeed = 0 );
  TetrisPieceType Next();
  TetrisPieceType Peek( int iIndex = 0 ) const;
  int GetQueueSize() const { return static_cast<int>(_Queue.size()); }

protected:
  void RefillBag();

protected:
  std::mt19937                _RNG;
  std::deque<TetrisPieceType> _Queue;
};

}

#endif /* _TetrisRandomizer_ */
