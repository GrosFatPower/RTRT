#include "Mesh.h"
#include "GpuBvh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace RTRT
{

Mesh::Mesh()
{
  _Bvh = std::make_shared<GpuBLAS>();

  _BoundingBox._Low  = Vec3(MAX_FLOAT);
  _BoundingBox._High = Vec3(-MAX_FLOAT);
}

Mesh::Mesh(const std::string        & iName,
           const std::vector<Vec3>  & iVertices,
           const std::vector<Vec3>  & iNormals,
           const std::vector<Vec2>  & iUVs,
           const std::vector<Vec3i> & iIndices)
: _Filename( iName )
, _Vertices( iVertices )
, _Normals( iNormals )
, _UVs( iUVs )
, _Indices( iIndices )
{
  _NbFaces = static_cast<int>(_Indices.size()) / 3;

  _Bvh = std::make_shared<GpuBLAS>();

  _BoundingBox._Low = Vec3(MAX_FLOAT);
  _BoundingBox._High = Vec3(-MAX_FLOAT);

  for ( Vec3 vert : _Vertices )
  {
    if ( vert.x < _BoundingBox._Low.x )
      _BoundingBox._Low.x = vert.x;
    if ( vert.y < _BoundingBox._Low.y )
      _BoundingBox._Low.y = vert.y;
    if ( vert.z < _BoundingBox._Low.z )
      _BoundingBox._Low.z = vert.z;
    if ( vert.x > _BoundingBox._High.x )
      _BoundingBox._High.x = vert.x;
    if ( vert.y > _BoundingBox._High.y )
      _BoundingBox._High.y = vert.y;
    if ( vert.z > _BoundingBox._High.z )
      _BoundingBox._High.z = vert.z;
  }
}


Mesh::~Mesh()
{

}

bool Mesh::LoadOBJ( const std::string & iFilename )
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err, warn;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, &warn, iFilename.c_str(), 0, true);
  if (!ret)
  {
    std::cout << "Mesh : Unable to load object" << iFilename << std::endl;
    if ( !err.empty() )
      std::cout << err;
    if ( !warn.empty() )
      std::cout << warn;
    return false;
  }
  else if ( !warn.empty() )
  {
    std::cout << "Mesh : loading object" << iFilename << " ..."<< std::endl;
    std::cout << warn;
  }

  _Filename = iFilename;

  size_t nbVertices = attrib.vertices.size() / 3;
  for ( unsigned int i = 0; i < nbVertices; ++i )
  {
    Vec3 coords;

    coords.x = attrib.vertices[3 * i + 0];
    coords.y = attrib.vertices[3 * i + 1];
    coords.z = attrib.vertices[3 * i + 2];

    _Vertices.push_back(coords);

    if ( coords.x < _BoundingBox._Low.x )
      _BoundingBox._Low.x = coords.x;
    if ( coords.y < _BoundingBox._Low.y )
      _BoundingBox._Low.y = coords.y;
    if ( coords.z < _BoundingBox._Low.z )
      _BoundingBox._Low.z = coords.z;
    if ( coords.x > _BoundingBox._High.x )
      _BoundingBox._High.x = coords.x;
    if ( coords.y > _BoundingBox._High.y )
      _BoundingBox._High.y = coords.y;
    if ( coords.z > _BoundingBox._High.z )
      _BoundingBox._High.z = coords.z;
  }

  size_t nbNormals = attrib.normals.size() / 3;
  for ( unsigned int i = 0; i < nbNormals; ++i )
  {
    Vec3 normal;
    normal.x = attrib.normals[3 * i + 0];
    normal.y = attrib.normals[3 * i + 1];
    normal.z = attrib.normals[3 * i + 2];

    _Normals.push_back(normal);
  }

  size_t nbUVs = attrib.texcoords.size() / 2;
  for ( unsigned int i = 0; i < nbUVs; ++i )
  {
    Vec2 uv;
    uv.x = attrib.texcoords[2 * i + 0];
    uv.y = 1.f - attrib.texcoords[2 * i + 1];

    _UVs.push_back(uv);
  }
  if ( 0 == nbUVs )
    _UVs.push_back(Vec2(0.f));

  for ( size_t s = 0; s < shapes.size(); ++s )
  {
    tinyobj::shape_t & shape = shapes[s];

    size_t index_offset = 0;
    for ( size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f )
    {
      if ( shape.mesh.num_face_vertices[f] == 3 ) // triangles only
      {
        for ( size_t v = 0; v < 3; ++v )
        {
          tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

          Vec3i indices;
          indices.x = idx.vertex_index;
          indices.y = idx.normal_index;
          indices.z = ( idx.texcoord_index >= 0 ) ? ( idx.texcoord_index ) : ( 0 );

          _Indices.push_back(indices);
        }
      }
      index_offset += shape.mesh.num_face_vertices[f];
      _NbFaces++;
    }
  }

  return true;
}

int Mesh::BuildBvh()
{
  _Bvh -> Clear();
  return _Bvh -> Build(*this);
}

// ----------------------------------------------------------------------------
int Mesh::RefitBvh()
{
  if ( !_Bvh )
    return 1;
  return _Bvh -> Refit(*this);
}

// ----------------------------------------------------------------------------
void Mesh::InitializeSkinningBindPose()
{
  _BindVertices = _Vertices;
  _BindNormals  = _Normals;
}

// ----------------------------------------------------------------------------
void Mesh::ResetToBindPose()
{
  if ( _BindVertices.empty() )
    return;

  _Vertices = _BindVertices;
  if ( !_BindNormals.empty() )
    _Normals = _BindNormals;

  _BoundingBox._Low  = Vec3(MAX_FLOAT);
  _BoundingBox._High = Vec3(-MAX_FLOAT);
  for ( const Vec3 & v : _Vertices )
  {
    _BoundingBox._Low  = glm::min(_BoundingBox._Low, v);
    _BoundingBox._High = glm::max(_BoundingBox._High, v);
  }
}

// ----------------------------------------------------------------------------
void Mesh::ApplySkinning( const std::vector<Mat4x4> & iJointMatrices )
{
  if ( !IsSkinned() || iJointMatrices.empty() )
    return;

  const size_t nbVerts   = _BindVertices.size();
  const size_t nbNorms   = _BindNormals.size();
  const int    nbJoints  = static_cast<int>(iJointMatrices.size());

  _Vertices.resize(nbVerts);
  if ( nbNorms == nbVerts )
    _Normals.resize(nbNorms);

  Vec3 bMin( MAX_FLOAT);
  Vec3 bMax(-MAX_FLOAT);

  for ( size_t i = 0; i < nbVerts; ++i )
  {
    const Vec4i & j = _Joints[i];
    const Vec4  & w = _Weights[i];

    Mat4x4 skinMat(0.0f);
    float totalWeight = 0.0f;

    const int   jArr[4] = { j.x, j.y, j.z, j.w };
    const float wArr[4] = { w.x, w.y, w.z, w.w };

    for ( int k = 0; k < 4; ++k )
    {
      if ( wArr[k] > 0.0f && jArr[k] >= 0 && jArr[k] < nbJoints )
      {
        skinMat     += wArr[k] * iJointMatrices[jArr[k]];
        totalWeight += wArr[k];
      }
    }

    if ( totalWeight <= 1e-6f )
      skinMat = Mat4x4(1.0f);
    else if ( std::abs(totalWeight - 1.0f) > 1e-4f )
      skinMat /= totalWeight;

    const Vec3 & bindPos = _BindVertices[i];
    const Vec3 deformedPos = Vec3(skinMat * Vec4(bindPos, 1.0f));
    _Vertices[i] = deformedPos;

    bMin = glm::min(bMin, deformedPos);
    bMax = glm::max(bMax, deformedPos);

    if ( nbNorms == nbVerts )
    {
      glm::mat3 m3(skinMat);
      const float det = glm::determinant(m3);
      glm::mat3 normalMat = ( std::abs(det) > 1e-8f ) ? glm::transpose(glm::inverse(m3)) : m3;
      const Vec3 n = normalMat * _BindNormals[i];
      const float len = glm::length(n);
      _Normals[i] = ( len > 1e-8f ) ? ( n / len ) : _BindNormals[i];
    }
  }

  _BoundingBox._Low  = bMin;
  _BoundingBox._High = bMax;
}

}
