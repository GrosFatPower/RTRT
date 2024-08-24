#include "Mesh.h"
#include "GpuBvh.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <iostream>
#include <limits>

namespace RTRT
{

Mesh::Mesh()
{
  _Bvh = std::make_shared<GpuBLAS>();

  _BoundingBox._Low  = Vec3( std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max());
  _BoundingBox._High = Vec3(-std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max(),
                            -std::numeric_limits<float>::max());
}

Mesh::~Mesh()
{

}

bool Mesh::Load( const std::string & iFilename )
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
          indices.z = idx.texcoord_index;

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
  return _Bvh -> Build(*this);
}

}
