#include "ProceduralMesh.h"

#include "Mesh.h"

#include <algorithm>
#include <cmath>

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

// ----------------------------------------------------------------------------
// CreateUVSphere
// ----------------------------------------------------------------------------
Mesh * CreateUVSphere( const std::string & iName, int iRings, int iSegments )
{
  iRings = std::max(3, iRings);
  iSegments = std::max(6, iSegments);

  std::vector<Vec3> vertices;
  std::vector<Vec3> normals;
  std::vector<Vec2> uvs;
  std::vector<Vec3i> indices;

  vertices.reserve((iRings + 1) * (iSegments + 1));
  normals.reserve((iRings + 1) * (iSegments + 1));
  uvs.reserve((iRings + 1) * (iSegments + 1));
  indices.reserve(iRings * iSegments * 6);

  const float pi = static_cast<float>(M_PI);
  for ( int ring = 0; ring <= iRings; ++ring )
  {
    const float v = static_cast<float>(ring) / static_cast<float>(iRings);
    const float theta = v * pi;
    const float y = std::cos(theta);
    const float r = std::sin(theta);

    for ( int segment = 0; segment <= iSegments; ++segment )
    {
      const float u = static_cast<float>(segment) / static_cast<float>(iSegments);
      const float phi = u * 2.f * pi;
      const Vec3 normal(r * std::cos(phi), y, r * std::sin(phi));

      vertices.push_back(normal * 0.5f);
      normals.push_back(normal);
      uvs.push_back(Vec2(u, 1.f - v));
    }
  }

  const int stride = iSegments + 1;
  for ( int ring = 0; ring < iRings; ++ring )
  {
    for ( int segment = 0; segment < iSegments; ++segment )
    {
      const int a = ring * stride + segment;
      const int b = (ring + 1) * stride + segment;
      const int c = (ring + 1) * stride + segment + 1;
      const int d = ring * stride + segment + 1;

      indices.push_back(Vec3i(a, a, a));
      indices.push_back(Vec3i(c, c, c));
      indices.push_back(Vec3i(b, b, b));

      indices.push_back(Vec3i(a, a, a));
      indices.push_back(Vec3i(d, d, d));
      indices.push_back(Vec3i(c, c, c));
    }
  }

  return new Mesh(iName, vertices, normals, uvs, indices);
}

}

}
