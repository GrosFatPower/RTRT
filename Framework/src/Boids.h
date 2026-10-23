#ifndef _Boids_
#define _Boids_

#include "MathUtil.h"

#include <vector>

namespace RTRT
{

class Scene;

struct BoidSettings
{
  int          _Count            = 128;
  unsigned int _Seed             = 1337;
  Vec3         _BoundsCenter     = Vec3(0.f);
  float        _BoundsRadius     = 4.f;
  float        _BoundsHeight     = 3.f;
  float        _MinSpeed         = 0.5f;
  float        _MaxSpeed         = 2.5f;
  float        _MaxForce         = 4.f;
  float        _NeighborRadius   = 1.2f;
  float        _SeparationRadius = 0.35f;
  float        _SeparationWeight = 1.6f;
  float        _AlignmentWeight  = 1.f;
  float        _CohesionWeight   = 1.f;
  float        _BoundsWeight     = 2.f;
  float        _Scale            = 0.12f;
  Vec3         _Color            = Vec3(0.95f, 0.35f, 0.12f);
  bool         _Paused           = false;
};

struct BoidState
{
  Vec3 _Position = Vec3(0.f);
  Vec3 _Velocity = Vec3(0.f, 0.f, 1.f);
};

class BoidSimulation
{
public:
  int Initialize( const BoidSettings & iSettings );
  int Reset( const BoidSettings & iSettings );
  int Resize( const BoidSettings & iSettings );
  int Update( float iDeltaTime, const BoidSettings & iSettings );

  const std::vector<BoidState> & GetBoids() const { return _Boids; }

protected:
  Vec3 RandomPosition( const BoidSettings & iSettings );
  Vec3 RandomVelocity( const BoidSettings & iSettings );

  Vec3 LimitLength( const Vec3 & iVec, float iMaxLength ) const;
  Vec3 ClampSpeed( const Vec3 & iVelocity, const BoidSettings & iSettings ) const;
  Vec3 SteerTowards( const Vec3 & iCurrentVelocity, const Vec3 & iDesiredVelocity, const BoidSettings & iSettings ) const;

protected:
  std::vector<BoidState> _Boids;
  unsigned int           _Seed = 0;
  unsigned int           _RandomState = 0;
};

class BoidSceneBinding
{
public:
  int Attach( Scene & iScene, const BoidSettings & iSettings );
  int Detach( Scene & iScene );
  int SyncMaterial( Scene & iScene, const BoidSettings & iSettings );
  int SetInstancesVisible( Scene & iScene, bool iVisible );
  int SyncTransforms( Scene & iScene, const BoidSimulation & iSimulation, const BoidSettings & iSettings );
  bool ContainsInstanceID( int iInstanceID ) const;
  bool Attached() const { return !_InstanceIDs.empty(); }
  void Reset();

protected:
  int EnsureSceneResources( Scene & iScene, const BoidSettings & iSettings );
  int ResizeInstances( Scene & iScene, const BoidSettings & iSettings );
  Mat4x4 BuildTransform( const BoidState & iBoid, float iScale ) const;

protected:
  int              _MeshID = -1;
  int              _MaterialID = -1;
  std::vector<int> _InstanceIDs;
};

}

#endif /* _Boids_ */
