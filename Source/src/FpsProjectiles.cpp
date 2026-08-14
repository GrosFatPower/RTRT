#include "FpsProjectiles.h"

#include "FpsGame.h"

#include <algorithm>
#include <cmath>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static Vec3 ObjectCollisionHalfExtents( const FpsSceneObject & iObject )
{
  Vec3 halfExtents = iObject._HalfExtents;
  MathUtil::Maximize(halfExtents, Vec3(0.05f));
  return halfExtents;
}

static Vec3 PlayerForward( const FpsPlayer & iPlayer )
{
  const float yawRad = MathUtil::ToRadians(iPlayer._Yaw);
  const float pitchRad = MathUtil::ToRadians(iPlayer._Pitch);
  Vec3 forward(std::cos(yawRad) * std::cos(pitchRad),
               std::sin(pitchRad),
               std::sin(yawRad) * std::cos(pitchRad));
  return glm::normalize(forward);
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int FpsProjectiles::Initialize( const FpsGameSettings & iSettings )
{
  return Reset(iSettings);
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
int FpsProjectiles::Reset( const FpsGameSettings & iSettings )
{
  ResizePool(iSettings);
  _CooldownTimer = 0.f;
  _AmmoRefillTimer = 0.f;
  _Ammo = std::max(0, iSettings._MaxProjectileAmmo);
  _PendingShots = 0;
  Clear();
  return 0;
}

// ----------------------------------------------------------------------------
// ResizePool
// ----------------------------------------------------------------------------
int FpsProjectiles::ResizePool( const FpsGameSettings & iSettings )
{
  const int maxProjectiles = std::max(1, iSettings._MaxProjectiles);
  if ( static_cast<int>(_Projectiles.size()) == maxProjectiles )
    return 0;

  _Projectiles.resize(maxProjectiles);
  _Dirty = true;
  return 0;
}

// ----------------------------------------------------------------------------
// Clear
// ----------------------------------------------------------------------------
void FpsProjectiles::Clear()
{
  for ( FpsProjectile & projectile : _Projectiles )
  {
    projectile._Active = false;
    projectile._Position = Vec3(0.f);
    projectile._Velocity = Vec3(0.f);
    projectile._Age = 0.f;
  }
  _Dirty = true;
}

// ----------------------------------------------------------------------------
// RequestFire
// ----------------------------------------------------------------------------
void FpsProjectiles::RequestFire( const FpsGameSettings & iSettings, const FpsPlayer & )
{
  const float cooldown = std::max(0.001f, iSettings._ProjectileCooldown);
  if ( ( _Ammo > 0 ) && ( _CooldownTimer > cooldown ) )
  {
    _PendingShots += 1;
    _Ammo--;
    _CooldownTimer = 0.f;
  }
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int FpsProjectiles::Update( float iRealDeltaTime, float iSimDeltaTime, const FpsProjectilesUpdateContext & iContext )
{
  if ( ( iRealDeltaTime <= 0.f ) && ( iSimDeltaTime <= 0.f ) )
    return 0;

  if ( iRealDeltaTime > 0.f )
  {
    ResizePool(iContext._Settings);
    const int maxAmmo = std::max(0, iContext._Settings._MaxProjectileAmmo);
    _Ammo = MathUtil::Clamp(_Ammo, 0, maxAmmo);

    if ( _Ammo < maxAmmo )
    {
      const float refillTime = std::max(0.001f, iContext._Settings._ProjectileAmmoRefillTime);
      _AmmoRefillTimer += iRealDeltaTime;
      while ( ( _AmmoRefillTimer >= refillTime ) && ( _Ammo < maxAmmo ) )
      {
        _AmmoRefillTimer -= refillTime;
        _Ammo++;
      }
    }
    else
      _AmmoRefillTimer = 0.f;

    _CooldownTimer += iRealDeltaTime;
  }

  if ( iSimDeltaTime <= 0.f )
    return 0;

  while ( _PendingShots > 0 )
  {
    Fire(iContext._Settings, iContext._Player);
    _PendingShots--;
  }

  UpdateActiveProjectiles(iSimDeltaTime, iContext);
  return 0;
}

// ----------------------------------------------------------------------------
// GetActiveCount
// ----------------------------------------------------------------------------
int FpsProjectiles::GetActiveCount() const
{
  int count = 0;
  for ( const FpsProjectile & projectile : _Projectiles )
  {
    if ( projectile._Active )
      count++;
  }
  return count;
}

// ----------------------------------------------------------------------------
// ConsumeDirty
// ----------------------------------------------------------------------------
bool FpsProjectiles::ConsumeDirty()
{
  const bool dirty = _Dirty;
  _Dirty = false;
  return dirty;
}

// ----------------------------------------------------------------------------
// Fire
// ----------------------------------------------------------------------------
void FpsProjectiles::Fire( const FpsGameSettings & iSettings, const FpsPlayer & iPlayer )
{
  if ( _Projectiles.empty() )
    ResizePool(iSettings);

  int projectileID = -1;
  float oldestAge = -1.f;
  for ( int i = 0; i < static_cast<int>(_Projectiles.size()); ++i )
  {
    if ( !_Projectiles[i]._Active )
    {
      projectileID = i;
      break;
    }

    if ( _Projectiles[i]._Age > oldestAge )
    {
      oldestAge = _Projectiles[i]._Age;
      projectileID = i;
    }
  }

  if ( projectileID < 0 )
    return;

  const float radius = std::max(0.02f, iSettings._ProjectileRadius);
  const Vec3 forward = PlayerForward(iPlayer);
  FpsProjectile & projectile = _Projectiles[projectileID];
  projectile._Active = true;
  projectile._Position = iPlayer.EyePosition(iSettings) + forward * (iSettings._PlayerRadius + radius + 0.08f);
  projectile._Velocity = iPlayer._Velocity + forward * std::max(0.f, iSettings._ProjectileSpeed);
  projectile._Age = 0.f;

  _Dirty = true;
}

// ----------------------------------------------------------------------------
// UpdateActiveProjectiles
// ----------------------------------------------------------------------------
void FpsProjectiles::UpdateActiveProjectiles( float iDeltaTime, const FpsProjectilesUpdateContext & iContext )
{
  const float radius = std::max(0.02f, iContext._Settings._ProjectileRadius);
  const float maxMovePerStep = std::max(0.05f, radius * 0.75f);

  for ( FpsProjectile & projectile : _Projectiles )
  {
    if ( !projectile._Active )
      continue;

    projectile._Age += iDeltaTime;
    if ( projectile._Age >= std::max(0.1f, iContext._Settings._ProjectileLifetime) )
    {
      projectile._Active = false;
      _Dirty = true;
      continue;
    }

    const float moveLen = glm::length(projectile._Velocity) * iDeltaTime;
    const int nbSteps = std::max(1, std::min(16, static_cast<int>(std::ceil(moveLen / maxMovePerStep))));
    const float stepDt = iDeltaTime / static_cast<float>(nbSteps);

    for ( int step = 0; step < nbSteps; ++step )
    {
      projectile._Velocity.y -= std::max(0.f, iContext._Settings._ProjectileGravity) * stepDt;
      MoveProjectileAxis(projectile, 0, projectile._Velocity.x * stepDt, iContext);
      MoveProjectileAxis(projectile, 1, projectile._Velocity.y * stepDt, iContext);
      MoveProjectileAxis(projectile, 2, projectile._Velocity.z * stepDt, iContext);
    }

    _Dirty = true;
  }
}

// ----------------------------------------------------------------------------
// MoveProjectileAxis
// ----------------------------------------------------------------------------
void FpsProjectiles::MoveProjectileAxis( FpsProjectile & ioProjectile, int iAxis, float iDelta, const FpsProjectilesUpdateContext & iContext )
{
  if ( fabs(iDelta) <= EPSILON )
    return;

  ioProjectile._Position[iAxis] += iDelta;

  const float radius = std::max(0.02f, iContext._Settings._ProjectileRadius);
  const float bounciness = MathUtil::Clamp(iContext._Settings._ProjectileBounciness, 0.f, 1.f);

  for ( const FpsSceneObject & object : iContext._Objects )
  {
    if ( !object._Collidable )
      continue;

    const Vec3 delta = ioProjectile._Position - object._Center;
    const Vec3 sumHalf = ObjectCollisionHalfExtents(object) + Vec3(radius);
    if ( ( std::abs(delta.x) >= sumHalf.x )
      || ( std::abs(delta.y) >= sumHalf.y )
      || ( std::abs(delta.z) >= sumHalf.z ) )
      continue;

    const Vec3 overlap = sumHalf - Vec3(std::abs(delta.x), std::abs(delta.y), std::abs(delta.z));
    int minAxis = 0;
    if ( overlap.y < overlap[minAxis] )
      minAxis = 1;
    if ( overlap.z < overlap[minAxis] )
      minAxis = 2;
    if ( minAxis != iAxis )
      continue;

    if ( iDelta > 0.f )
      ioProjectile._Position[iAxis] -= overlap[iAxis];
    else
      ioProjectile._Position[iAxis] += overlap[iAxis];

    ioProjectile._Velocity[iAxis] = -ioProjectile._Velocity[iAxis] * bounciness;
    if ( std::abs(ioProjectile._Velocity[iAxis]) < 0.15f )
      ioProjectile._Velocity[iAxis] = 0.f;

    for ( int axis = 0; axis < 3; ++axis )
    {
      if ( axis != iAxis )
        ioProjectile._Velocity[axis] *= 0.96f;
    }

    _Dirty = true;
  }

  for ( const FpsCollisionObb & collider : iContext._PropColliders )
  {
    FpsCollisionSphereResult hit;
    if ( !FpsCollision::ResolveSphereObb(ioProjectile._Position, radius, collider, hit) )
      continue;

    ioProjectile._Position += hit._Correction;

    const float normalSpeed = glm::dot(ioProjectile._Velocity, hit._Normal);
    if ( normalSpeed < 0.f )
      ioProjectile._Velocity -= ( 1.f + bounciness ) * normalSpeed * hit._Normal;

    ioProjectile._Velocity *= 0.96f;
    if ( glm::length(ioProjectile._Velocity) < 0.15f )
      ioProjectile._Velocity = Vec3(0.f);

    _Dirty = true;
  }
}

}
