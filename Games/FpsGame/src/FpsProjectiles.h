#ifndef _FpsProjectiles_
#define _FpsProjectiles_

#include "FpsCollision.h"
#include "MathUtil.h"

#include <vector>

namespace FPS
{
using namespace RTRT;

struct FpsGameSettings;
struct FpsPlayer;
struct FpsSceneObject;

struct FpsProjectile
{
  bool  _Active = false;
  bool  _HitscanImpact = false;
  Vec3  _Position = Vec3(0.f);
  Vec3  _Velocity = Vec3(0.f);
  float _Age = 0.f;
  float _MaxLifetime = 0.f;
  float _RadiusScale = 1.f;
  float _LastBounceAge = -1.f;
};

struct FpsProjectilesUpdateContext
{
  FpsProjectilesUpdateContext( const FpsGameSettings & iSettings,
                               const FpsPlayer & iPlayer,
                               const std::vector<FpsSceneObject> & iObjects,
                               const std::vector<FpsCollisionObb> & iPropColliders )
  : _Settings(iSettings)
  , _Player(iPlayer)
  , _Objects(iObjects)
  , _PropColliders(iPropColliders)
  {}

  const FpsGameSettings & _Settings;
  const FpsPlayer & _Player;
  const std::vector<FpsSceneObject> & _Objects;
  const std::vector<FpsCollisionObb> & _PropColliders;
};

class FpsProjectiles
{
public:
  int Initialize( const FpsGameSettings & iSettings );
  int Reset( const FpsGameSettings & iSettings );
  int ResizePool( const FpsGameSettings & iSettings );
  void Clear();

  bool RequestFire( const FpsGameSettings & iSettings, const FpsPlayer & iPlayer );
  void SpawnHitscanImpact( const FpsGameSettings & iSettings, const Vec3 & iPosition, const Vec3 & iNormal );
  int Update( float iRealDeltaTime, float iSimDeltaTime, const FpsProjectilesUpdateContext & iContext );

  const std::vector<FpsProjectile> & GetProjectiles() const { return _Projectiles; }
  int GetAmmo() const { return _Ammo; }
  int GetActiveCount() const;
  bool ConsumeDirty();
  void ConsumeSoundEvents( std::vector<Vec3> & oBouncePositions, std::vector<Vec3> & oExplosionPositions );

protected:
  void Fire( const FpsGameSettings & iSettings, const FpsPlayer & iPlayer );
  void UpdateActiveProjectiles( float iDeltaTime, const FpsProjectilesUpdateContext & iContext );
  void MoveProjectileAxis( FpsProjectile & ioProjectile, int iAxis, float iDelta, const FpsProjectilesUpdateContext & iContext );

protected:
  std::vector<FpsProjectile> _Projectiles;
  std::vector<Vec3>          _BounceSoundEvents;
  std::vector<Vec3>          _ExplosionSoundEvents;
  float                      _CooldownTimer = 0.f;
  float                      _AmmoRefillTimer = 0.f;
  int                        _Ammo = 32;
  int                        _PendingShots = 0;
  bool                       _Dirty = false;
};

}

#endif /* _FpsProjectiles_ */
