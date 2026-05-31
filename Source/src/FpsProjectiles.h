#ifndef _FpsProjectiles_
#define _FpsProjectiles_

#include "FpsCollision.h"
#include "MathUtil.h"

#include <vector>

namespace RTRT
{

struct FpsGameSettings;
struct FpsPlayer;
struct FpsSceneObject;

struct FpsProjectile
{
  bool  _Active = false;
  Vec3  _Position = Vec3(0.f);
  Vec3  _Velocity = Vec3(0.f);
  float _Age = 0.f;
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

  void RequestFire( const FpsGameSettings & iSettings, const FpsPlayer & iPlayer );
  int Update( float iRealDeltaTime, float iSimDeltaTime, const FpsProjectilesUpdateContext & iContext );

  const std::vector<FpsProjectile> & GetProjectiles() const { return _Projectiles; }
  int GetAmmo() const { return _Ammo; }
  int GetActiveCount() const;
  bool ConsumeDirty();

protected:
  void Fire( const FpsGameSettings & iSettings, const FpsPlayer & iPlayer );
  void UpdateActiveProjectiles( float iDeltaTime, const FpsProjectilesUpdateContext & iContext );
  void MoveProjectileAxis( FpsProjectile & ioProjectile, int iAxis, float iDelta, const FpsProjectilesUpdateContext & iContext );

protected:
  std::vector<FpsProjectile> _Projectiles;
  float                      _CooldownTimer = 0.f;
  float                      _AmmoRefillTimer = 0.f;
  int                        _Ammo = 32;
  int                        _PendingShots = 0;
  bool                       _Dirty = false;
};

}

#endif /* _FpsProjectiles_ */
