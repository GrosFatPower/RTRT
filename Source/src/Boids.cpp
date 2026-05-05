#include "Boids.h"

#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "Scene.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Local helpers
// ----------------------------------------------------------------------------
static float BoidRand01( unsigned int & ioState )
{
  ioState = 1664525u * ioState + 1013904223u;
  return ( ioState & 0x00FFFFFFu ) / float(0x01000000u);
}

static float BoidRandRange( unsigned int & ioState, float iMin, float iMax )
{
  return iMin + ( iMax - iMin ) * BoidRand01(ioState);
}

static Vec3 SafeNormalize( const Vec3 & iVec, const Vec3 & iFallback )
{
  const float len = glm::length(iVec);
  if ( len <= EPSILON )
    return iFallback;
  return iVec / len;
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int BoidSimulation::Initialize( const BoidSettings & iSettings )
{
  return Reset(iSettings);
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
int BoidSimulation::Reset( const BoidSettings & iSettings )
{
  _Seed = iSettings._Seed;
  _RandomState = _Seed ? _Seed : 1u;
  _Boids.clear();
  _Boids.resize(std::max(iSettings._Count, 0));

  for ( BoidState & boid : _Boids )
  {
    boid._Position = RandomPosition(iSettings);
    boid._Velocity = RandomVelocity(iSettings);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Resize
// ----------------------------------------------------------------------------
int BoidSimulation::Resize( const BoidSettings & iSettings )
{
  const int newCount = std::max(iSettings._Count, 0);
  const int oldCount = static_cast<int>(_Boids.size());

  if ( newCount == oldCount )
    return 0;

  _Boids.resize(newCount);

  for ( int i = oldCount; i < newCount; ++i )
  {
    _Boids[i]._Position = RandomPosition(iSettings);
    _Boids[i]._Velocity = RandomVelocity(iSettings);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int BoidSimulation::Update( float iDeltaTime, const BoidSettings & iSettings )
{
  if ( _Boids.empty() )
    return 0;

  const float dt = MathUtil::Clamp(iDeltaTime, 0.f, 0.05f);
  const float neighborRadius2 = iSettings._NeighborRadius * iSettings._NeighborRadius;
  const float separationRadius2 = iSettings._SeparationRadius * iSettings._SeparationRadius;
  const float halfHeight = std::max(iSettings._BoundsHeight * 0.5f, 0.001f);

  std::vector<Vec3> accelerations(_Boids.size(), Vec3(0.f));

  for ( int i = 0; i < static_cast<int>(_Boids.size()); ++i )
  {
    const BoidState & boid = _Boids[i];

    Vec3 separation(0.f);
    Vec3 alignment(0.f);
    Vec3 cohesion(0.f);
    int neighborCount = 0;
    int separationCount = 0;

    for ( int j = 0; j < static_cast<int>(_Boids.size()); ++j )
    {
      if ( i == j )
        continue;

      const Vec3 offset = _Boids[j]._Position - boid._Position;
      const float dist2 = glm::dot(offset, offset);
      if ( dist2 > neighborRadius2 )
        continue;

      alignment += _Boids[j]._Velocity;
      cohesion += _Boids[j]._Position;
      neighborCount++;

      if ( dist2 < separationRadius2 )
      {
        const float dist = std::sqrt(std::max(dist2, EPSILON));
        separation -= offset / ( dist * dist );
        separationCount++;
      }
    }

    Vec3 acceleration(0.f);

    if ( separationCount > 0 )
    {
      Vec3 desired = SafeNormalize(separation / float(separationCount), boid._Velocity) * iSettings._MaxSpeed;
      acceleration += SteerTowards(boid._Velocity, desired, iSettings) * iSettings._SeparationWeight;
    }

    if ( neighborCount > 0 )
    {
      Vec3 desiredAlignment = SafeNormalize(alignment / float(neighborCount), boid._Velocity) * iSettings._MaxSpeed;
      acceleration += SteerTowards(boid._Velocity, desiredAlignment, iSettings) * iSettings._AlignmentWeight;

      Vec3 center = cohesion / float(neighborCount);
      Vec3 desiredCohesion = SafeNormalize(center - boid._Position, boid._Velocity) * iSettings._MaxSpeed;
      acceleration += SteerTowards(boid._Velocity, desiredCohesion, iSettings) * iSettings._CohesionWeight;
    }

    const Vec3 toCenter = iSettings._BoundsCenter - boid._Position;
    const Vec2 horizontal(toCenter.x, toCenter.z);
    const float horizontalDist = glm::length(horizontal);
    const float verticalDist = std::abs(boid._Position.y - iSettings._BoundsCenter.y);
    if ( ( horizontalDist > iSettings._BoundsRadius ) || ( verticalDist > halfHeight ) )
    {
      Vec3 desiredBounds = SafeNormalize(toCenter, -boid._Velocity) * iSettings._MaxSpeed;
      acceleration += SteerTowards(boid._Velocity, desiredBounds, iSettings) * iSettings._BoundsWeight;
    }

    accelerations[i] = LimitLength(acceleration, iSettings._MaxForce);
  }

  for ( int i = 0; i < static_cast<int>(_Boids.size()); ++i )
  {
    BoidState & boid = _Boids[i];
    boid._Velocity = ClampSpeed(boid._Velocity + accelerations[i] * dt, iSettings);
    boid._Position += boid._Velocity * dt;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RandomPosition
// ----------------------------------------------------------------------------
Vec3 BoidSimulation::RandomPosition( const BoidSettings & iSettings )
{
  const float radius = std::max(iSettings._BoundsRadius, 0.001f);
  const float height = std::max(iSettings._BoundsHeight, 0.001f);
  const float angle = BoidRandRange(_RandomState, 0.f, 2.f * static_cast<float>(M_PI));
  const float r = std::sqrt(BoidRand01(_RandomState)) * radius;

  return iSettings._BoundsCenter
       + Vec3(std::cos(angle) * r,
              BoidRandRange(_RandomState, -0.5f * height, 0.5f * height),
              std::sin(angle) * r);
}

// ----------------------------------------------------------------------------
// RandomVelocity
// ----------------------------------------------------------------------------
Vec3 BoidSimulation::RandomVelocity( const BoidSettings & iSettings )
{
  Vec3 dir(BoidRandRange(_RandomState, -1.f, 1.f),
           BoidRandRange(_RandomState, -0.25f, 0.25f),
           BoidRandRange(_RandomState, -1.f, 1.f));
  dir = SafeNormalize(dir, Vec3(0.f, 0.f, 1.f));

  return dir * BoidRandRange(_RandomState, iSettings._MinSpeed, iSettings._MaxSpeed);
}

// ----------------------------------------------------------------------------
// LimitLength
// ----------------------------------------------------------------------------
Vec3 BoidSimulation::LimitLength( const Vec3 & iVec, float iMaxLength ) const
{
  const float len = glm::length(iVec);
  if ( ( len <= EPSILON ) || ( len <= iMaxLength ) )
    return iVec;
  return iVec * ( iMaxLength / len );
}

// ----------------------------------------------------------------------------
// ClampSpeed
// ----------------------------------------------------------------------------
Vec3 BoidSimulation::ClampSpeed( const Vec3 & iVelocity, const BoidSettings & iSettings ) const
{
  const float len = glm::length(iVelocity);
  if ( len <= EPSILON )
    return Vec3(0.f, 0.f, std::max(iSettings._MinSpeed, 0.001f));

  const float minSpeed = std::max(iSettings._MinSpeed, 0.001f);
  const float maxSpeed = std::max(iSettings._MaxSpeed, minSpeed);
  const float clampedLen = MathUtil::Clamp(len, minSpeed, maxSpeed);
  return iVelocity * ( clampedLen / len );
}

// ----------------------------------------------------------------------------
// SteerTowards
// ----------------------------------------------------------------------------
Vec3 BoidSimulation::SteerTowards( const Vec3 & iCurrentVelocity, const Vec3 & iDesiredVelocity, const BoidSettings & iSettings ) const
{
  return LimitLength(iDesiredVelocity - iCurrentVelocity, iSettings._MaxForce);
}

// ----------------------------------------------------------------------------
// Reset
// ----------------------------------------------------------------------------
void BoidSceneBinding::Reset()
{
  _MeshID = -1;
  _MaterialID = -1;
  _InstanceIDs.clear();
}

// ----------------------------------------------------------------------------
// Attach
// ----------------------------------------------------------------------------
int BoidSceneBinding::Attach( Scene & iScene, const BoidSettings & iSettings )
{
  if ( 0 != EnsureSceneResources(iScene) )
    return 1;

  return ResizeInstances(iScene, iSettings);
}

// ----------------------------------------------------------------------------
// Detach
// ----------------------------------------------------------------------------
int BoidSceneBinding::Detach( Scene & iScene )
{
  std::vector<MeshInstance> & meshInstances = iScene.GetMeshInstances();
  std::sort(_InstanceIDs.begin(), _InstanceIDs.end(), std::greater<int>());

  for ( int instanceID : _InstanceIDs )
  {
    if ( ( instanceID >= 0 ) && ( instanceID < static_cast<int>(meshInstances.size()) ) )
      meshInstances.erase(meshInstances.begin() + instanceID);
  }

  _InstanceIDs.clear();
  return 0;
}

// ----------------------------------------------------------------------------
// SyncTransforms
// ----------------------------------------------------------------------------
int BoidSceneBinding::SyncTransforms( Scene & iScene, const BoidSimulation & iSimulation, const BoidSettings & iSettings )
{
  const std::vector<BoidState> & boids = iSimulation.GetBoids();
  if ( boids.size() != _InstanceIDs.size() )
    return 1;

  std::vector<MeshInstance> & meshInstances = iScene.GetMeshInstances();
  for ( int i = 0; i < static_cast<int>(_InstanceIDs.size()); ++i )
  {
    const int instanceID = _InstanceIDs[i];
    if ( ( instanceID < 0 ) || ( instanceID >= static_cast<int>(meshInstances.size()) ) )
      return 1;

    meshInstances[instanceID]._Transform = BuildTransform(boids[i], iSettings._Scale);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ContainsInstanceID
// ----------------------------------------------------------------------------
bool BoidSceneBinding::ContainsInstanceID( int iInstanceID ) const
{
  return std::find(_InstanceIDs.begin(), _InstanceIDs.end(), iInstanceID) != _InstanceIDs.end();
}

// ----------------------------------------------------------------------------
// EnsureSceneResources
// ----------------------------------------------------------------------------
int BoidSceneBinding::EnsureSceneResources( Scene & iScene )
{
  if ( _MaterialID < 0 )
  {
    Material material;
    material._Albedo = Vec3(0.95f, 0.35f, 0.12f);
    material._Roughness = 0.55f;
    material._Metallic = 0.f;
    _MaterialID = iScene.AddMaterial(material, "__BoidMaterial");
  }

  if ( _MeshID < 0 )
  {
    std::vector<Vec3> vertices;
    vertices.push_back(Vec3( 0.00f,  0.00f,  0.70f));
    vertices.push_back(Vec3(-0.22f, -0.12f, -0.45f));
    vertices.push_back(Vec3( 0.22f, -0.12f, -0.45f));
    vertices.push_back(Vec3( 0.22f,  0.12f, -0.45f));
    vertices.push_back(Vec3(-0.22f,  0.12f, -0.45f));

    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    uvs.push_back(Vec2(0.f));

    std::vector<Vec3i> indices;
    auto addTri = [&]( int iA, int iB, int iC )
    {
      const Vec3 normal = SafeNormalize(glm::cross(vertices[iB] - vertices[iA], vertices[iC] - vertices[iA]), Vec3(0.f, 1.f, 0.f));
      const int normalID = static_cast<int>(normals.size());
      normals.push_back(normal);
      indices.push_back(Vec3i(iA, normalID, 0));
      indices.push_back(Vec3i(iB, normalID, 0));
      indices.push_back(Vec3i(iC, normalID, 0));
    };

    addTri(0, 1, 2);
    addTri(0, 2, 3);
    addTri(0, 3, 4);
    addTri(0, 4, 1);
    addTri(1, 4, 3);
    addTri(1, 3, 2);

    Mesh * mesh = new Mesh("__BoidArrowMesh", vertices, normals, uvs, indices);
    _MeshID = iScene.AddMesh(mesh);
    if ( _MeshID < 0 )
    {
      delete mesh;
      return 1;
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeInstances
// ----------------------------------------------------------------------------
int BoidSceneBinding::ResizeInstances( Scene & iScene, const BoidSettings & iSettings )
{
  if ( ( _MeshID < 0 ) || ( _MaterialID < 0 ) )
    return 1;

  std::vector<MeshInstance> & meshInstances = iScene.GetMeshInstances();
  const int targetCount = std::max(iSettings._Count, 0);

  while ( static_cast<int>(_InstanceIDs.size()) > targetCount )
  {
    const int instanceID = _InstanceIDs.back();
    _InstanceIDs.pop_back();
    if ( ( instanceID >= 0 ) && ( instanceID < static_cast<int>(meshInstances.size()) ) )
      meshInstances.erase(meshInstances.begin() + instanceID);
  }

  while ( static_cast<int>(_InstanceIDs.size()) < targetCount )
  {
    MeshInstance meshInstance("__Boid", _MeshID, _MaterialID, Mat4x4(1.f));
    const int instanceID = iScene.AddMeshInstance(meshInstance);
    _InstanceIDs.push_back(instanceID);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// BuildTransform
// ----------------------------------------------------------------------------
Mat4x4 BoidSceneBinding::BuildTransform( const BoidState & iBoid, float iScale ) const
{
  const Vec3 forward = SafeNormalize(iBoid._Velocity, Vec3(0.f, 0.f, 1.f));
  Vec3 worldUp(0.f, 1.f, 0.f);
  if ( std::abs(glm::dot(forward, worldUp)) > 0.95f )
    worldUp = Vec3(1.f, 0.f, 0.f);

  const Vec3 right = SafeNormalize(glm::cross(worldUp, forward), Vec3(1.f, 0.f, 0.f));
  const Vec3 up = SafeNormalize(glm::cross(forward, right), Vec3(0.f, 1.f, 0.f));
  const float scale = std::max(iScale, 0.001f);

  Mat4x4 transform(1.f);
  transform[0] = Vec4(right * scale, 0.f);
  transform[1] = Vec4(up * scale, 0.f);
  transform[2] = Vec4(forward * scale, 0.f);
  transform[3] = Vec4(iBoid._Position, 1.f);
  return transform;
}

}
