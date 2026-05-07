#include "ProceduralMesh.h"

#include "Mesh.h"

namespace RTRT
{

namespace ProceduralMesh
{

// ----------------------------------------------------------------------------
// CreateCube
// ----------------------------------------------------------------------------
Mesh * CreateCube( const std::string & iName )
{
  std::vector<Vec3> vertices;
  vertices.reserve(24);

  const float h = 0.5f;

  auto addFaceVertices = [&]( const Vec3 & iA, const Vec3 & iB, const Vec3 & iC, const Vec3 & iD )
  {
    vertices.push_back(iA);
    vertices.push_back(iB);
    vertices.push_back(iC);
    vertices.push_back(iD);
  };

  addFaceVertices(Vec3( h, -h, -h), Vec3( h, -h,  h), Vec3( h,  h,  h), Vec3( h,  h, -h));
  addFaceVertices(Vec3(-h, -h,  h), Vec3(-h, -h, -h), Vec3(-h,  h, -h), Vec3(-h,  h,  h));
  addFaceVertices(Vec3(-h,  h, -h), Vec3( h,  h, -h), Vec3( h,  h,  h), Vec3(-h,  h,  h));
  addFaceVertices(Vec3(-h, -h,  h), Vec3( h, -h,  h), Vec3( h, -h, -h), Vec3(-h, -h, -h));
  addFaceVertices(Vec3( h, -h,  h), Vec3(-h, -h,  h), Vec3(-h,  h,  h), Vec3( h,  h,  h));
  addFaceVertices(Vec3(-h, -h, -h), Vec3( h, -h, -h), Vec3( h,  h, -h), Vec3(-h,  h, -h));

  std::vector<Vec3> normals;
  normals.push_back(Vec3( 1.f,  0.f,  0.f));
  normals.push_back(Vec3(-1.f,  0.f,  0.f));
  normals.push_back(Vec3( 0.f,  1.f,  0.f));
  normals.push_back(Vec3( 0.f, -1.f,  0.f));
  normals.push_back(Vec3( 0.f,  0.f,  1.f));
  normals.push_back(Vec3( 0.f,  0.f, -1.f));

  std::vector<Vec2> uvs;
  uvs.push_back(Vec2(0.f, 0.f));
  uvs.push_back(Vec2(1.f, 0.f));
  uvs.push_back(Vec2(1.f, 1.f));
  uvs.push_back(Vec2(0.f, 1.f));

  std::vector<Vec3i> indices;
  indices.reserve(36);

  for ( int face = 0; face < 6; ++face )
  {
    const int v = face * 4;
    indices.push_back(Vec3i(v + 0, face, 0));
    indices.push_back(Vec3i(v + 2, face, 2));
    indices.push_back(Vec3i(v + 1, face, 1));

    indices.push_back(Vec3i(v + 0, face, 0));
    indices.push_back(Vec3i(v + 3, face, 3));
    indices.push_back(Vec3i(v + 2, face, 2));
  }

  return new Mesh(iName, vertices, normals, uvs, indices);
}

}

}
