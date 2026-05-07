#include "FpsGame.h"

#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "ProceduralMesh.h"
#include "Scene.h"

#include <algorithm>
#include <cmath>

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

static Material MakeMaterial( const Vec3 & iAlbedo, float iRoughness, float iMetallic = 0.f )
{
  Material material;
  material._Albedo = iAlbedo;
  material._Roughness = iRoughness;
  material._Metallic = iMetallic;
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
  _Player._Health = 100;

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int FpsGameWorld::Update( float iDeltaTime, const FpsGameInput & iInput, const FpsGameSettings & iSettings )
{
  if ( iInput._ResetPressed )
    Reset(iSettings);

  const float dt = MathUtil::Clamp(iDeltaTime, 0.f, 0.05f);
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

  return 0;
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
  for ( int i = 0; i < (int)FpsMaterialSlot::Count; ++i )
    _MaterialIDs[i] = -1;
  _ObjectInstanceIDs.clear();
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
int FpsGameSceneBinding::SyncTransforms( Scene & iScene, const FpsGameWorld & iWorld )
{
  const std::vector<FpsSceneObject> & objects = iWorld.GetObjects();
  if ( objects.size() != _ObjectInstanceIDs.size() )
    return 1;

  std::vector<MeshInstance> & instances = iScene.GetMeshInstances();
  for ( int i = 0; i < static_cast<int>(_ObjectInstanceIDs.size()); ++i )
  {
    const int instanceID = _ObjectInstanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(instances.size()) ) )
      return 1;

    instances[instanceID]._Transform = BuildObjectTransform(objects[i]);
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

  Material floor = MakeMaterial(Vec3(0.48f, 0.46f, 0.40f), 0.72f);
  Material wall = MakeMaterial(Vec3(0.34f, 0.37f, 0.42f), 0.65f);
  Material pillar = MakeMaterial(Vec3(0.58f, 0.55f, 0.49f), 0.55f);
  Material crate = MakeMaterial(Vec3(0.80f, 0.24f, 0.13f), 0.45f);
  Material accent = MakeMaterial(Vec3(0.14f, 0.45f, 0.62f), 0.50f);

  _MaterialIDs[(int)FpsMaterialSlot::Floor] = iScene.AddMaterial(floor, "__FpsFloor");
  _MaterialIDs[(int)FpsMaterialSlot::Wall] = iScene.AddMaterial(wall, "__FpsWall");
  _MaterialIDs[(int)FpsMaterialSlot::Pillar] = iScene.AddMaterial(pillar, "__FpsPillar");
  _MaterialIDs[(int)FpsMaterialSlot::Crate] = iScene.AddMaterial(crate, "__FpsCrate");
  _MaterialIDs[(int)FpsMaterialSlot::Accent] = iScene.AddMaterial(accent, "__FpsAccent");

  for ( int i = 0; i < (int)FpsMaterialSlot::Count; ++i )
  {
    if ( _MaterialIDs[i] < 0 )
      return 1;
  }

  return 0;
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
