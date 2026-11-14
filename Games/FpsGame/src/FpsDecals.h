#ifndef _FpsDecals_
#define _FpsDecals_

#include "MathUtil.h"

#include <vector>

namespace FPS
{
using namespace RTRT;

struct FpsGameSettings;

struct FpsDecal
{
  bool  _Active = false;
  Vec3  _Position = Vec3(0.f);
  Vec3  _Normal = Vec3(0.f, 0.f, 1.f);
  float _Rotation = 0.f;
  float _Size = 0.18f;
  float _Age = 0.f;
  float _MaxLifetime = 0.f;
  int   _LayerOrder = 0;
};

class FpsDecals
{
public:
  int Initialize( const FpsGameSettings & iSettings );
  int Reset( const FpsGameSettings & iSettings );
  int ResizePool( const FpsGameSettings & iSettings );
  void Clear();

  void Spawn( const FpsGameSettings & iSettings, const Vec3 & iPosition, const Vec3 & iNormal );
  int Update( float iDeltaTime, const FpsGameSettings & iSettings );

  const std::vector<FpsDecal> & GetDecals() const { return _Decals; }
  int GetActiveCount() const;
  bool ConsumeDirty();

protected:
  std::vector<FpsDecal> _Decals;
  int                   _NextSlotIndex = 0;
  int                   _SpawnCounter = 0;
  bool                  _Dirty = false;
};

}

#endif /* _FpsDecals_ */
