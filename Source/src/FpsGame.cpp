#include "FpsGame.h"

#include "Camera.h"
#include "FpsGameMap.h"
#include "Light.h"
#include "Loader.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "PathUtils.h"
#include "ProceduralMesh.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "Texture.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>

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
  switch ( iMaterial )
  {
    case FpsMaterialSlot::Floor:  object._MaterialName = "floor";  break;
    case FpsMaterialSlot::Wall:   object._MaterialName = "wall";   break;
    case FpsMaterialSlot::Pillar: object._MaterialName = "pillar"; break;
    case FpsMaterialSlot::Crate:  object._MaterialName = "crate";  break;
    case FpsMaterialSlot::Accent: object._MaterialName = "accent"; break;
    default:                      object._MaterialName = "wall";   break;
  }
  object._Collidable = iCollidable;
  object._Visible = true;
  return object;
}

static std::filesystem::path GetPropFilePath( const std::string & iPath )
{
  const std::filesystem::path filepath(iPath);
  if ( filepath.is_absolute() )
    return filepath.lexically_normal();
  return std::filesystem::path(PathUtils::GetAssetPath(iPath)).lexically_normal();
}

static std::string MakePropResourceName( const FpsMapProp & iProp, int iPropIndex, const std::string & iName )
{
  std::string name = "__FpsProp_" + std::to_string(iPropIndex) + "_" + iProp._Name;
  for ( char & c : name )
  {
    if ( std::isspace(static_cast<unsigned char>(c)) )
      c = '_';
  }

  if ( !iName.empty() )
    name += "_" + iName;
  return name;
}

static int RemapMaterialTextureID( float & ioTextureID, const std::vector<int> & iTextureIDs )
{
  if ( ioTextureID < 0.f )
    return 0;

  const int srcID = static_cast<int>(ioTextureID + 0.5f);
  if ( srcID >= static_cast<int>(iTextureIDs.size()) )
    return 1;

  ioTextureID = static_cast<float>(iTextureIDs[srcID]);
  return 0;
}

static Mat4x4 BuildEulerTransform( const Vec3 & iPosition, const Vec3 & iRotation, const Vec3 & iScale )
{
  const Vec3 rotRad(MathUtil::ToRadians(iRotation.x),
                    MathUtil::ToRadians(iRotation.y),
                    MathUtil::ToRadians(iRotation.z));

  Mat4x4 transform = glm::translate(iPosition);
  transform = transform * glm::rotate(rotRad.x, Vec3(1.f, 0.f, 0.f));
  transform = transform * glm::rotate(rotRad.y, Vec3(0.f, 1.f, 0.f));
  transform = transform * glm::rotate(rotRad.z, Vec3(0.f, 0.f, 1.f));
  transform = transform * glm::scale(iScale);
  return transform;
}

static glm::quat QuatFromVec4( const Vec4 & iQuat )
{
  glm::quat quat(iQuat.w, iQuat.x, iQuat.y, iQuat.z);
  const float len = glm::length(quat);
  if ( len <= EPSILON )
    return glm::quat(1.f, 0.f, 0.f, 0.f);
  return glm::normalize(quat);
}

static Vec4 Vec4FromEuler( const Vec3 & iRotation )
{
  glm::quat quat = glm::quat_cast(glm::mat3(BuildEulerTransform(Vec3(0.f), iRotation, Vec3(1.f))));
  return Vec4(quat.x, quat.y, quat.z, quat.w);
}

static Mat4x4 BuildObjectRotationTransform( const FpsSceneObject & iObject )
{
  Vec4 orientation = iObject._Orientation;
  if ( glm::length(orientation) <= EPSILON )
    orientation = Vec4FromEuler(iObject._Rotation);

  return glm::toMat4(QuatFromVec4(orientation));
}

static Mat4x4 BuildSceneObjectTransform( const FpsSceneObject & iObject, const Vec3 & iScale )
{
  Mat4x4 transform = glm::translate(iObject._Center);
  transform = transform * BuildObjectRotationTransform(iObject);
  transform = transform * glm::scale(iScale);
  return transform;
}

static Vec3 ObjectCollisionHalfExtents( const FpsSceneObject & iObject )
{
  if ( ( glm::length(iObject._Rotation) <= EPSILON ) && ( glm::length(iObject._Orientation - Vec4(0.f, 0.f, 0.f, 1.f)) <= EPSILON ) )
    return iObject._HalfExtents;

  const Mat4x4 rotation = BuildObjectRotationTransform(iObject);
  Vec3 corners[8] =
  {
    Vec3(-iObject._HalfExtents.x, -iObject._HalfExtents.y, -iObject._HalfExtents.z),
    Vec3( iObject._HalfExtents.x, -iObject._HalfExtents.y, -iObject._HalfExtents.z),
    Vec3(-iObject._HalfExtents.x,  iObject._HalfExtents.y, -iObject._HalfExtents.z),
    Vec3( iObject._HalfExtents.x,  iObject._HalfExtents.y, -iObject._HalfExtents.z),
    Vec3(-iObject._HalfExtents.x, -iObject._HalfExtents.y,  iObject._HalfExtents.z),
    Vec3( iObject._HalfExtents.x, -iObject._HalfExtents.y,  iObject._HalfExtents.z),
    Vec3(-iObject._HalfExtents.x,  iObject._HalfExtents.y,  iObject._HalfExtents.z),
    Vec3( iObject._HalfExtents.x,  iObject._HalfExtents.y,  iObject._HalfExtents.z)
  };

  Vec3 halfExtents(0.f);
  for ( int i = 0; i < 8; ++i )
  {
    const Vec3 corner = Vec3(rotation * Vec4(corners[i], 1.f));
    halfExtents = MathUtil::Max(halfExtents, Vec3(std::abs(corner.x), std::abs(corner.y), std::abs(corner.z)));
  }

  return halfExtents;
}

static AABB<Vec3> TransformAABB( const AABB<Vec3> & iBox, const Mat4x4 & iTransform )
{
  Vec3 corners[8];
  iBox.Corners(corners);

  AABB<Vec3> result;
  for ( int i = 0; i < 8; ++i )
    result.Insert(MathUtil::TransformPoint(corners[i], iTransform));

  return result;
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
// ViewPosition
// ----------------------------------------------------------------------------
Vec3 FpsPlayer::ViewPosition( const FpsGameSettings & iSettings ) const
{
  if ( iSettings._FreeLook )
    return EyePosition(iSettings);
  return EyePosition(iSettings) + _HeadBob.GetOffset();
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int FpsGameWorld::Initialize( const FpsGameSettings & iSettings )
{
  BuildDefaultArena();
  return Reset(iSettings);
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int FpsGameWorld::Initialize( const FpsGameSettings & iSettings, const FpsGameMap & iMap )
{
  BuildFromMap(iMap);
  return Reset(iSettings);
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
int FpsGameWorld::Reset( const FpsGameSettings & iSettings )
{
  _Player._Position = _SpawnPosition;
  _Player._Velocity = Vec3(0.f);
  _Player._Yaw = _SpawnYaw;
  _Player._Pitch = _SpawnPitch;
  _Player._Grounded = false;
  _Player._Health = std::max(0, ( _SpawnHealth >= 0 ) ? _SpawnHealth : iSettings._MaxHealth);
  _Player._Armor = std::max(0, ( _SpawnArmor >= 0 ) ? _SpawnArmor : iSettings._MaxArmor);
  _Player._HeadBob.Reset();

  return _Projectiles.Initialize(iSettings);
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int FpsGameWorld::Update( float iDeltaTime, const FpsGameInput & iInput, const FpsGameSettings & iSettings )
{
  if ( iInput._ResetPressed )
    Reset(iSettings);

  const float realDt = MathUtil::Clamp(iDeltaTime, 0.f, 0.25f);
  const float dt = MathUtil::Clamp(iDeltaTime, 0.f, 0.05f);
  if ( realDt <= 0.f )
    return 0;

  FpsProjectilesUpdateContext projectilesContext(iSettings, _Player, _Objects, _PropCollisionColliders);
  _Projectiles.Update(realDt, 0.f, projectilesContext);
  if ( iInput._FirePressed )
    _Projectiles.RequestFire(iSettings, _Player);

  if ( dt <= 0.f )
    return 0;

  _Player._Yaw += iInput._MouseDeltaX * iSettings._MouseSensitivity;
  _Player._Pitch -= iInput._MouseDeltaY * iSettings._MouseSensitivity;

  if ( fabs(_Player._Yaw) > 360.f )
    _Player._Yaw -= MathUtil::Sign(_Player._Yaw) * 360.f * floor( fabs( _Player._Yaw / 360.f ) );
  _Player._Pitch = MathUtil::Clamp(_Player._Pitch, -89.f, 89.f);

  const float speed = iInput._Sprint ? iSettings._SprintSpeed : iSettings._MoveSpeed;

  if ( iSettings._FreeLook )
  {
    const Vec3 forward = PlayerForward();
    const Vec3 right = glm::normalize(glm::cross(forward, Vec3(0.f, 1.f, 0.f)));

    Vec3 wishDir(0.f);
    if ( iInput._MoveForward )
      wishDir += forward;
    if ( iInput._MoveBackward )
      wishDir -= forward;
    if ( iInput._MoveRight )
      wishDir += right;
    if ( iInput._MoveLeft )
      wishDir -= right;
    if ( iInput._MoveUp )
      wishDir += Vec3(0.f, 1.f, 0.f);
    if ( iInput._MoveDown )
      wishDir -= Vec3(0.f, 1.f, 0.f);

    const float wishLen = glm::length(wishDir);
    if ( wishLen > EPSILON )
      wishDir /= wishLen;

    _Player._Velocity = wishDir * speed;
    _Player._Position += _Player._Velocity * dt;
    _Player._Grounded = false;
  }
  else
  {
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
  }

  const float horizontalSpeed = glm::length(Vec2(_Player._Velocity.x, _Player._Velocity.z));
  const float yawRad = MathUtil::ToRadians(_Player._Yaw);
  const Vec3 right(-std::sin(yawRad), 0.f, std::cos(yawRad));
  FpsHeadBobUpdate headBobUpdate;
  headBobUpdate._DeltaTime = dt;
  headBobUpdate._HorizontalSpeed = horizontalSpeed;
  headBobUpdate._Grounded = _Player._Grounded;
  headBobUpdate._Enabled = !iSettings._FreeLook;
  headBobUpdate._Right = right;
  headBobUpdate._Up = Vec3(0.f, 1.f, 0.f);
  _Player._HeadBob.Update(iSettings._HeadBob, headBobUpdate);

  _Projectiles.Update(0.f, dt, projectilesContext);

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeProjectilePool
// ----------------------------------------------------------------------------
int FpsGameWorld::ResizeProjectilePool( const FpsGameSettings & iSettings )
{
  return _Projectiles.ResizePool(iSettings);
}

// ----------------------------------------------------------------------------
// ClearProjectiles
// ----------------------------------------------------------------------------
void FpsGameWorld::ClearProjectiles()
{
  _Projectiles.Clear();
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
// BuildDefaultArena
// ----------------------------------------------------------------------------
void FpsGameWorld::BuildDefaultArena()
{
  _Objects.clear();
  _PropCollisionColliders.clear();
  _SpawnPosition = Vec3(0.f, 0.05f, -8.f);
  _SpawnYaw = 90.f;
  _SpawnPitch = 0.f;
  _SpawnHealth = -1;
  _SpawnArmor = -1;

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
// BuildFromMap
// ----------------------------------------------------------------------------
void FpsGameWorld::BuildFromMap( const FpsGameMap & iMap )
{
  _Objects = iMap._Objects;
  _PropCollisionColliders.clear();
  _SpawnPosition = iMap._Player._Position;
  _SpawnYaw = iMap._Player._Yaw;
  _SpawnPitch = iMap._Player._Pitch;
  _SpawnHealth = iMap._Player._Health;
  _SpawnArmor = iMap._Player._Armor;
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

  for ( const FpsCollisionObb & collider : _PropCollisionColliders )
  {
    const Vec3 playerHalf(std::max(iSettings._PlayerRadius, 0.05f),
                          std::max(iSettings._PlayerHeight * 0.5f, 0.1f),
                          std::max(iSettings._PlayerRadius, 0.05f));
    const Vec3 playerCenter = _Player._Position + Vec3(0.f, playerHalf.y, 0.f);
    FpsCollisionAxisResult result;
    if ( !FpsCollision::ResolveAabbObbAxis(playerCenter, playerHalf, collider, iAxis, iDelta, result) )
      continue;

    _Player._Position[iAxis] += result._Correction;

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
  const Vec3 sumHalf = playerHalf + ObjectCollisionHalfExtents(iObject);

  if ( ( std::abs(delta.x) >= sumHalf.x )
    || ( std::abs(delta.y) >= sumHalf.y )
    || ( std::abs(delta.z) >= sumHalf.z ) )
    return false;

  oOverlap = sumHalf - Vec3(std::abs(delta.x), std::abs(delta.y), std::abs(delta.z));
  return true;
}

// Reset
// ----------------------------------------------------------------------------
void FpsGameSceneBinding::Reset()
{
  _CubeMeshID = -1;
  _SphereMeshID = -1;
  _ProjectileMaterialID = -1;
  for ( int i = 0; i < (int)FpsMaterialSlot::Count; ++i )
    _MaterialIDs[i] = -1;
  _MapMaterialIDs.clear();
  _ObjectInstanceIDs.clear();
  _PropInstanceIDs.clear();
  _PropBaseTransforms.clear();
  _ProjectileInstanceIDs.clear();
  _WeaponInstanceIDs.clear();
  _WeaponBaseTransforms.clear();
}

// ----------------------------------------------------------------------------
// Attach
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings )
{
  FpsGameMap fallbackMap;
  fallbackMap._Weapon._Path = "overwatch_junkrats_grenade_launcher/scene.gltf";
  return Attach(iScene, iWorld, iSettings, fallbackMap);
}

// ----------------------------------------------------------------------------
// Attach
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::Attach( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings, const FpsGameMap & iMap )
{
  Reset();

  if ( 0 != EnsureResources(iScene, &iMap) )
    return 1;

  if ( 0 != AddLights(iScene, iMap._Lights.empty() ? nullptr : &iMap) )
    return 1;

  if ( 0 != LoadProps(iScene, iMap) )
    std::cout << "FpsGameSceneBinding : failed to load one or more map props" << std::endl;

  if ( 0 != LoadViewWeapon(iScene, iMap._Weapon._Path) )
    std::cout << "FpsGameSceneBinding : failed to load view weapon model" << std::endl;

  const std::vector<FpsSceneObject> & objects = iWorld.GetObjects();
  _ObjectInstanceIDs.reserve(objects.size());

  for ( const FpsSceneObject & object : objects )
  {
    if ( !object._Visible )
    {
      _ObjectInstanceIDs.push_back(-1);
      continue;
    }

    const int materialID = MaterialID(object._MaterialName, object._Material);
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

  if ( 0 != SyncCamera(iScene, iWorld, iSettings) )
    return 1;

  return SyncTransforms(iScene, iWorld, iSettings);
}

// ----------------------------------------------------------------------------
// SyncCamera
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::SyncCamera( Scene & iScene, const FpsGameWorld & iWorld, const FpsGameSettings & iSettings )
{
  Camera & camera = iScene.GetCamera();
  const FpsPlayer & player = iWorld.GetPlayer();
  camera.SetFreeLookPose(player.ViewPosition(iSettings), player._Yaw, player._Pitch);
  camera.SetFOVInDegrees(MathUtil::Clamp(iSettings._CameraFOV, 30.f, 140.f));
  const float zNear = std::max(0.001f, iSettings._CameraZNear);
  const float zFar = std::max(zNear + 0.001f, iSettings._CameraZFar);
  camera.SetZNearFar(zNear, zFar);
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
    if ( instanceID < 0 )
      continue;
    if ( instanceID >= static_cast<int>(instances.size()) )
      return 1;

    instances[instanceID]._Visible = objects[i]._Visible;
    instances[instanceID]._Transform = BuildObjectTransform(objects[i]);
  }

  for ( int i = 0; i < static_cast<int>(_ProjectileInstanceIDs.size()); ++i )
  {
    const int instanceID = _ProjectileInstanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      return 1;

    instances[instanceID]._Visible = projectiles[i]._Active;
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

    instances[instanceID]._Visible = iSettings._ShowViewWeapon;
    instances[instanceID]._Transform = weaponTransform * _WeaponBaseTransforms[i];
  }

  return 0;
}

// ----------------------------------------------------------------------------
// SetObjectInstanceVisible
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::SetObjectInstanceVisible( Scene & iScene, int iObjectIndex, bool iVisible )
{
  if ( ( iObjectIndex < 0 ) || ( iObjectIndex >= static_cast<int>(_ObjectInstanceIDs.size()) ) )
    return 1;

  const int instanceID = _ObjectInstanceIDs[iObjectIndex];
  if ( instanceID < 0 )
    return 0;

  std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  if ( instanceID >= static_cast<int>(instances.size()) )
    return 1;

  instances[instanceID]._Visible = iVisible;
  return 0;
}

// ----------------------------------------------------------------------------
// SyncProp
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::SyncProp( Scene & iScene, const FpsGameMap & iMap, int iPropIndex )
{
  if ( ( iPropIndex < 0 ) || ( iPropIndex >= static_cast<int>(iMap._Props.size()) ) )
    return 1;
  if ( ( iPropIndex >= static_cast<int>(_PropInstanceIDs.size()) )
    || ( iPropIndex >= static_cast<int>(_PropBaseTransforms.size()) ) )
    return 1;

  const FpsMapProp & prop = iMap._Props[iPropIndex];
  const Mat4x4 propTransform = BuildPropTransform(prop);
  const std::vector<int> & instanceIDs = _PropInstanceIDs[iPropIndex];
  const std::vector<Mat4x4> & baseTransforms = _PropBaseTransforms[iPropIndex];
  if ( instanceIDs.size() != baseTransforms.size() )
    return 1;

  std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  for ( int i = 0; i < static_cast<int>(instanceIDs.size()); ++i )
  {
    const int instanceID = instanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      return 1;

    instances[instanceID]._Visible = prop._Visible;
    instances[instanceID]._Transform = propTransform * baseTransforms[i];
  }

  return 0;
}

// ----------------------------------------------------------------------------
// GetPropInstanceIDs
// ----------------------------------------------------------------------------
const std::vector<int> * FpsGameSceneBinding::GetPropInstanceIDs( int iPropIndex ) const
{
  if ( ( iPropIndex < 0 ) || ( iPropIndex >= static_cast<int>(_PropInstanceIDs.size()) ) )
    return nullptr;
  return &_PropInstanceIDs[iPropIndex];
}

// ----------------------------------------------------------------------------
// BuildPropCollisionColliders
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::BuildPropCollisionColliders( Scene & iScene, const FpsGameMap & iMap, std::vector<FpsCollisionObb> & oColliders ) const
{
  oColliders.clear();

  const std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  const std::vector<Mesh*> & meshes = iScene.GetMeshes();

  for ( int propIndex = 0; propIndex < static_cast<int>(iMap._Props.size()); ++propIndex )
  {
    const FpsMapProp & prop = iMap._Props[propIndex];
    if ( ( FpsPropCollisionMode::None == prop._CollisionMode ) || !prop._Visible )
      continue;

    if ( FpsPropCollisionMode::Compound == prop._CollisionMode )
    {
      const Mat4x4 propTransform = BuildPropTransform(prop);
      const Mat4x4 propRotation = BuildEulerTransform(Vec3(0.f), prop._Rotation, Vec3(1.f));
      for ( int colliderIndex = 0; colliderIndex < static_cast<int>(prop._Colliders.size()); ++colliderIndex )
      {
        const FpsMapPropCollider & collider = prop._Colliders[colliderIndex];
        const Mat4x4 colliderRotation = BuildEulerTransform(Vec3(0.f), collider._Rotation, Vec3(1.f));
        const Vec3 localAxes[3] = { Vec3(colliderRotation[0]), Vec3(colliderRotation[1]), Vec3(colliderRotation[2]) };
        Vec3 worldAxes[3];
        Vec3 scaledEdges[3];
        for ( int axis = 0; axis < 3; ++axis )
        {
          worldAxes[axis] = glm::normalize(Vec3(propRotation * Vec4(localAxes[axis], 0.f)));
          scaledEdges[axis] = Vec3(propRotation * Vec4(prop._Scale * ( localAxes[axis] * collider._HalfExtents[axis] ), 0.f));
        }

        Vec3 worldHalfExtents(0.f);
        for ( int axis = 0; axis < 3; ++axis )
        {
          for ( int edge = 0; edge < 3; ++edge )
            worldHalfExtents[axis] += std::abs(glm::dot(scaledEdges[edge], worldAxes[axis]));
        }

        oColliders.push_back(FpsCollision::MakeObb(propIndex, colliderIndex, MathUtil::TransformPoint(collider._Center, propTransform), worldAxes, worldHalfExtents));
      }
      continue;
    }

    if ( FpsPropCollisionMode::Bounds == prop._CollisionMode )
    {
      if ( propIndex >= static_cast<int>(_PropInstanceIDs.size()) )
        continue;

      const std::vector<int> & instanceIDs = _PropInstanceIDs[propIndex];
      for ( int instanceID : instanceIDs )
      {
        if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
          continue;

        const MeshInstance & instance = instances[instanceID];
        if ( !instance._Visible )
          continue;
        if ( ( instance._MeshID < 0 ) || ( instance._MeshID >= static_cast<int>(meshes.size()) ) )
          continue;

        const Mesh * mesh = meshes[instance._MeshID];
        if ( !mesh )
          continue;

        oColliders.push_back(FpsCollision::MakeObb(propIndex, -1, mesh -> GetBoundingBox(), instance._Transform));
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// BuildPropBoundsColliders
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::BuildPropBoundsColliders( Scene & iScene, const FpsGameMap & iMap, int iPropIndex, std::vector<FpsMapPropCollider> & oColliders ) const
{
  oColliders.clear();

  if ( ( iPropIndex < 0 ) || ( iPropIndex >= static_cast<int>(iMap._Props.size()) ) )
    return 1;
  if ( ( iPropIndex >= static_cast<int>(_PropInstanceIDs.size()) )
    || ( iPropIndex >= static_cast<int>(_PropBaseTransforms.size()) ) )
    return 1;

  const std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  const std::vector<Mesh*> & meshes = iScene.GetMeshes();
  const std::vector<int> & instanceIDs = _PropInstanceIDs[iPropIndex];
  const std::vector<Mat4x4> & baseTransforms = _PropBaseTransforms[iPropIndex];
  if ( instanceIDs.size() != baseTransforms.size() )
    return 1;

  for ( int i = 0; i < static_cast<int>(instanceIDs.size()); ++i )
  {
    const int instanceID = instanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      continue;

    const MeshInstance & instance = instances[instanceID];
    if ( !instance._Visible )
      continue;
    if ( ( instance._MeshID < 0 ) || ( instance._MeshID >= static_cast<int>(meshes.size()) ) )
      continue;

    const Mesh * mesh = meshes[instance._MeshID];
    if ( !mesh )
      continue;

    const AABB<Vec3> localBounds = TransformAABB(mesh -> GetBoundingBox(), baseTransforms[i]);
    FpsMapPropCollider collider;
    collider._Name = "Collider " + std::to_string(static_cast<int>(oColliders.size()));
    collider._Center = 0.5f * ( localBounds._Low + localBounds._High );
    collider._Rotation = Vec3(0.f);
    collider._HalfExtents = MathUtil::Max(0.5f * ( localBounds._High - localBounds._Low ), Vec3(0.001f));
    oColliders.push_back(collider);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// EnsureResources
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::EnsureResources( Scene & iScene, const FpsGameMap * iMap )
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

  Material projectile = MakeMaterial(Vec3(1.0f, 0.04f, 0.02f), 0.28f, 0.f, 0.75f);

  FpsGameMap materialMap = iMap ? *iMap : FpsGameMap();
  FpsGameMapLoader::SeedDefaultMaterials(materialMap);

  for ( const FpsMapMaterial & mapMaterial : materialMap._Materials )
  {
    Material material = mapMaterial._Material;
    const int materialID = iScene.AddMaterial(material, "__FpsMap_" + mapMaterial._Name);
    _MapMaterialIDs[mapMaterial._Name] = materialID;

    if ( "floor" == mapMaterial._Name )
      _MaterialIDs[(int)FpsMaterialSlot::Floor] = materialID;
    else if ( "wall" == mapMaterial._Name )
      _MaterialIDs[(int)FpsMaterialSlot::Wall] = materialID;
    else if ( "pillar" == mapMaterial._Name )
      _MaterialIDs[(int)FpsMaterialSlot::Pillar] = materialID;
    else if ( "crate" == mapMaterial._Name )
      _MaterialIDs[(int)FpsMaterialSlot::Crate] = materialID;
    else if ( "accent" == mapMaterial._Name )
      _MaterialIDs[(int)FpsMaterialSlot::Accent] = materialID;
  }

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
int FpsGameSceneBinding::LoadViewWeapon( Scene & iScene, const std::string & iPath )
{
  if ( iPath.empty() )
    return 1;

  const int firstInstanceID = iScene.GetNbMeshInstances();
  RenderSettings loaderSettings;

  if ( !Loader::LoadScene(PathUtils::GetAssetPath(iPath), iScene, loaderSettings) )
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
// LoadProps
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::LoadProps( Scene & iScene, const FpsGameMap & iMap )
{
  int result = 0;
  for ( int propIndex = 0; propIndex < static_cast<int>(iMap._Props.size()); ++propIndex )
  {
    if ( 0 != LoadProp(iScene, iMap, propIndex) )
      result = 1;
  }

  return result;
}

// ----------------------------------------------------------------------------
// LoadProp
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::LoadProp( Scene & iScene, const FpsGameMap & iMap, int iPropIndex )
{
  if ( ( iPropIndex < 0 ) || ( iPropIndex >= static_cast<int>(iMap._Props.size()) ) )
    return 1;

  if ( iPropIndex >= static_cast<int>(_PropInstanceIDs.size()) )
    _PropInstanceIDs.resize(iPropIndex + 1);
  if ( iPropIndex >= static_cast<int>(_PropBaseTransforms.size()) )
    _PropBaseTransforms.resize(iPropIndex + 1);

  _PropInstanceIDs[iPropIndex].clear();
  _PropBaseTransforms[iPropIndex].clear();

  const FpsMapProp & prop = iMap._Props[iPropIndex];
  const std::filesystem::path filepath(GetPropFilePath(prop._Path));
  std::string ext = filepath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), []( unsigned char c ) { return static_cast<char>(std::tolower(c)); });

  if ( ".obj" == ext )
  {
    const int meshID = iScene.AddMesh(filepath.string());
    const int materialID = MaterialID(FpsMaterialSlot::Wall);
    if ( ( meshID < 0 ) || ( materialID < 0 ) )
    {
      std::cout << "FpsGameSceneBinding : failed to load obj prop " << prop._Path << std::endl;
      return 1;
    }

    MeshInstance instance(prop._Name, meshID, materialID, BuildPropTransform(prop));
    instance._Visible = prop._Visible;
    const int instanceID = iScene.AddMeshInstance(instance);
    _PropInstanceIDs[iPropIndex].push_back(instanceID);
    _PropBaseTransforms[iPropIndex].push_back(Mat4x4(1.f));
    return 0;
  }

  if ( ( ".gltf" != ext ) && ( ".glb" != ext ) )
  {
    std::cout << "FpsGameSceneBinding : unsupported prop format " << prop._Path << std::endl;
    return 1;
  }

  Scene propScene;
  RenderSettings loaderSettings;
  if ( !Loader::LoadScene(filepath.string(), propScene, loaderSettings) )
  {
    std::cout << "FpsGameSceneBinding : failed to load prop " << prop._Path << std::endl;
    return 1;
  }

  std::vector<int> textureIDs(propScene.GetNbTextures(), -1);
  for ( int textureID = 0; textureID < propScene.GetNbTextures(); ++textureID )
  {
    Texture * texture = propScene.GetTextures()[textureID];
    if ( !texture )
      continue;

    const std::string textureName = MakePropResourceName(prop, iPropIndex, texture -> Filename());
    const int dstTextureID = iScene.AddTexture(textureName, texture -> GetUCData(), texture -> GetWidth(), texture -> GetHeight(), texture -> GetNbComponents());
    if ( dstTextureID < 0 )
    {
      std::cout << "FpsGameSceneBinding : failed to merge prop texture " << textureName << std::endl;
      return 1;
    }
    textureIDs[textureID] = dstTextureID;
  }

  std::vector<int> materialIDs(propScene.GetNbMaterials(), -1);
  for ( int materialID = 0; materialID < propScene.GetNbMaterials(); ++materialID )
  {
    Material material = propScene.GetMaterials()[materialID];
    if ( 0 != RemapMaterialTextureID(material._BaseColorTexId, textureIDs)
      || 0 != RemapMaterialTextureID(material._MetallicRoughnessTexID, textureIDs)
      || 0 != RemapMaterialTextureID(material._NormalMapTexID, textureIDs)
      || 0 != RemapMaterialTextureID(material._EmissionMapTexID, textureIDs) )
    {
      std::cout << "FpsGameSceneBinding : failed to remap prop material textures " << prop._Path << std::endl;
      return 1;
    }

    std::string materialName = propScene.FindMaterialName(materialID);
    if ( materialName.empty() )
      materialName = "Material_" + std::to_string(materialID);

    const int dstMaterialID = iScene.AddMaterial(material, MakePropResourceName(prop, iPropIndex, materialName));
    if ( dstMaterialID < 0 )
    {
      std::cout << "FpsGameSceneBinding : failed to merge prop material " << materialName << std::endl;
      return 1;
    }
    materialIDs[materialID] = dstMaterialID;
  }

  std::vector<int> meshIDs(propScene.GetNbMeshes(), -1);
  for ( int meshID = 0; meshID < propScene.GetNbMeshes(); ++meshID )
  {
    Mesh * mesh = propScene.GetMeshes()[meshID];
    if ( !mesh )
      continue;

    Mesh * newMesh = new Mesh(MakePropResourceName(prop, iPropIndex, mesh -> Filename()),
                              mesh -> GetVertices(),
                              mesh -> GetNormals(),
                              mesh -> GetUVs(),
                              mesh -> GetIndices());
    const int dstMeshID = iScene.AddMesh(newMesh);
    if ( dstMeshID < 0 )
    {
      delete newMesh;
      std::cout << "FpsGameSceneBinding : failed to merge prop mesh " << mesh -> Filename() << std::endl;
      return 1;
    }
    meshIDs[meshID] = dstMeshID;
  }

  const Mat4x4 propTransform = BuildPropTransform(prop);
  std::vector<MeshInstance> & propInstances = propScene.GetMeshInstances();
  for ( int instanceID = 0; instanceID < static_cast<int>(propInstances.size()); ++instanceID )
  {
    MeshInstance instance = propInstances[instanceID];
    if ( ( instance._MeshID < 0 ) || ( instance._MeshID >= static_cast<int>(meshIDs.size()) )
      || ( instance._MaterialID < 0 ) || ( instance._MaterialID >= static_cast<int>(materialIDs.size()) ) )
    {
      std::cout << "FpsGameSceneBinding : failed to remap prop instance " << prop._Path << std::endl;
      return 1;
    }

    instance._Filename = MakePropResourceName(prop, iPropIndex, instance._Filename);
    instance._MeshID = meshIDs[instance._MeshID];
    instance._MaterialID = materialIDs[instance._MaterialID];
    instance._Visible = prop._Visible;

    _PropBaseTransforms[iPropIndex].push_back(instance._Transform);
    instance._Transform = propTransform * _PropBaseTransforms[iPropIndex].back();

    const int dstInstanceID = iScene.AddMeshInstance(instance);
    _PropInstanceIDs[iPropIndex].push_back(dstInstanceID);
  }

  if ( _PropInstanceIDs[iPropIndex].empty() )
  {
    std::cout << "FpsGameSceneBinding : prop loaded no instances " << prop._Path << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// AddLights
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::AddLights( Scene & iScene, const FpsGameMap * iMap )
{
  if ( iMap && !iMap -> _Lights.empty() )
  {
    for ( const Light & light : iMap -> _Lights )
      iScene.AddLight(light);
    return 0;
  }

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
  return BuildSceneObjectTransform(iObject, iObject._HalfExtents * 2.f);
}

// ----------------------------------------------------------------------------
// BuildProjectileTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsGameSceneBinding::BuildProjectileTransform( const FpsProjectile & iProjectile, const FpsGameSettings & iSettings ) const
{
  if ( !iProjectile._Active )
    return glm::translate(iProjectile._Position) * glm::scale(Vec3(0.001f));

  const float radius = std::max(0.02f, iSettings._ProjectileRadius);
  return glm::translate(iProjectile._Position) * glm::scale(Vec3(radius * 2.f));
}

// ----------------------------------------------------------------------------
// BuildViewWeaponTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsGameSceneBinding::BuildViewWeaponTransform( const FpsPlayer & iPlayer, const FpsGameSettings & iSettings ) const
{
  if ( !iSettings._ShowViewWeapon )
    return glm::translate(iPlayer.ViewPosition(iSettings)) * glm::scale(Vec3(std::max(0.001f, iSettings._ViewWeaponScale)));

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
  cameraTransform[3] = Vec4(iPlayer.ViewPosition(iSettings), 1.f);

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
// BuildPropTransform
// ----------------------------------------------------------------------------
Mat4x4 FpsGameSceneBinding::BuildPropTransform( const FpsMapProp & iProp ) const
{
  return BuildEulerTransform(iProp._Position, iProp._Rotation, iProp._Scale);
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

// ----------------------------------------------------------------------------
// MaterialID
// ----------------------------------------------------------------------------
int FpsGameSceneBinding::MaterialID( const std::string & iMaterialName, FpsMaterialSlot iFallback ) const
{
  const auto found = _MapMaterialIDs.find(iMaterialName);
  if ( found != _MapMaterialIDs.end() )
    return found -> second;

  return MaterialID(iFallback);
}

}
