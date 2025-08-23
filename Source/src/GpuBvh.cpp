#include "GpuBvh.h"
#include "Mesh.h"
#include "Primitive.h"
#include "MathUtil.h"
#include <iostream>
#include <memory>
#include <chrono>

#include "split_bvh.h"
//#define USE_TINYBVH
#ifdef USE_TINYBVH
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#endif

namespace RTRT
{

using TLASNode = RadeonRays::Bvh::Node;
#ifdef USE_TINYBVH
using BLASNode = tinybvh::BVH::BVHNode;
#else
using BLASNode = RadeonRays::Bvh::Node;
#endif

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
GpuBvh::~GpuBvh()
{
  Clear();
}

void GpuBvh::Clear()
{
  _CurNode = 0;
  _Nodes.clear();
}

// ----------------------------------------------------------------------------
// TLAS
// ----------------------------------------------------------------------------
GpuTLAS::~GpuTLAS()
{
  Clear();
}

void GpuTLAS::Clear()
{
  GpuBvh::Clear();

  _PackedMeshInstances.clear();
}

int ProcessNodes( TLASNode * iNode, GpuTLAS * ioGpuTLAS )
{
  const RadeonRays::bbox & bbox = iNode -> bounds;

  ioGpuTLAS -> _Nodes[ioGpuTLAS -> _CurNode]._BBoxMin = bbox.pmin;
  ioGpuTLAS -> _Nodes[ioGpuTLAS -> _CurNode]._BBoxMax = bbox.pmax;
  ioGpuTLAS -> _Nodes[ioGpuTLAS -> _CurNode]._LcRcLeaf.z = 0;

  int index = ioGpuTLAS -> _CurNode;

  if ( iNode -> type == RadeonRays::Bvh::NodeType::kLeaf )
  {
    ioGpuTLAS -> _Nodes[ioGpuTLAS -> _CurNode]._LcRcLeaf.x = static_cast<float>(iNode -> startidx);
    ioGpuTLAS -> _Nodes[ioGpuTLAS -> _CurNode]._LcRcLeaf.y = static_cast<float>(iNode -> numprims);
    ioGpuTLAS -> _Nodes[ioGpuTLAS -> _CurNode]._LcRcLeaf.z = static_cast<float>(-1);
  }
  else
  {
    ioGpuTLAS -> _CurNode++;
    ioGpuTLAS -> _Nodes[index]._LcRcLeaf.x = static_cast<float>(ProcessNodes( iNode -> lc, ioGpuTLAS ));
    ioGpuTLAS -> _CurNode++;
    ioGpuTLAS -> _Nodes[index]._LcRcLeaf.y = static_cast<float>(ProcessNodes( iNode -> rc, ioGpuTLAS ));
  }

  return index;
}

int GpuTLAS::Build( std::vector<Mesh*> & iMeshes, std::vector<MeshInstance> & iMeshInstances )
{
  auto startTime = std::chrono::system_clock::now();

  // 1. Compute BVH
  const int nbInstances = static_cast<int>(iMeshInstances.size());
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
  ProcessNodes(bvh ->GetRootNode(), this);

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

  auto endTime = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
  std::cout << "GpuTLAS built in " << elapsed << "ms\n";

  return 0;
}

// ----------------------------------------------------------------------------
// BLAS
// ----------------------------------------------------------------------------
GpuBLAS::~GpuBLAS()
{
  Clear();
}

void GpuBLAS::Clear()
{
  GpuBvh::Clear();

  _PackedTriangleIdx.clear();
}

#ifdef USE_TINYBVH
int ProcessNodes( tinybvh::BVH::BVHNode * iBVHNodes, unsigned int iCurIndex, GpuBLAS * ioGpuBLAS )
{
  const BLASNode & curBVHNode = iBVHNodes[iCurIndex];
  ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._BBoxMin = Vec3(curBVHNode.aabbMin.x, curBVHNode.aabbMin.y, curBVHNode.aabbMin.z);
  ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._BBoxMax = Vec3(curBVHNode.aabbMax.x, curBVHNode.aabbMax.y, curBVHNode.aabbMax.z);
  ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.z = 0;

  int index = ioGpuBLAS -> _CurNode;

  if ( curBVHNode.triCount )
  {
    ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.x = curBVHNode.leftFirst;
    ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.y = curBVHNode.triCount;
    ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.z = 1;
  }
  else
  {
    ioGpuBLAS -> _CurNode++;
    ioGpuBLAS -> _Nodes[index]._LcRcLeaf.x = ProcessNodes( iBVHNodes, curBVHNode.leftFirst, ioGpuBLAS );
    ioGpuBLAS -> _CurNode++;
    ioGpuBLAS -> _Nodes[index]._LcRcLeaf.y = ProcessNodes( iBVHNodes, curBVHNode.leftFirst+1, ioGpuBLAS );
  }

  return index;
}
#else
int ProcessNodes( BLASNode * iNode, GpuBLAS * ioGpuBLAS )
{
  const RadeonRays::bbox & bbox = iNode -> bounds;

  ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._BBoxMin = bbox.pmin;
  ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._BBoxMax = bbox.pmax;
  ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.z = 0;

  int index = ioGpuBLAS -> _CurNode;

  if ( iNode -> type == RadeonRays::Bvh::NodeType::kLeaf )
  {
    ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.x = static_cast<float>(iNode -> startidx);
    ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.y = static_cast<float>(iNode -> numprims);
    ioGpuBLAS -> _Nodes[ioGpuBLAS -> _CurNode]._LcRcLeaf.z = static_cast < float>(1);
  }
  else
  {
    ioGpuBLAS -> _CurNode++;
    ioGpuBLAS -> _Nodes[index]._LcRcLeaf.x = static_cast<float>(ProcessNodes( iNode -> lc, ioGpuBLAS ));
    ioGpuBLAS -> _CurNode++;
    ioGpuBLAS -> _Nodes[index]._LcRcLeaf.y = static_cast<float>(ProcessNodes( iNode -> rc, ioGpuBLAS ));
  }

  return index;
}
#endif

int GpuBLAS::Build( Mesh & iMesh )
{
  auto startTime = std::chrono::system_clock::now();

  // 1. Compute BVH
  const int nbTris = static_cast<int>(iMesh.GetIndices().size()) / 3;
  if ( 0 == nbTris )
    return 1;

  #ifdef USE_TINYBVH
  std::vector<tinybvh::bvhvec4> triangles(nbTris*3);

  for ( int i = 0; i < nbTris; ++i )
  {
    for ( int j = 0; j < 3; ++j )
    {
      Vec3i indices = iMesh.GetIndices()[i * 3 + j];
      triangles[i * 3 + j] = tinybvh::bvhvec4(iMesh.GetVertices()[indices.x].x, iMesh.GetVertices()[indices.x].y, iMesh.GetVertices()[indices.x].z, 0.f);
    }
  }

  tinybvh::BVH bvh;
  //bvh.Build( &(triangles[0]) , nbTris );
  bvh.BuildHQ( &( triangles[0] ), nbTris );

  // 2. Remplissage du BVH

  int nodeCount = bvh.NodeCount();
  if ( !nodeCount || !bvh.bvhNode )
    return 1;

  _Nodes.clear();
  _Nodes.resize(nodeCount);
  ProcessNodes( bvh.bvhNode, 0, this );

  // 3. Remplissage de _PackedTriangleIdx

  size_t nbPackedTriangles = bvh.idxCount;
  _PackedTriangleIdx.clear();
  _PackedTriangleIdx.reserve( nbPackedTriangles * 3 );

  if ( bvh.triIdx )
  {
    for ( int i = 0; i < nbPackedTriangles; ++i )
    {
      unsigned int triangleIdx = bvh.triIdx[i];

      Vec3i indices1 = iMesh.GetIndices()[triangleIdx * 3];
      Vec3i indices2 = iMesh.GetIndices()[triangleIdx * 3 + 1];
      Vec3i indices3 = iMesh.GetIndices()[triangleIdx * 3 + 2];
      _PackedTriangleIdx.push_back( indices1 );
      _PackedTriangleIdx.push_back( indices2 );
      _PackedTriangleIdx.push_back( indices3 );
    }
  }
  else
    return 1;

  #else
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
  ProcessNodes(bvh ->GetRootNode(), this);

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
  #endif

  auto endTime = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( endTime - startTime ).count();
  std::cout << "GpuBLAS built in " << elapsed << "ms\n";

  return 0;
}

}
