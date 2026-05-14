#ifndef _FpsGame_
#define _FpsGame_

#include "MathUtil.h"

#include <map>
#include <string>
#include <vector>

namespace RTRT
{

class Scene;
struct FpsGameMap;
struct FpsMapProp;

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
  int             _MaxHealth = 100;
  int             _MaxArmor = 50;
  int             _MaxProjectileAmmo = 32;
  float           _ProjectileAmmoRefillTime = 0.5f;
  bool            _FreeLook = false;
  bool            _ShowViewWeapon = true;
  Vec3            _ViewWeaponOffset = Vec3(0.44f, -0.33f, 0.83f);
  Vec3            _ViewWeaponRotation = Vec3(-6.5f, -10.5f, 3.f);
  float           _ViewWeaponScale = 1.24f;
};

struct FpsGameInput
{
  bool  _MoveForward = false;
  bool  _MoveBackward = false;
  bool  _MoveLeft = false;
  bool  _MoveRight = false;
  bool  _MoveUp = false;
  bool  _MoveDown = false;
  bool  _Sprint = false;
  bool  _JumpPressed = false;
  bool  _ResetPressed = false;
  bool   _FirePressed = false;
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
  int   _Armor = 50;

  Vec3 EyePosition( const FpsGameSettings & iSettings ) const;
};

struct FpsSceneObject
{
  std::string     _Name;
  Vec3            _Center = Vec3(0.f);
  Vec3            _HalfExtents = Vec3(0.5f);
  FpsMaterialSlot _Material = FpsMaterialSlot::Wall;
  std::string     _MaterialName = "wall";
  bool            _Collidable = true;
  bool            _Visible = true;
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
  int Initialize( const FpsGameSettings & iSettings, const FpsGameMap & iMap );
  int Reset( const FpsGameSettings & iSettings );
  int Update( float iDeltaTime, const FpsGameInput & iInput, const FpsGameSettings & iSettings );
  int ResizeProjectilePool( const FpsGameSettings & iSettings );
  void ClearProjectiles();

  const FpsPlayer & GetPlayer() const { return _Player; }
  FpsPlayer & GetPlayer() { return _Player; }
  const std::vector<FpsSceneObject> & GetObjects() const { return _Objects; }
  std::vector<FpsSceneObject> & GetObjects() { return _Objects; }
  const std::vector<FpsProjectile> & GetProjectiles() const { return _Projectiles; }
  int GetActiveProjectileCount() const;
  int GetProjectileAmmo() const { return _ProjectileAmmo; }
  bool ConsumeProjectilesDirty();

protected:
  void BuildDefaultArena();
  void BuildFromMap( const FpsGameMap & iMap );
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
  float                      _ProjectileAmmoRefillTimer = 0.f;
  int                        _ProjectileAmmo = 32;
  int                        _PendingProjectileShots = 0;
  bool                       _ProjectilesDirty = false;
  Vec3                       _SpawnPosition = Vec3(0.f, 0.05f, -8.f);
  float                      _SpawnYaw = 90.f;
  float                      _SpawnPitch = 0.f;
  int                        _SpawnHealth = -1;
  int                        _SpawnArmor = -1;
};

class FpsGameSceneBinding
{
public:
  int Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  int Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, const FpsGameMap & iMap );
  int SyncCamera( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  int SyncTransforms( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  bool HasViewWeapon() const { return !_WeaponInstanceIDs.empty(); }
  void Reset();

protected:
  int EnsureResources( Scene & iScene, const FpsGameMap * iMap );
  int LoadViewWeapon( Scene & iScene, const std::string & iPath );
  int LoadProps( Scene & iScene, const FpsGameMap & iMap );
  int AddLights( Scene & iScene, const FpsGameMap * iMap );
  Mat4x4 BuildObjectTransform( const FpsSceneObject & iObject ) const;
  Mat4x4 BuildProjectileTransform( const FpsProjectile & iProjectile, const FpsGameSettings & iSettings ) const;
  Mat4x4 BuildViewWeaponTransform( const FpsPlayer & iPlayer, const FpsGameSettings & iSettings ) const;
  Mat4x4 BuildPropTransform( const FpsMapProp & iProp ) const;
  int MaterialID( FpsMaterialSlot iMaterial ) const;
  int MaterialID( const std::string & iMaterialName, FpsMaterialSlot iFallback ) const;

protected:
  int              _CubeMeshID = -1;
  int              _SphereMeshID = -1;
  int              _ProjectileMaterialID = -1;
  int              _MaterialIDs[(int)FpsMaterialSlot::Count] = { -1, -1, -1, -1, -1 };
  std::map<std::string, int> _MapMaterialIDs;
  std::vector<int> _ObjectInstanceIDs;
  std::vector<int> _ProjectileInstanceIDs;
  std::vector<int> _WeaponInstanceIDs;
  std::vector<Mat4x4> _WeaponBaseTransforms;
};

}

#endif /* _FpsGame_ */
