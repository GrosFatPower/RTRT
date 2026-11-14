#include "FpsDecals.h"

#include "FpsGame.h"

#include <algorithm>
#include <cmath>

namespace FPS
{

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int FpsDecals::Initialize( const FpsGameSettings & iSettings )
{
  return Reset(iSettings);
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
int FpsDecals::Reset( const FpsGameSettings & iSettings )
{
  const int poolSize = std::max(1, iSettings._MaxDecals);
  _Decals.assign(poolSize, FpsDecal());
  _NextSlotIndex = 0;
  _SpawnCounter = 0;
  _Dirty = true;
  return 0;
}

// ----------------------------------------------------------------------------
// ResizePool
// ----------------------------------------------------------------------------
int FpsDecals::ResizePool( const FpsGameSettings & iSettings )
{
  const int poolSize = std::max(1, iSettings._MaxDecals);
  if ( static_cast<int>(_Decals.size()) == poolSize )
    return 0;

  _Decals.resize(poolSize, FpsDecal());
  _NextSlotIndex = _NextSlotIndex % poolSize;
  _Dirty = true;
  return 0;
}

// ----------------------------------------------------------------------------
// Clear
// ----------------------------------------------------------------------------
void FpsDecals::Clear()
{
  for ( FpsDecal & decal : _Decals )
    decal = FpsDecal();

  _NextSlotIndex = 0;
  _SpawnCounter = 0;
  _Dirty = true;
}

// ----------------------------------------------------------------------------
// Spawn
// ----------------------------------------------------------------------------
void FpsDecals::Spawn( const FpsGameSettings & iSettings, const Vec3 & iPosition, const Vec3 & iNormal )
{
  if ( _Decals.empty() )
    ResizePool(iSettings);
  if ( _Decals.empty() )
    return;

  const int poolSize = static_cast<int>(_Decals.size());
  const int slotIndex = ( ( _NextSlotIndex >= 0 ) && ( _NextSlotIndex < poolSize ) ) ? _NextSlotIndex : 0;
  _NextSlotIndex = ( slotIndex + 1 ) % poolSize;

  FpsDecal & decal = _Decals[slotIndex];
  decal._Active = true;
  decal._Position = iPosition;

  const float normalLen = glm::length(iNormal);
  decal._Normal = ( normalLen > EPSILON ) ? ( iNormal / normalLen ) : Vec3(0.f, 1.f, 0.f);

  // Golden-ratio angle sequence gives well-distributed roll angles without RNG state dependencies
  const float angleFraction = std::fmod(static_cast<float>(_SpawnCounter) * 0.61803398875f, 1.f);
  decal._Rotation = angleFraction * 2.f * static_cast<float>(M_PI);
  decal._Size = std::max(0.01f, iSettings._DecalSize);
  decal._Age = 0.f;
  decal._MaxLifetime = std::max(0.f, iSettings._DecalLifetime);
  decal._LayerOrder = _SpawnCounter++;

  _Dirty = true;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int FpsDecals::Update( float iDeltaTime, const FpsGameSettings & iSettings )
{
  (void)iSettings;
  const float dt = std::clamp(iDeltaTime, 0.f, 0.05f);
  if ( dt <= 0.f )
    return 0;

  for ( FpsDecal & decal : _Decals )
  {
    if ( !decal._Active )
      continue;

    decal._Age += dt;
    if ( ( decal._MaxLifetime > 0.f ) && ( decal._Age >= decal._MaxLifetime ) )
    {
      decal._Active = false;
      _Dirty = true;
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// GetActiveCount
// ----------------------------------------------------------------------------
int FpsDecals::GetActiveCount() const
{
  int count = 0;
  for ( const FpsDecal & decal : _Decals )
  {
    if ( decal._Active )
      ++count;
  }
  return count;
}

// ----------------------------------------------------------------------------
// ConsumeDirty
// ----------------------------------------------------------------------------
bool FpsDecals::ConsumeDirty()
{
  const bool dirty = _Dirty;
  _Dirty = false;
  return dirty;
}

}
