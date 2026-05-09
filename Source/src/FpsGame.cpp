#include "FpsGame.h"

#include "Camera.h"
#include "Light.h"
#include "Loader.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "PathUtils.h"
#include "ProceduralMesh.h"
#include "RenderSettings.h"
#include "Scene.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static FpsSceneObject MakeBox( const std::string & iName,
                               const Vec3 & iCenter,
                               const Vec3 & iHalfExtents,
                               FpsMaterialSlot iMaterial,
                               bool iCollidable = true )
{
  FpsSceneObject object;
  object._Name = iName;
  object._Center = iCenter;
  object._HalfExtents = iHalfExtents;
  object._Material = iMaterial;
  object._Collidable = iCollidable;
  return object;
}

static Material MakeMaterial( const Vec3 & iAlbedo, float iRoughness, float iMetallic = 0.f, float iReflectance = 0.5f )
{
  Material material;
  material._Albedo = iAlbedo;
  material._Roughness = iRoughness;
  material._Metallic = iMetallic;
  material._Reflectance = iReflectance;
  return material;
}

// ----------------------------------------------------------------------------
// EyePosition
// ----------------------------------------------------------------------------
Vec3 FpsPlayer::EyePosition( const FpsGameSettings & iSettings ) const
{
  return _Position + Vec3(0.f, iSettings._EyeHeight, 0.f);
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int FpsGameWorld::Initialize( const FpsGameSettings & iSettings )
{
  BuildDefaultArena();
  ResizeProjectilePool(iSettings);
  return Reset(iSettings);
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
int FpsGameWorld::Reset( const FpsGameSettings & iSettings )
{
  _Player._Position = Vec3(0.f, 0.05f, -8.f);
  _Player._Velocity = Vec3(0.f);
  _Player._Yaw = 90.f;
  _Player._Pitch = 0.f;
  _Player._Grounded = false;
  _Player._Health = std::max(0, iSettings._MaxHealth);
  _Player._Armor = std::max(0, iSettings._MaxArmor);
  _ProjectileCooldownTimer = 0.f;
  _ProjectileAmmoRefillTimer = 0.f;
  _ProjectileAmmo = std::max(0, iSettings._MaxProjectileAmmo);
  _PendingProjectileShots = 0;
  ClearProjectiles();

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int FpsGameWorld::Update( float iDeltaTime, const FpsGameInput & iInput, const FpsGameSettings & iSettings )
{
  _ProjectilesDirty = false;

  if ( iInput._ResetPressed )
    Reset(iSettings);

  const float realDt = MathUtil::Clamp(iDeltaTime, 0.f, 0.25f);
  const float dt = MathUtil::Clamp(iDeltaTime, 0.f, 0.05f);
  if ( realDt <= 0.f )
    return 0;

  ResizeProjectilePool(iSettings);
  const float cooldown = std::max(0.001f, iSettings._ProjectileCooldown);
  const int maxAmmo = std::max(0, iSettings._MaxProjectileAmmo);
  _ProjectileAmmo = MathUtil::Clamp(_ProjectileAmmo, 0, maxAmmo);

  if ( _ProjectileAmmo < maxAmmo )
  {
    const float refillTime = std::max(0.001f, iSettings._ProjectileAmmoRefillTime);
    _ProjectileAmmoRefillTimer += realDt;
    while ( ( _ProjectileAmmoRefillTimer >= refillTime ) && ( _ProjectileAmmo < maxAmmo ) )
    {
      _ProjectileAmmoRefillTimer -= refillTime;
      _ProjectileAmmo++;
    }
  }
  else
    _ProjectileAmmoRefillTimer = 0.f;

  _ProjectileCooldownTimer += iDeltaTime;
  if ( iInput._FirePressed )
  {
    if ( ( _ProjectileAmmo > 0 ) && ( _ProjectileCooldownTimer > cooldown ) )
    {
      _PendingProjectileShots += 1;
      _ProjectileAmmo--;
      _ProjectileCooldownTimer = 0;
    }
  }

  if ( dt <= 0.f )
    return 0;

  _Player._Yaw += iInput._MouseDeltaX * iSettings._MouseSensitivity;
  _Player._Pitch -= iInput._MouseDeltaY * iSettings._MouseSensitivity;

  if ( fabs(_Player._Yaw) > 360.f )
    _Player._Yaw -= MathUtil::Sign(_Player._Yaw) * 360.f * floor( fabs( _Player._Yaw / 360.f ) );
  _Player._Pitch = MathUtil::Clamp(_Player._Pitch, -89.f, 89.f);

  const float yawRad = MathUtil::ToRadians(_Player._Yaw);
  Vec3 forward(std::cos(yawRad), 0.f, std::sin(yawRad));
  Vec3 right = glm::normalize(glm::cross(forward, Vec3(0.f, 1.f, 0.f)));

  Vec3 wishDir(0.f);
  if ( iInput._MoveForward )
    wishDir += forward;
  if ( iInput._MoveBackward )
    wishDir -= forward;
  if ( iInput._MoveRight )
    wishDir += right;
  if ( iInput._MoveLeft )
    wishDir -= right;

  const float wishLen = glm::length(wishDir);
  if ( wishLen > EPSILON )
    wishDir /= wishLen;

  const float speed = iInput._Sprint ? iSettings._SprintSpeed : iSettings._MoveSpeed;
  _Player._Velocity.x = wishDir.x * speed;
  _Player._Velocity.z = wishDir.z * speed;

  if ( _Player._Grounded && ( _Player._Velocity.y < 0.f ) )
    _Player._Velocity.y = 0.f;

  if ( iInput._JumpPressed && _Player._Grounded )
  {
    _Player._Velocity.y = iSettings._JumpSpeed;
    _Player._Grounded = false;
  }

  _Player._Velocity.y -= iSettings._Gravity * dt;

  MoveAxis(0, _Player._Velocity.x * dt, iSettings);
  MoveAxis(2, _Player._Velocity.z * dt, iSettings);

  _Player._Grounded = false;
  MoveAxis(1, _Player._Velocity.y * dt, iSettings);

  while ( _PendingProjectileShots > 0 )
  {
    FireProjectile(iSettings);
    _PendingProjectileShots--;
  }

  UpdateProjectiles(dt, iSettings);

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeProjectilePool
// ----------------------------------------------------------------------------
int FpsGameWorld::ResizeProjectilePool( const FpsGameSettings & iSettings )
{
  const int maxProjectiles = std::max(1, iSettings._MaxProjectiles);
  if ( static_cast<int>(_Projectiles.size()) == maxProjectiles )
    return 0;

  _Projectiles.resize(maxProjectiles);
  _ProjectilesDirty = true;
  return 0;
}

// ----------------------------------------------------------------------------
// ClearProjectiles
// ----------------------------------------------------------------------------
void FpsGameWorld::ClearProjectiles()
{
  for ( FpsProjectile & projectile : _Projectiles )
  {
    projectile._Active = false;
    projectile._Position = Vec3(0.f);
    projectile._Velocity = Vec3(0.f);
    projectile._Age = 0.f;
  }
  _ProjectilesDirty = true;
}

// ----------------------------------------------------------------------------
// GetActiveProjectileCount
// ----------------------------------------------------------------------------
int FpsGameWorld::GetActiveProjectileCount() const
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
// ConsumeProjectilesDirty
// ----------------------------------------------------------------------------
bool FpsGameWorld::ConsumeProjectilesDirty()
{
  const bool dirty = _ProjectilesDirty;
  _ProjectilesDirty = false;
  return dirty;
}

// ----------------------------------------------------------------------------
// PlayerForward
// ----------------------------------------------------------------------------
Vec3 FpsGameWorld::PlayerForward() const
{
  const float yawRad = MathUtil::ToRadians(_Player._Yaw);
  const float pitchRad = MathUtil::ToRadians(_Player._Pitch);
  Vec3 forward(std::cos(yawRad) * std::cos(pitchRad),
               std::sin(pitchRad),
               std::sin(yawRad) * std::cos(pitchRad));
  return glm::normalize(forward);
}

// ----------------------------------------------------------------------------
// FireProjectile
// ----------------------------------------------------------------------------
void FpsGameWorld::FireProjectile( const FpsGameSettings & iSettings )
{
  if ( _Projectiles.empty() )
    ResizeProjectilePool(iSettings);

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
  const Vec3 forward = PlayerForward();
  FpsProjectile & projectile = _Projectiles[projectileID];
  projectile._Active = true;
  projectile._Position = _Player.EyePosition(iSettings) + forward * (iSettings._PlayerRadius + radius + 0.08f);
  projectile._Velocity = forward * std::max(0.f, iSettings._ProjectileSpeed);
  projectile._Age = 0.f;

  _ProjectilesDirty = true;
}

// ----------------------------------------------------------------------------
// UpdateProjectiles
// ----------------------------------------------------------------------------
void FpsGameWorld::UpdateProjectiles( float iDeltaTime, const FpsGameSettings & iSettings )
{
  const float radius = std::max(0.02f, iSettings._ProjectileRadius);
  const float maxMovePerStep = std::max(0.05f, radius * 0.75f);

  for ( FpsProjectile & projectile : _Projectiles )
  {
    if ( !projectile._Active )
      continue;

    projectile._Age += iDeltaTime;
    if ( projectile._Age >= std::max(0.1f, iSettings._ProjectileLifetime) )
    {
      projectile._Active = false;
      _ProjectilesDirty = true;
      continue;
    }

    const float moveLen = glm::length(projectile._Velocity) * iDeltaTime;
    const int nbSteps = std::max(1, std::min(16, static_cast<int>(std::ceil(moveLen / maxMovePerStep))));
    const float stepDt = iDeltaTime / static_cast<float>(nbSteps);

    for ( int step = 0; step < nbSteps; ++step )
    {
      projectile._Velocity.y -= std::max(0.f, iSettings._ProjectileGravity) * stepDt;
      MoveProjectileAxis(projectile, 0, projectile._Velocity.x * stepDt, iSettings);
      MoveProjectileAxis(projectile, 1, projectile._Velocity.y * stepDt, iSettings);
      MoveProjectileAxis(projectile, 2, projectile._Velocity.z * stepDt, iSettings);
    }

    _ProjectilesDirty = true;
  }
}

// ----------------------------------------------------------------------------
// MoveProjectileAxis
// ----------------------------------------------------------------------------
void FpsGameWorld::MoveProjectileAxis( FpsProjectile & ioProjectile, int iAxis, float iDelta, const FpsGameSettings & iSettings )
{
  if ( fabs(iDelta) <= EPSILON )
    return;

  ioProjectile._Position[iAxis] += iDelta;

  const float radius = std::max(0.02f, iSettings._ProjectileRadius);
  const float bounciness = MathUtil::Clamp(iSettings._ProjectileBounciness, 0.f, 1.f);

  for ( const FpsSceneObject & object : _Objects )
  {
    if ( !object._Collidable )
      continue;

    const Vec3 delta = ioProjectile._Position - object._Center;
    const Vec3 sumHalf = object._HalfExtents + Vec3(radius);
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

    _ProjectilesDirty = true;
  }
}

// ----------------------------------------------------------------------------
// BuildDefaultArena
// ----------------------------------------------------------------------------
void FpsGameWorld::BuildDefaultArena()
{
  _Objects.clear();

  _Objects.push_back(MakeBox("Floor", Vec3(0.f, -0.25f, 0.f), Vec3(14.f, 0.25f, 14.f), FpsMaterialSlot::Floor));
  _Objects.push_back(MakeBox("North Wall", Vec3(0.f, 2.f, 14.f), Vec3(14.f, 2.f, 0.35f), FpsMaterialSlot::Wall));
  _Objects.push_back(MakeBox("South Wall", Vec3(0.f, 2.f, -14.f), Vec3(14.f, 2.f, 0.35f), FpsMaterialSlot::Wall));
  _Objects.push_back(MakeBox("East Wall", Vec3(14.f, 2.f, 0.f), Vec3(0.35f, 2.f, 14.f), FpsMaterialSlot::Wall));
  _Objects.push_back(MakeBox("West Wall", Vec3(-14.f, 2.f, 0.f), Vec3(0.35f, 2.f, 14.f), FpsMaterialSlot::Wall));

  _Objects.push_back(MakeBox("Pillar A", Vec3(-5.f, 1.5f, -2.f), Vec3(0.8f, 1.5f, 0.8f), FpsMaterialSlot::Pillar));
  _Objects.push_back(MakeBox("Pillar B", Vec3(5.f, 1.5f, 3.f), Vec3(0.8f, 1.5f, 0.8f), FpsMaterialSlot::Pillar));
  _Objects.push_back(MakeBox("Low Block A", Vec3(-2.f, 0.5f, 4.f), Vec3(2.0f, 0.5f, 1.0f), FpsMaterialSlot::Accent));
  _Objects.push_back(MakeBox("Low Block B", Vec3(3.f, 0.75f, -5.f), Vec3(1.2f, 0.75f, 1.2f), FpsMaterialSlot::Accent));

  _Objects.push_back(MakeBox("Target Crate A", Vec3(-7.f, 0.6f, 6.f), Vec3(0.6f), FpsMaterialSlot::Crate));
  _Objects.push_back(MakeBox("Target Crate B", Vec3(7.f, 0.6f, -7.f), Vec3(0.6f), FpsMaterialSlot::Crate));
  _Objects.push_back(MakeBox("Target Crate C", Vec3(0.f, 0.6f, 8.f), Vec3(0.6f), FpsMaterialSlot::Crate));
}

// ----------------------------------------------------------------------------
// MoveAxis
// ----------------------------------------------------------------------------
void FpsGameWorld::MoveAxis( int iAxis, float iDelta, const FpsGameSettings & iSettings )
{
  if ( fabs(iDelta) <= EPSILON )
    return;

  _Player._Position[iAxis] += iDelta;

  for ( const FpsSceneObject & object : _Objects )
  {
    if ( !object._Collidable )
      continue;

    Vec3 overlap(0.f);
    if ( !OverlapPlayerObject(object, iSettings, overlap) )
      continue;

    int minAxis = 0;
    if ( overlap.y < overlap[minAxis] )
      minAxis = 1;
    if ( overlap.z < overlap[minAxis] )
      minAxis = 2;
    if ( minAxis != iAxis )
      continue;

    if ( iDelta > 0.f )
      _Player._Position[iAxis] -= overlap[iAxis];
    else
      _Player._Position[iAxis] += overlap[iAxis];

    _Player._Velocity[iAxis] = 0.f;
    if ( ( 1 == iAxis ) && ( iDelta < 0.f ) )
      _Player._Grounded = true;
  }
}

// ----------------------------------------------------------------------------
// OverlapPlayerObject
// ----------------------------------------------------------------------------
bool FpsGameWorld::OverlapPlayerObject( const FpsSceneObject & iObject, const FpsGameSettings & iSettings, Vec3 & oOverlap ) const
{
  const Vec3 playerHalf(std::max(iSettings._PlayerRadius, 0.05f),
                        std::max(iSettings._PlayerHeight * 0.5f, 0.1f),
                        std::max(iSettings._PlayerRadius, 0.05f));
  const Vec3 playerCenter = _Player._Position + Vec3(0.f, playerHalf.y, 0.f);
  const Vec3 delta = playerCenter - iObject._Center;
  const Vec3 sumHalf = playerHalf + iObject._HalfExtents;

  if ( ( std::abs(delta.x) >= sumHalf.x )
    || ( std::abs(delta.y) >= sumHalf.y )
    || ( std::abs(delta.z) >= sumHalf.z ) )
    return false;

  oOverlap = sumHalf - Vec3(std::abs(delta.x), std::abs(delta.y), std::abs(delta.z));
  return true;
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void FpsGameSceneBinding::Reset()
{
  _CubeMeshID = -1;
  _SphereMeshID = -1;
  _ProjectileMaterialID = -1;
  for ( int i = 0; i < (int)FpsMaterialSlot::Count; ++i )
    _MaterialIDs[i] = -1;
  _ObjectInstanceIDs.clear();
  _ProjectileInstanceIDs.clear();
  _WeaponInstanceIDs.clear();
  _WeaponBaseTransforms.clear();
}

// ----------------------------------------------------------------------------
// Attach
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings )
{
  Reset();

  if ( 0 != EnsureResources(iScene) )
    return 1;

  if ( 0 != AddLights(iScene) )
    return 1;

  if ( 0 != LoadViewWeapon(iScene) )
    std::cout << "FpsGameSceneBinding : failed to load view weapon model" << std::endl;

  const std::vector<FpsSceneObject> & objects = iWorld.GetObjects();
  _ObjectInstanceIDs.reserve(objects.size());

  for ( const FpsSceneObject & object : objects )
  {
    const int materialID = MaterialID(object._Material);
    if ( ( _CubeMeshID < 0 ) || ( materialID < 0 ) )
      return 1;

    MeshInstance instance(object._Name, _CubeMeshID, materialID, BuildObjectTransform(object));
    _ObjectInstanceIDs.push_back(iScene.AddMeshInstance(instance));
  }

  const std::vector<FpsProjectile> & projectiles = iWorld.GetProjectiles();
  _ProjectileInstanceIDs.reserve(projectiles.size());
  for ( int i = 0; i < static_cast<int>(projectiles.size()); ++i )
  {
    if ( ( _SphereMeshID < 0 ) || ( _ProjectileMaterialID < 0 ) )
      return 1;

    MeshInstance instance("Projectile " + std::to_string(i), _SphereMeshID, _ProjectileMaterialID, BuildProjectileTransform(projectiles[i], iSettings));
    _ProjectileInstanceIDs.push_back(iScene.AddMeshInstance(instance));
  }

  return SyncCamera(iScene, iWorld, iSettings);
}

// ----------------------------------------------------------------------------
// SyncCamera
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::SyncCamera( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings )
{
  Camera & camera = iScene.GetCamera();
  const FpsPlayer & player = iWorld.GetPlayer();
  camera.SetFreeLookPose(player.EyePosition(iSettings), player._Yaw, player._Pitch);
  camera.SetFOVInDegrees(85.f);
  camera.SetZNearFar(0.05f, 200.f);
  return 0;
}

// ----------------------------------------------------------------------------
// SyncTransforms
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::SyncTransforms( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings )
{
  const std::vector<FpsSceneObject> & objects = iWorld.GetObjects();
  if ( objects.size() != _ObjectInstanceIDs.size() )
    return 1;

  const std::vector<FpsProjectile> & projectiles = iWorld.GetProjectiles();
  if ( projectiles.size() != _ProjectileInstanceIDs.size() )
    return 1;

  std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  for ( int i = 0; i < static_cast<int>(_ObjectInstanceIDs.size()); ++i )
  {
    const int instanceID = _ObjectInstanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      return 1;

    instances[instanceID]._Transform = BuildObjectTransform(objects[i]);
  }

  for ( int i = 0; i < static_cast<int>(_ProjectileInstanceIDs.size()); ++i )
  {
    const int instanceID = _ProjectileInstanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      return 1;

    instances[instanceID]._Transform = BuildProjectileTransform(projectiles[i], iSettings);
  }

  if ( _WeaponInstanceIDs.size() != _WeaponBaseTransforms.size() )
    return 1;

  const Mat4x4 weaponTransform = BuildViewWeaponTransform(iWorld.GetPlayer(), iSettings);
  for ( int i = 0; i < static_cast<int>(_WeaponInstanceIDs.size()); ++i )
  {
    const int instanceID = _WeaponInstanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      return 1;

    instances[instanceID]._Transform = weaponTransform * _WeaponBaseTransforms[i];
  }

  return 0;
}

// ----------------------------------------------------------------------------
// EnsureResources
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::EnsureResources( Scene & iScene )
{
  Mesh * cubeMesh = ProceduralMesh::CreateCube("__FpsCubeMesh");
  _CubeMeshID = iScene.AddMesh(cubeMesh);
  if ( _CubeMeshID < 0 )
  {
    delete cubeMesh;
    return 1;
  }

  Mesh * sphereMesh = ProceduralMesh::CreateUVSphere("__FpsProjectileSphere", 8, 16);
  _SphereMeshID = iScene.AddMesh(sphereMesh);
  if ( _SphereMeshID < 0 )
  {
    delete sphereMesh;
    return 1;
  }

  Material floor = MakeMaterial(Vec3(0.42f, 0.43f, 0.44f), 0.18f, 0.f, 0.9f);
  Material wall = MakeMaterial(Vec3(0.34f, 0.37f, 0.42f), 0.65f);
  Material pillar = MakeMaterial(Vec3(0.58f, 0.55f, 0.49f), 0.55f);
  Material crate = MakeMaterial(Vec3(0.80f, 0.24f, 0.13f), 0.45f);
  Material accent = MakeMaterial(Vec3(0.12f, 0.42f, 0.68f), 0.16f, 0.f, 0.85f);
  Material projectile = MakeMaterial(Vec3(1.0f, 0.04f, 0.02f), 0.28f, 0.f, 0.75f);

  _MaterialIDs[(int)FpsMaterialSlot::Floor] = iScene.AddMaterial(floor, "__FpsFloor");
  _MaterialIDs[(int)FpsMaterialSlot::Wall] = iScene.AddMaterial(wall, "__FpsWall");
  _MaterialIDs[(int)FpsMaterialSlot::Pillar] = iScene.AddMaterial(pillar, "__FpsPillar");
  _MaterialIDs[(int)FpsMaterialSlot::Crate] = iScene.AddMaterial(crate, "__FpsCrate");
  _MaterialIDs[(int)FpsMaterialSlot::Accent] = iScene.AddMaterial(accent, "__FpsAccent");
  _ProjectileMaterialID = iScene.AddMaterial(projectile, "__FpsProjectile");

  for ( int i = 0; i < (int)FpsMaterialSlot::Count; ++i )
  {
    if ( _MaterialIDs[i] < 0 )
      return 1;
  }
  if ( _ProjectileMaterialID < 0 )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// LoadViewWeapon
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::LoadViewWeapon( Scene & iScene )
{
  const int firstInstanceID = iScene.GetNbMeshInstances();
  RenderSettings loaderSettings;

  if ( !Loader::LoadScene(PathUtils::GetAssetPath("overwatch_junkrats_grenade_launcher/scene.gltf"), iScene, loaderSettings) )
    return 1;

  std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  for ( int instanceID = firstInstanceID; instanceID < static_cast<int>(instances.size()); ++instanceID )
  {
    _WeaponInstanceIDs.push_back(instanceID);
    _WeaponBaseTransforms.push_back(instances[instanceID]._Transform);
  }

  return _WeaponInstanceIDs.empty() ? 1 : 0;
}

// ----------------------------------------------------------------------------
// AddLights
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::AddLights( Scene & iScene )
{
  Light sun;
  sun._Type = (float)LightType::DistantLight;
  sun._Pos = Vec3(-0.4f, 0.8f, -0.3f);
  sun._Emission = Vec3(1.f, 0.94f, 0.86f);
  sun._Intensity = 2.0f;
  sun._CastShadow = true;
  iScene.AddLight(sun);

  Light warm;
  warm._Type = (float)LightType::SphereLight;
  warm._Pos = Vec3(-6.f, 3.5f, -6.f);
  warm._Emission = Vec3(1.f, 0.55f, 0.25f);
  warm._Intensity = 35.f;
  warm._Radius = 0.25f;
  warm._Area = 4.0f * static_cast<float>(M_PI) * warm._Radius * warm._Radius;
  warm._CastShadow = true;
  warm._ShadowRadius = 14.f;
  iScene.AddLight(warm);

  Light cool;
  cool._Type = (float)LightType::SphereLight;
  cool._Pos = Vec3(6.f, 3.0f, 5.f);
  cool._Emission = Vec3(0.25f, 0.55f, 1.f);
  cool._Intensity = 25.f;
  cool._Radius = 0.25f;
  cool._Area = 4.0f * static_cast<float>(M_PI) * cool._Radius * cool._Radius;
  cool._CastShadow = false;
  iScene.AddLight(cool);

  return 0;
}

// ----------------------------------------------------------------------------
// BuildObjectTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsGameSceneBinding::BuildObjectTransform( const FpsSceneObject & iObject ) const
{
  return glm::translate(iObject._Center) * glm::scale(iObject._HalfExtents * 2.f);
}

// ----------------------------------------------------------------------------
// BuildProjectileTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsGameSceneBinding::BuildProjectileTransform( const FpsProjectile & iProjectile, const FpsGameSettings & iSettings ) const
{
  if ( !iProjectile._Active )
    return glm::translate(Vec3(0.f, -4.f, 0.f)) * glm::scale(Vec3(0.001f));

  const float radius = std::max(0.02f, iSettings._ProjectileRadius);
  return glm::translate(iProjectile._Position) * glm::scale(Vec3(radius * 2.f));
}

// ----------------------------------------------------------------------------
// BuildViewWeaponTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsGameSceneBinding::BuildViewWeaponTransform( const FpsPlayer & iPlayer, const FpsGameSettings & iSettings ) const
{
  if ( !iSettings._ShowViewWeapon )
    return glm::translate(Vec3(0.f, -1000.f, 0.f)) * glm::scale(Vec3(0.001f));

  const float yawRad = MathUtil::ToRadians(iPlayer._Yaw);
  const float pitchRad = MathUtil::ToRadians(iPlayer._Pitch);
  Vec3 forward(std::cos(yawRad) * std::cos(pitchRad),
               std::sin(pitchRad),
               std::sin(yawRad) * std::cos(pitchRad));
  forward = glm::normalize(forward);

  const Vec3 worldUp(0.f, 1.f, 0.f);
  const Vec3 right = glm::normalize(glm::cross(forward, worldUp));
  const Vec3 up = glm::normalize(glm::cross(right, forward));

  Mat4x4 cameraTransform(1.f);
  cameraTransform[0] = Vec4(right, 0.f);
  cameraTransform[1] = Vec4(up, 0.f);
  cameraTransform[2] = Vec4(forward, 0.f);
  cameraTransform[3] = Vec4(iPlayer.EyePosition(iSettings), 1.f);

  const Vec3 rotRad(MathUtil::ToRadians(iSettings._ViewWeaponRotation.x),
                    MathUtil::ToRadians(iSettings._ViewWeaponRotation.y),
                    MathUtil::ToRadians(iSettings._ViewWeaponRotation.z));

  Mat4x4 localTransform = glm::translate(iSettings._ViewWeaponOffset);
  localTransform = localTransform * glm::rotate(rotRad.y, Vec3(0.f, 1.f, 0.f));
  localTransform = localTransform * glm::rotate(rotRad.x, Vec3(1.f, 0.f, 0.f));
  localTransform = localTransform * glm::rotate(rotRad.z, Vec3(0.f, 0.f, 1.f));
  localTransform = localTransform * glm::scale(Vec3(std::max(0.001f, iSettings._ViewWeaponScale)));

  return cameraTransform * localTransform;
}

// ----------------------------------------------------------------------------
// MaterialID
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::MaterialID( FpsMaterialSlot iMaterial ) const
{
  const int materialIndex = (int)iMaterial;
  if ( ( materialIndex < 0 ) || ( materialIndex >= (int)FpsMaterialSlot::Count ) )
    return -1;
  return _MaterialIDs[materialIndex];
}

}
