#ifndef _GpuBvh_
#define _GpuBvh_

#include "MathUtil.h"
#include "MeshInstance.h"
#include <vector>

namespace RTRT
{

class Mesh;

class GpuBvh
{
public:

  GpuBvh() {};
  virtual ~GpuBvh() = 0;

  struct Node
  {
    Vec3 _BBoxMin;
    Vec3 _BBoxMax;
    Vec3 _LcRcLeaf; // LeftChildren/RightChildren/Leaf=0  FirstPrimitiveIdx/NbPrimitives/Leaf=-1 (TLAS) FirstTriangleIdx/NbTriangles/Leaf=1 (BLAS)
  };

  int               _CurNode = 0;
  std::vector<Node> _Nodes;
};

class GpuTLAS : public GpuBvh
{
public:
  GpuTLAS() {}
  virtual ~GpuTLAS();

  int Build( std::vector<Mesh*> & iMeshes , std::vector<MeshInstance> & iMeshInstances );

  const std::vector<MeshInstance> & GetPackedMeshInstances() const { return _PackedMeshInstances; }

private:

  std::vector<MeshInstance> _PackedMeshInstances;
};

class GpuBLAS : public GpuBvh
{
public:
  GpuBLAS() {}
  virtual ~GpuBLAS();

  int Build( Mesh & iMesh );

  const std::vector<Vec3i> & GetPackedTriangleIdx() const { return _PackedTriangleIdx; }

private:

  std::vector<Vec3i> _PackedTriangleIdx;
};

}

#endif /* _GpuBvh_ */
