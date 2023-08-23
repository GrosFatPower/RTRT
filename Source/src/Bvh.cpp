#include "Bvh.h"
#include "Mesh.h"
#include <iostream>
#include <memory>

namespace RTRT
{

Bvh::~Bvh()
{

}

BLAS::~BLAS()
{

}

int BLAS::Build( Mesh * iMesh )
{
  if ( !iMesh )
    return 1;

  // 1. Compute BVH
  const int nbTris = iMesh -> GetIndices().size() / 3;

  std::vector<RadeonRays::bbox> bounds(nbTris);

//#pragma omp parallel for
  for ( int i = 0; i < nbTris; ++i )
  {
    for ( int j = 0; j < 3; ++j )
    {
      Vec3i indices = iMesh -> GetIndices()[i];
      bounds[i].grow(iMesh -> GetVertices()[indices.x]);
    }
  }

  std::unique_ptr<RadeonRays::SplitBvh> bvh = std::make_unique<RadeonRays::SplitBvh>(2.0f, 64, 0, 0.001f, 0.f);

  bvh -> Build(&bounds[0], nbTris);

  // 2. Remplissage du BVH

  _Nodes.clear();
  _Nodes.resize(bvh -> GetNodeCount());
  ProcessNodes(bvh ->GetRootNode());

  // 3. Remplissage de _TriangleIdx

  size_t nbTriangles = bvh -> GetNumIndices();
  _TriangleIdx.clear();
  _TriangleIdx.reserve(nbTriangles * 3);

  const int * TrianglesIdx = bvh -> GetIndices();
  if ( TrianglesIdx )
  {
    for ( int i = 0; i < nbTriangles; ++i )
    {
      int triangleIdx = TrianglesIdx[i];

      Vec3i indices1 = iMesh -> GetIndices()[triangleIdx * 3    ];
      Vec3i indices2 = iMesh -> GetIndices()[triangleIdx * 3 + 1];
      Vec3i indices3 = iMesh -> GetIndices()[triangleIdx * 3 + 2];
      _TriangleIdx.push_back(indices1);
      _TriangleIdx.push_back(indices2);
      _TriangleIdx.push_back(indices3);
    }
  }

  return 0;
}

int BLAS::ProcessNodes(RadeonRays::Bvh::Node * iNode)
{
  RadeonRays::bbox bbox = iNode -> bounds;

  _Nodes[_CurNode]._BBoxMin = bbox.pmin;
  _Nodes[_CurNode]._BBoxMax = bbox.pmax;
  _Nodes[_CurNode]._LcRcLeaf.z = 0;

  int index = _CurNode;

  if ( iNode -> type == RadeonRays::Bvh::NodeType::kLeaf )
  {
    _Nodes[_CurNode]._LcRcLeaf.x = iNode -> startidx;
    _Nodes[_CurNode]._LcRcLeaf.y = iNode -> numprims;
    _Nodes[_CurNode]._LcRcLeaf.z = 1;
  }
  else
  {
    _CurNode++;
    _Nodes[index]._LcRcLeaf.x = ProcessNodes(iNode -> lc);
    _CurNode++;
    _Nodes[index]._LcRcLeaf.y = ProcessNodes(iNode -> rc);
  }

  return index;
}

}
