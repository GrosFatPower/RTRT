#ifndef _Bvh_
#define _Bvh_

#include "Math.h"
#include "split_bvh.h"
#include <vector>

namespace RTRT
{

class Mesh;

class Bvh
{
public:

  struct Node
  {
    Vec3 _BBoxMin;
    Vec3 _BBoxMax;
    Vec3 _LcRcLeaf; // LeftChildren/RightChildren/Leaf=0  FirstPrimitiveIdx/NbPrimitives/Leaf=1
  };

  virtual int ProcessNodes(RadeonRays::Bvh::Node * iNode) = 0;

  const std::vector<Node> GetNodes() const { return _Nodes; }

public:
  Bvh() {}
  virtual ~Bvh();

protected:

  int               _CurNode = 0;
  std::vector<Node> _Nodes;
};

class TLAS : public Bvh
{
public:
  TLAS() {}
  virtual ~TLAS();

private:

};

class BLAS : public Bvh
{
public:
  BLAS() {}
  virtual ~BLAS();

  int Build( Mesh * iMesh );

  int ProcessNodes(RadeonRays::Bvh::Node * iNode);

  const std::vector<Vec3i> GetTriangleIdx() const { return _TriangleIdx; }

private:

  std::vector<Vec3i> _TriangleIdx;
};

}

#endif /* _Bvh_ */
