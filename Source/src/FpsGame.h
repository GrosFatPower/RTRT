#ifndef _FpsGame_
#define _FpsGame_

#include "MathUtil.h"

#include <string>
#include <vector>

namespace RTRT
{

class Scene;

enum class FpsRendererMode
{
  Deferred = 0,
  Software,
  PhotoPathTracer
};

enum class FpsMaterialSlot
{
  Floor = 0,
  Wall,
  Pillar,
  Crate,
  Accent,
  Count
};

struct FpsGameSettings
{
  FpsRendererMode _RendererMode = FpsRendererMode::Deferred;
  float           _MoveSpeed    = 5.0f;
  float           _SprintSpeed  = 8.0f;
  float           _MouseSensitivity = 0.08f;
  float           _PlayerHeight = 1.8f;
  float           _PlayerRadius = 0.35f;
  float           _EyeHeight    = 1.62f;
  float           _Gravity      = 18.0f;
  float           _JumpSpeed    = 6.2f;
  float           _ProjectileRadius = 0.12f;
  float           _ProjectileSpeed = 18.f;
  float           _ProjectileGravity = 9.8f;
  float           _ProjectileBounciness = 0.72f;
  float           _ProjectileLifetime = 8.f;
  int             _MaxProjectiles = 32;
  float           _ProjectileCooldown = 0.12f;
};

struct FpsGameInput
{
  bool  _MoveForward = false;
  bool  _MoveBackward = false;
  bool  _MoveLeft = false;
  bool  _MoveRight = false;
  bool  _Sprint = false;
  bool  _JumpPressed = false;
  bool  _ResetPressed = false;
  int   _FireCount = 0;
  float _MouseDeltaX = 0.f;
  float _MouseDeltaY = 0.f;
};

struct FpsPlayer
{
  Vec3  _Position = Vec3(0.f);
  Vec3  _Velocity = Vec3(0.f);
  float _Yaw = 0.f;
  float _Pitch = 0.f;
  bool  _Grounded = false;
  int   _Health = 100;

  Vec3 EyePosition( const FpsGameSettings & iSettings ) const;
};

struct FpsSceneObject
{
  std::string     _Name;
  Vec3            _Center = Vec3(0.f);
  Vec3            _HalfExtents = Vec3(0.5f);
  FpsMaterialSlot _Material = FpsMaterialSlot::Wall;
  bool            _Collidable = true;
};

struct FpsProjectile
{
  bool  _Active = false;
  Vec3  _Position = Vec3(0.f);
  Vec3  _Velocity = Vec3(0.f);
  float _Age = 0.f;
};

class FpsGameWorld
{
public:
  int Initialize( const FpsGameSettings & iSettings );
  int Reset( const FpsGameSettings & iSettings );
  int Update( float iDeltaTime, const FpsGameInput & iInput, const FpsGameSettings & iSettings );
  int ResizeProjectilePool( const FpsGameSettings & iSettings );
  void ClearProjectiles();

  const FpsPlayer & GetPlayer() const { return _Player; }
  FpsPlayer & GetPlayer() { return _Player; }
  const std::vector<FpsSceneObject> & GetObjects() const { return _Objects; }
  const std::vector<FpsProjectile> & GetProjectiles() const { return _Projectiles; }
  int GetActiveProjectileCount() const;
  bool ConsumeProjectilesDirty();

protected:
  void BuildDefaultArena();
  void MoveAxis( int iAxis, float iDelta, const FpsGameSettings & iSettings );
  bool OverlapPlayerObject( const FpsSceneObject & iObject, const FpsGameSettings & iSettings, Vec3 & oOverlap ) const;
  void FireProjectile( const FpsGameSettings & iSettings );
  void UpdateProjectiles( float iDeltaTime, const FpsGameSettings & iSettings );
  void MoveProjectileAxis( FpsProjectile & ioProjectile, int iAxis, float iDelta, const FpsGameSettings & iSettings );
  Vec3 PlayerForward() const;

protected:
  FpsPlayer                  _Player;
  std::vector<FpsSceneObject> _Objects;
  std::vector<FpsProjectile> _Projectiles;
  float                      _ProjectileCooldownTimer = 0.f;
  int                        _PendingProjectileShots = 0;
  bool                       _ProjectilesDirty = false;
};

class FpsGameSceneBinding
{
public:
  int Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  int SyncCamera( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  int SyncTransforms( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  void Reset();

protected:
  int EnsureResources( Scene & iScene );
  int AddLights( Scene & iScene );
  Mat4x4 BuildObjectTransform( const FpsSceneObject & iObject ) const;
  Mat4x4 BuildProjectileTransform( const FpsProjectile & iProjectile, const FpsGameSettings & iSettings ) const;
  int MaterialID( FpsMaterialSlot iMaterial ) const;

protected:
  int              _CubeMeshID = -1;
  int              _SphereMeshID = -1;
  int              _ProjectileMaterialID = -1;
  int              _MaterialIDs[(int)FpsMaterialSlot::Count] = { -1, -1, -1, -1, -1 };
  std::vector<int> _ObjectInstanceIDs;
  std::vector<int> _ProjectileInstanceIDs;
};

}

#endif /* _FpsGame_ */
