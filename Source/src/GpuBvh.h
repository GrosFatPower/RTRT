#ifndef _GpuBvh_
#define _GpuBvh_

#include "MathUtil.h"
#include "MeshInstance.h"
#include "split_bvh.h"
#include <vector>

namespace RTRT
{

class Mesh;

class GpuBvh
{
public:

  struct Node
  {
    Vec3 _BBoxMin;
    Vec3 _BBoxMax;
    Vec3 _LcRcLeaf; // LeftChildren/RightChildren/Leaf=0  FirstPrimitiveIdx/NbPrimitives/Leaf=-1 (TLAS) FirstTriangleIdx/NbTriangles/Leaf=1 (BLAS)
  };

  const std::vector<Node> & GetNodes() const { return _Nodes; }

public:
  GpuBvh() {}
  virtual ~GpuBvh();

protected:

  virtual int ProcessNodes(RadeonRays::Bvh::Node * iNode) = 0;

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

  int ProcessNodes(RadeonRays::Bvh::Node * iNode);

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

  int ProcessNodes(RadeonRays::Bvh::Node * iNode);

  std::vector<Vec3i> _PackedTriangleIdx;
};

}

#endif /* _GpuBvh_ */
