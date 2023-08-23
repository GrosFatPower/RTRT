#ifndef _GpuBvh_
#define _GpuBvh_

#include "MathUtil.h"
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
    Vec3 _LcRcLeaf; // LeftChildren/RightChildren/Leaf=0  FirstPrimitiveIdx/NbPrimitives/Leaf=1
  };

  virtual int ProcessNodes(RadeonRays::Bvh::Node * iNode) = 0;

  const std::vector<Node> & GetNodes() const { return _Nodes; }

public:
  GpuBvh() {}
  virtual ~GpuBvh();

protected:

  int               _CurNode = 0;
  std::vector<Node> _Nodes;
};

class GpuTLAS : public GpuBvh
{
public:
  GpuTLAS() {}
  virtual ~GpuTLAS();

private:

};

class GpuBLAS : public GpuBvh
{
public:
  GpuBLAS() {}
  virtual ~GpuBLAS();

  int Build( Mesh * iMesh );

  int ProcessNodes(RadeonRays::Bvh::Node * iNode);

  const std::vector<Vec3i> GetTriangleIdx() const { return _TriangleIdx; }

private:

  std::vector<Vec3i> _TriangleIdx;
};

}

#endif /* _GpuBvh_ */
