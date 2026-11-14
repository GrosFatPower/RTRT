#ifndef _FpsGame_
#define _FpsGame_

#include "FpsCollision.h"
#include "FpsDecals.h"
#include "FpsHeadBob.h"
#include "FpsProjectiles.h"
#include "MathUtil.h"

#include <map>
#include <string>
#include <vector>

namespace RTRT
{
class Scene;
}

namespace FPS
{
using namespace RTRT;

struct FpsGameMap;
struct FpsMapProp;
struct FpsMapPropCollider;

enum class FpsRendererMode
{
  Deferred = 0,
  Software
};

enum class FpsPhotoModeState
{
  Inactive = 0,
  Setup,
  Capturing,
  Preview
};

struct FpsPhotoModeSettings
{
  int          _TargetPasses    = 64;
  int          _SamplesPerPixel = 1;
  int          _Bounces         = 4;
  bool         _Denoise         = true;
#if defined(RTRT_HAS_OIDN)
  unsigned int _DenoisingMethod = 3;
#else
  unsigned int _DenoisingMethod = 2;
#endif
};

enum class FpsAppState
{
  TitleScreen = 0,
  InGame
};

enum class FpsMenuPage
{
  MainMenu = 0,
  MapSelect,
  Options
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

enum class FpsPropCollisionMode
{
  None = 0,
  Bounds,
  Compound,
};

enum class FpsWeaponId : int
{
  GrenadeLauncher = 0,
  Mark23Pistol,
  CarbineRifle,
  Count
};

enum class FpsWeaponFireMode
{
  Projectile = 0,
  Hitscan
};

enum class FpsWeaponAnimAction
{
  Idle = 0,
  Draw,
  Shoot,
  Reload
};

struct FpsWeaponViewSettings
{
  Vec3  _Offset     = Vec3(0.44f, -0.33f, 0.83f);
  Vec3  _Rotation   = Vec3(-6.5f, -10.5f, 3.f);
  float _Scale      = 1.24f;
  Vec3  _ModelPivot = Vec3(0.f);
  bool  _FlipModelX = false;
};

struct FpsWeaponSlotConfig
{
  FpsWeaponId           _Id = FpsWeaponId::GrenadeLauncher;
  const char *          _Name = "GRENADE LAUNCHER";
  FpsWeaponFireMode     _FireMode = FpsWeaponFireMode::Projectile;
  const char *          _AssetPath = "";
  bool                  _Automatic = false;
  float                 _FireCooldown = 0.24f;
  float                 _MaxRange = 120.f;
  int                   _ClipCapacity = 32;
  float                 _ReloadDuration = 0.f;
  float                 _DrawDuration = 0.35f;
  float                 _ShootAnimDuration = 0.25f;
  FpsWeaponViewSettings _View;
};

struct FpsGameSettings
{
  FpsRendererMode _RendererMode = FpsRendererMode::Deferred;
  float           _MoveSpeed    = 5.0f;
  float           _SprintSpeed  = 8.0f;
  float           _MouseSensitivity = 0.08f;
  float           _CameraZNear = 0.05f;
  float           _CameraZFar = 200.f;
  float           _CameraFOV = 85.f;
  bool            _CameraOrthographic = false;
  float           _CameraOrthographicHeight = 10.f;
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
  int             _MaxDecals = 64;
  float           _DecalSize = 0.18f;
  float           _DecalLifetime = 0.f;
  float           _DecalNormalOffset = 0.003f;
  int             _MaxHealth = 100;
  int             _MaxArmor = 50;
  int             _MaxProjectileAmmo = 32;
  float           _ProjectileAmmoRefillTime = 0.5f;
  bool            _FreeLook = false;
  FpsHeadBobSettings _HeadBob;
  bool            _ShowViewWeapon = true;
  Vec3            _ViewWeaponOffset = Vec3(0.44f, -0.33f, 0.83f);
  Vec3            _ViewWeaponRotation = Vec3(-6.5f, -10.5f, 3.f);
  float           _ViewWeaponScale = 1.24f;
  int             _ActiveWeaponSlot = 0;
  FpsWeaponSlotConfig _WeaponSlots[(int)FpsWeaponId::Count];

  FpsGameSettings();
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
  bool   _FireHeld = false;
  bool   _ReloadPressed = false;
  int    _WeaponSwitchDelta = 0;
  int    _RequestedWeaponSlot = -1;
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
  FpsHeadBob _HeadBob;

  Vec3 EyePosition( const FpsGameSettings & iSettings ) const;
  Vec3 ViewPosition( const FpsGameSettings & iSettings ) const;
};

struct FpsSceneObject
{
  std::string     _Name;
  Vec3            _Center = Vec3(0.f);
  Vec3            _Rotation = Vec3(0.f);
  Vec4            _Orientation = Vec4(0.f, 0.f, 0.f, 1.f);
  Vec3            _HalfExtents = Vec3(0.5f);
  FpsMaterialSlot _Material = FpsMaterialSlot::Wall;
  std::string     _MaterialName = "wall";
  bool            _Collidable = true;
  bool            _Visible = true;
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
  int ResizeDecalPool( const FpsGameSettings & iSettings );
  void ClearDecals();

  const FpsPlayer & GetPlayer() const { return _Player; }
  FpsPlayer & GetPlayer() { return _Player; }
  const std::vector<FpsSceneObject> & GetObjects() const { return _Objects; }
  std::vector<FpsSceneObject> & GetObjects() { return _Objects; }
  const std::vector<FpsCollisionObb> & GetPropCollisionColliders() const { return _PropCollisionColliders; }
  void SetPropCollisionColliders( const std::vector<FpsCollisionObb> & iColliders ) { _PropCollisionColliders = iColliders; }
  void ClearPropCollisionColliders() { _PropCollisionColliders.clear(); }
  const std::vector<FpsProjectile> & GetProjectiles() const { return _Projectiles.GetProjectiles(); }
  int GetActiveProjectileCount() const { return _Projectiles.GetActiveCount(); }
  int GetProjectileAmmo() const { return _Projectiles.GetAmmo(); }
  bool ConsumeProjectilesDirty() { return _Projectiles.ConsumeDirty(); }
  const std::vector<FpsDecal> & GetDecals() const { return _Decals.GetDecals(); }
  int GetActiveDecalCount() const { return _Decals.GetActiveCount(); }
  bool ConsumeDecalsDirty() { return _Decals.ConsumeDirty(); }
  int GetActiveWeaponSlot() const { return _ActiveWeaponSlot; }
  FpsWeaponAnimAction GetWeaponAnimAction() const { return _WeaponAnimAction; }
  float GetWeaponAnimTimer() const { return _WeaponAnimTimer; }
  float GetWeaponRecoil() const { return _WeaponRecoil; }
  float GetHitMarkerTimer() const { return _HitMarkerTimer; }
  bool IsWeaponReloading() const { return FpsWeaponAnimAction::Reload == _WeaponAnimAction; }
  int GetWeaponAmmo( int iSlot, const FpsGameSettings & iSettings ) const;
  int GetWeaponMaxAmmo( int iSlot, const FpsGameSettings & iSettings ) const;
  bool SelectWeaponSlot( int iSlot, const FpsGameSettings & iSettings );
  bool CycleWeaponSlot( int iDelta, const FpsGameSettings & iSettings );
  bool TriggerReload( const FpsGameSettings & iSettings );
  bool ConsumeWeaponFired( int * oWeaponSlot = nullptr );
  bool ConsumeWeaponReloaded( int * oWeaponSlot = nullptr );
  void ConsumeProjectileSoundEvents( std::vector<Vec3> & oBouncePositions, std::vector<Vec3> & oExplosionPositions ) { _Projectiles.ConsumeSoundEvents(oBouncePositions, oExplosionPositions); }
  const Vec3 & GetSpawnPosition() const { return _SpawnPosition; }
  float GetSpawnYaw() const { return _SpawnYaw; }
  float GetSpawnPitch() const { return _SpawnPitch; }

protected:
  void BuildDefaultArena();
  void BuildFromMap( const FpsGameMap & iMap );
  void MoveAxis( int iAxis, float iDelta, const FpsGameSettings & iSettings );
  bool OverlapPlayerObject( const FpsSceneObject & iObject, const FpsGameSettings & iSettings, Vec3 & oOverlap ) const;
  Vec3 PlayerForward() const;
  bool FireHitscanWeapon( const FpsWeaponSlotConfig & iSlotConfig, const FpsGameSettings & iSettings );

protected:
  FpsPlayer                  _Player;
  std::vector<FpsSceneObject> _Objects;
  std::vector<FpsCollisionObb>     _PropCollisionColliders;
  FpsProjectiles             _Projectiles;
  FpsDecals                  _Decals;
  Vec3                       _SpawnPosition = Vec3(0.f, 0.05f, -8.f);
  float                      _SpawnYaw = 90.f;
  float                      _SpawnPitch = 0.f;
  int                        _SpawnHealth = -1;
  int                        _SpawnArmor = -1;
  int                        _ActiveWeaponSlot = 0;
  FpsWeaponAnimAction        _WeaponAnimAction = FpsWeaponAnimAction::Idle;
  float                      _WeaponAnimTimer = 0.f;
  float                      _WeaponFireCooldown = 0.f;
  float                      _WeaponDrawCooldown = 0.f;
  float                      _WeaponRecoil = 0.f;
  float                      _HitMarkerTimer = 0.f;
  int                        _WeaponAmmo[(int)FpsWeaponId::Count] = { 32, 12, 30 };
  bool                       _WeaponFiredDirty = false;
  int                        _LastFiredWeaponSlot = 0;
  bool                       _WeaponReloadDirty = false;
  int                        _LastReloadWeaponSlot = 0;
  bool                       _PrevFireHeld = false;
};

struct FpsLoadedWeaponModel
{
  std::vector<int>    _InstanceIDs;
  std::vector<Mat4x4> _BaseTransforms;
  int                 _AnimStartIndex = -1;
  int                 _AnimCount = 0;
};

class FpsGameSceneBinding
{
public:
  int Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings );
  int Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, const FpsGameMap & iMap );
  int SyncCamera( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, bool iAllowOrthographic = false );
  int SyncTransforms( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, bool * oMeshDataDirty = nullptr );
  int SyncTitleCamera( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, float iTime );
  void SetViewWeaponVisible( Scene & iScene, bool iVisible );
  int SyncProp( Scene & iScene, const FpsGameMap & iMap, int iPropIndex );
  int LoadProp( Scene & iScene, const FpsGameMap & iMap, int iPropIndex );
  int BuildPropCollisionColliders( Scene & iScene, const FpsGameMap & iMap, std::vector<FpsCollisionObb> & oColliders ) const;
  int BuildPropBoundsColliders( Scene & iScene, const FpsGameMap & iMap, int iPropIndex, std::vector<FpsMapPropCollider> & oColliders ) const;
  int SetObjectInstanceVisible( Scene & iScene, int iObjectIndex, bool iVisible );
  const std::vector<int> * GetPropInstanceIDs( int iPropIndex ) const;
  bool HasViewWeapon() const;
  void Reset();

protected:
  int EnsureResources( Scene & iScene, const FpsGameMap * iMap );
  int LoadViewWeaponSlot( Scene & iScene, int iSlotIndex, const std::string & iPath );
  int LoadProps( Scene & iScene, const FpsGameMap & iMap );
  int AddLights( Scene & iScene, const FpsGameMap * iMap );
  Mat4x4 BuildObjectTransform( const FpsSceneObject & iObject ) const;
  Mat4x4 BuildProjectileTransform( const FpsProjectile & iProjectile, const FpsGameSettings & iSettings ) const;
  Mat4x4 BuildDecalTransform( const FpsDecal & iDecal, const FpsGameSettings & iSettings ) const;
  Mat4x4 BuildViewWeaponTransform( const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, int iSlotIndex ) const;
  Mat4x4 BuildPropTransform( const FpsMapProp & iProp ) const;
  int MaterialID( FpsMaterialSlot iMaterial ) const;
  int MaterialID( const std::string & iMaterialName, FpsMaterialSlot iFallback ) const;

protected:
  int              _CubeMeshID = -1;
  int              _SphereMeshID = -1;
  int              _QuadMeshID = -1;
  int              _ProjectileMaterialID = -1;
  int              _BulletDecalMaterialID = -1;
  int              _MaterialIDs[(int)FpsMaterialSlot::Count] = { -1, -1, -1, -1, -1 };
  std::map<std::string, int> _MapMaterialIDs;
  std::vector<int> _ObjectInstanceIDs;
  std::vector<std::vector<int>> _PropInstanceIDs;
  std::vector<std::vector<Mat4x4>> _PropBaseTransforms;
  std::vector<int> _ProjectileInstanceIDs;
  std::vector<int> _DecalInstanceIDs;
  FpsLoadedWeaponModel _WeaponModels[(int)FpsWeaponId::Count];
  int                  _LastEvaluatedSlot = -1;
  int                  _LastEvaluatedClip = -1;
  float                _LastEvaluatedTime = -1.f;
};

}

#endif /* _FpsGame_ */
