/*
  This is a modified version of the original code : https://github.com/knightcrawler25/GLSL-PathTracer
  already derived from: https://github.com/mmacklin/tinsel
*/

#include "Loader.h"
#include "Scene.h"
#include "MeshData.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include <map>
#include <vector>
#include <iostream>
#include <filesystem> // C++17

namespace fs = std::filesystem;

namespace RTRT
{

bool Loader::LoadScene(const std::string & iFilename, Scene * oScene)
{
  oScene = nullptr;

  fs::path filepath = iFilename;

  if ( ".scene" == filepath.extension() )
    return Loader::LoadFromSceneFile(iFilename, oScene);

  return false;
}

bool Loader::LoadFromSceneFile(const std::string & iFilename, Scene * oScene)
{
  oScene = nullptr;

  FILE* file;
  file = fopen(iFilename.c_str(), "r");

  if (!file)
  {
    printf("Loader : Couldn't open %s for reading\n", iFilename.c_str());
    return false;
  }

  printf("Loading Scene...");

  fs::path filepath = iFilename;
  filepath.remove_filename();

  oScene = new Scene;
  
  fclose(file);

  printf("DONE\n");

  if ( oScene )
    return true;
  return false;
}

bool Loader::LoadMesh( const std::string & iFilename, MeshData * oMeshData )
{
  oMeshData = nullptr;

  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string err, warn;
  bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, &warn, iFilename.c_str(), 0, true);
  if (!ret)
  {
    std::cout << "Loader : Unable to load object" << iFilename << std::endl;
    if ( !err.empty() )
      std::cout << err;
    if ( !warn.empty() )
      std::cout << warn;
    return false;
  }
  else if ( !warn.empty() )
  {
    std::cout << "Loader : loading object" << iFilename << " ..."<< std::endl;
    std::cout << warn;
  }

  oMeshData = new MeshData;

  oMeshData -> _Filename = iFilename;

  size_t nbVertices = attrib.vertices.size() / 3;
  for ( unsigned int i = 0; i < nbVertices; ++i )
  {
    Vec3 coords;

    coords.x = attrib.vertices[3 * i + 0];
    coords.y = attrib.vertices[3 * i + 1];
    coords.z = attrib.vertices[3 * i + 2];

    oMeshData -> _Vertices.push_back(coords);
  }

  size_t nbNormals = attrib.normals.size() / 3;
  for ( unsigned int i = 0; i < nbNormals; ++i )
  {
    Vec3 normal;
    normal.x = attrib.normals[3 * i + 0];
    normal.y = attrib.normals[3 * i + 1];
    normal.z = attrib.normals[3 * i + 2];

    oMeshData -> _Normals.push_back(normal);
  }

  size_t nbUVs = attrib.texcoords.size() / 3;
  for ( unsigned int i = 0; i < nbUVs; ++i )
  {
    Vec2 uv;
    uv.x = attrib.texcoords[2 * i + 0];
    uv.y = attrib.texcoords[2 * i + 1];

    oMeshData -> _UVs.push_back(uv);
  }

  for ( size_t s = 0; s < shapes.size(); ++s )
  {
    tinyobj::shape_t & shape = shapes[s];

    size_t index_offset = 0;
    for ( size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f )
    {
      for ( size_t v = 0; v < 3; ++v )
      {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        Vec3i indices;
        indices.x = idx.vertex_index;
        indices.y = idx.normal_index;
        indices.z = idx.texcoord_index;

        oMeshData -> _Indices.push_back(indices);
      }
      index_offset += 3;
    }
  }
  oMeshData -> _NbFaces = (int) oMeshData -> _Indices.size();

  return true;
}

}
