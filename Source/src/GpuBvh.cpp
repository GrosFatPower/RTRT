#include "GpuBvh.h"
#include "Mesh.h"
#include "Primitive.h"
#include "MathUtil.h"
#include <iostream>
#include <memory>

namespace RTRT
{

GpuBvh::~GpuBvh()
{
}

// ----------------------------------------------------------------------------
// TLAS
// ----------------------------------------------------------------------------
GpuTLAS::~GpuTLAS()
{
}

int GpuTLAS::Build( std::vector<Mesh*> & iMeshes, std::vector<MeshInstance> & iMeshInstances )
{
  // 1. Compute BVH
  const int nbInstances = iMeshInstances.size();
  if ( 0 == nbInstances )
    return 0;

  std::vector<RadeonRays::bbox> bounds(nbInstances);

//#pragma omp parallel for
  for ( int i = 0; i < nbInstances; ++i )
  {
    Mesh * curMesh = iMeshes[iMeshInstances[i]._MeshID];
    if ( !curMesh )
      return 1;

    Box boundingBox = curMesh -> GetBoundingBox();

    Vec3 right, up, forward, pos;
    MathUtil::Decompose(iMeshInstances[i]._Transform, right, up, forward, pos);

    // Transformation de la bbox
    Vec3 lowRight = right * boundingBox._Low.x;
    Vec3 highRight = right * boundingBox._High.x;

    Vec3 lowUp = up * boundingBox._Low.y;
    Vec3 highUp = up * boundingBox._High.y;

    Vec3 lowForward = forward * boundingBox._Low.z;
    Vec3 highForward = forward * boundingBox._High.z;

    bounds[i].pmin = MathUtil::Min(lowRight, highRight) + MathUtil::Min(lowUp, highUp) + MathUtil::Min(lowForward, highForward) + pos;
    bounds[i].pmax = MathUtil::Max(lowRight, highRight) + MathUtil::Max(lowUp, highUp) + MathUtil::Max(lowForward, highForward) + pos;
  }

  std::unique_ptr<RadeonRays::Bvh> bvh = std::make_unique<RadeonRays::Bvh>(10.0f, 64, false);
  bvh -> Build(&bounds[0], nbInstances);

  // 2. Remplissage du BVH

  _Nodes.clear();
  _Nodes.resize(bvh -> GetNodeCount());
  ProcessNodes(bvh ->GetRootNode());

  // 3. Remplissage de _PackedMeshInstances

  size_t nbPackedInstance = bvh -> GetNumIndices();
  _PackedMeshInstances.clear();
  _PackedMeshInstances.reserve(nbPackedInstance);

  const int * PackedInstances = bvh -> GetIndices();
  if ( PackedInstances )
  {
    for ( int i = 0; i < nbPackedInstance; ++i )
    {
      int instanceId = PackedInstances[i];
      _PackedMeshInstances.push_back(iMeshInstances[instanceId]);
    }
  }

  return 0;
}

int GpuTLAS::ProcessNodes(RadeonRays::Bvh::Node * iNode)
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
    _Nodes[_CurNode]._LcRcLeaf.z = -1;
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

// ----------------------------------------------------------------------------
// BLAS
// ----------------------------------------------------------------------------
GpuBLAS::~GpuBLAS()
{
}

int GpuBLAS::Build( Mesh & iMesh )
{
  // 1. Compute BVH
  const int nbTris = iMesh.GetIndices().size() / 3;
  if ( 0 == nbTris )
    return 0;

  std::vector<RadeonRays::bbox> bounds(nbTris);

//#pragma omp parallel for
  for ( int i = 0; i < nbTris; ++i )
  {
    for ( int j = 0; j < 3; ++j )
    {
      Vec3i indices = iMesh.GetIndices()[i*3+j];
      bounds[i].grow(iMesh.GetVertices()[indices.x]);
    }
  }

  std::unique_ptr<RadeonRays::SplitBvh> bvh = std::make_unique<RadeonRays::SplitBvh>(2.0f, 64, 0, 0.001f, 0.f);

  bvh -> Build(&bounds[0], nbTris);

  // 2. Remplissage du BVH

  _Nodes.clear();
  _Nodes.resize(bvh -> GetNodeCount());
  ProcessNodes(bvh ->GetRootNode());

  // 3. Remplissage de _PackedTriangleIdx

  size_t nbPackedTriangles = bvh -> GetNumIndices();
  _PackedTriangleIdx.clear();
  _PackedTriangleIdx.reserve(nbPackedTriangles * 3);

  const int * TrianglesIdx = bvh -> GetIndices();
  if ( TrianglesIdx )
  {
    for ( int i = 0; i < nbPackedTriangles; ++i )
    {
      int triangleIdx = TrianglesIdx[i];

      Vec3i indices1 = iMesh.GetIndices()[triangleIdx * 3    ];
      Vec3i indices2 = iMesh.GetIndices()[triangleIdx * 3 + 1];
      Vec3i indices3 = iMesh.GetIndices()[triangleIdx * 3 + 2];
      _PackedTriangleIdx.push_back(indices1);
      _PackedTriangleIdx.push_back(indices2);
      _PackedTriangleIdx.push_back(indices3);
    }
  }

  return 0;
}

int GpuBLAS::ProcessNodes(RadeonRays::Bvh::Node * iNode)
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
