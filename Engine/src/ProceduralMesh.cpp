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
// CreateBeveledCube
// ----------------------------------------------------------------------------
Mesh * CreateBeveledCube( const std::string & iName, float iBevelRadius, int iBevelSegments )
{
  const float bevelRadius = std::clamp(iBevelRadius, 0.0f, 0.49f);
  if ( bevelRadius <= 1e-5f )
    return CreateCube(iName);

  const int segments = std::max(1, iBevelSegments);
  const float halfExtent = 0.5f;
  const float core = halfExtent - bevelRadius;
  const float halfPi = 0.5f * static_cast<float>(M_PI);

  std::vector<Vec3>  vertices;
  std::vector<Vec3>  normals;
  std::vector<Vec2>  uvs;
  std::vector<Vec3i> indices;

  auto computeBoxUV = []( const Vec3 & iPos, const Vec3 & iNorm ) -> Vec2
  {
    const float ax = std::abs(iNorm.x);
    const float ay = std::abs(iNorm.y);
    const float az = std::abs(iNorm.z);
    if ( ax >= ay && ax >= az )
      return Vec2(0.5f + ( ( iNorm.x >= 0.f ) ? -iPos.z : iPos.z ), 0.5f + iPos.y);
    if ( ay >= ax && ay >= az )
      return Vec2(0.5f + iPos.x, 0.5f + ( ( iNorm.y >= 0.f ) ? -iPos.z : iPos.z ));
    return Vec2(0.5f + ( ( iNorm.z >= 0.f ) ? iPos.x : -iPos.x ), 0.5f + iPos.y);
  };

  auto addVertex = [&]( const Vec3 & iPos, const Vec3 & iNorm, const Vec2 & iUV ) -> int
  {
    const int idx = static_cast<int>(vertices.size());
    vertices.push_back(iPos);
    normals.push_back(glm::normalize(iNorm));
    uvs.push_back(iUV);
    return idx;
  };

  auto emitTri = [&]( int iA, int iB, int iC )
  {
    indices.push_back(Vec3i(iA, iA, iA));
    indices.push_back(Vec3i(iB, iB, iB));
    indices.push_back(Vec3i(iC, iC, iC));
  };

  auto emitQuad = [&]( int iA, int iB, int iC, int iD )
  {
    emitTri(iA, iB, iC);
    emitTri(iA, iC, iD);
  };

  auto exactTrig = [&]( int iStep, float & oCos, float & oSin )
  {
    if ( 0 == iStep )
    {
      oCos = 1.0f;
      oSin = 0.0f;
    }
    else if ( segments == iStep )
    {
      oCos = 0.0f;
      oSin = 1.0f;
    }
    else
    {
      const float angle = halfPi * static_cast<float>(iStep) / static_cast<float>(segments);
      oCos = std::cos(angle);
      oSin = std::sin(angle);
    }
  };

  // 1. Six flat inner face quads (exact flat normals prevent reflection/refraction distortion)
  struct FaceDef { Vec3 N, U, V; };
  const FaceDef faces[6] = {
    { Vec3( 1.f,  0.f,  0.f), Vec3( 0.f,  0.f, -1.f), Vec3(0.f, 1.f,  0.f) },
    { Vec3(-1.f,  0.f,  0.f), Vec3( 0.f,  0.f,  1.f), Vec3(0.f, 1.f,  0.f) },
    { Vec3( 0.f,  1.f,  0.f), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, 0.f, -1.f) },
    { Vec3( 0.f, -1.f,  0.f), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, 0.f,  1.f) },
    { Vec3( 0.f,  0.f,  1.f), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, 1.f,  0.f) },
    { Vec3( 0.f,  0.f, -1.f), Vec3(-1.f,  0.f,  0.f), Vec3(0.f, 1.f,  0.f) }
  };

  for ( const FaceDef & f : faces )
  {
    const Vec3 p0 = f.N * ( core + bevelRadius ) - f.U * core - f.V * core;
    const Vec3 p1 = f.N * ( core + bevelRadius ) + f.U * core - f.V * core;
    const Vec3 p2 = f.N * ( core + bevelRadius ) + f.U * core + f.V * core;
    const Vec3 p3 = f.N * ( core + bevelRadius ) - f.U * core + f.V * core;

    const int v0 = addVertex(p0, f.N, computeBoxUV(p0, f.N));
    const int v1 = addVertex(p1, f.N, computeBoxUV(p1, f.N));
    const int v2 = addVertex(p2, f.N, computeBoxUV(p2, f.N));
    const int v3 = addVertex(p3, f.N, computeBoxUV(p3, f.N));
    emitQuad(v0, v1, v2, v3);
  }

  // 2. Twelve cylindrical quarter-round edge strips (A x B = T)
  struct EdgeDef { Vec3 A, B; };
  const EdgeDef edges[12] = {
    // Edges parallel to +Z
    { Vec3( 1.f,  0.f,  0.f), Vec3( 0.f,  1.f,  0.f) },
    { Vec3( 0.f,  1.f,  0.f), Vec3(-1.f,  0.f,  0.f) },
    { Vec3(-1.f,  0.f,  0.f), Vec3( 0.f, -1.f,  0.f) },
    { Vec3( 0.f, -1.f,  0.f), Vec3( 1.f,  0.f,  0.f) },
    // Edges parallel to +X
    { Vec3( 0.f,  1.f,  0.f), Vec3( 0.f,  0.f,  1.f) },
    { Vec3( 0.f,  0.f,  1.f), Vec3( 0.f, -1.f,  0.f) },
    { Vec3( 0.f, -1.f,  0.f), Vec3( 0.f,  0.f, -1.f) },
    { Vec3( 0.f,  0.f, -1.f), Vec3( 0.f,  1.f,  0.f) },
    // Edges parallel to +Y
    { Vec3( 0.f,  0.f,  1.f), Vec3( 1.f,  0.f,  0.f) },
    { Vec3( 1.f,  0.f,  0.f), Vec3( 0.f,  0.f, -1.f) },
    { Vec3( 0.f,  0.f, -1.f), Vec3(-1.f,  0.f,  0.f) },
    { Vec3(-1.f,  0.f,  0.f), Vec3( 0.f,  0.f,  1.f) }
  };

  for ( const EdgeDef & e : edges )
  {
    const Vec3 T = glm::cross(e.A, e.B);
    const Vec3 baseCenter = ( e.A + e.B ) * core;
    std::vector<int> negVerts;
    std::vector<int> posVerts;
    negVerts.reserve(segments + 1);
    posVerts.reserve(segments + 1);

    for ( int i = 0; i <= segments; ++i )
    {
      float cosA = 0.f, sinA = 0.f;
      exactTrig(i, cosA, sinA);
      const Vec3 n = e.A * cosA + e.B * sinA;
      const Vec3 pNeg = baseCenter - T * core + n * bevelRadius;
      const Vec3 pPos = baseCenter + T * core + n * bevelRadius;

      negVerts.push_back(addVertex(pNeg, n, computeBoxUV(pNeg, n)));
      posVerts.push_back(addVertex(pPos, n, computeBoxUV(pPos, n)));
    }

    for ( int i = 0; i < segments; ++i )
      emitQuad(negVerts[i], negVerts[i + 1], posVerts[i + 1], posVerts[i]);
  }

  // 3. Eight spherical octant corners (E1 x E2 = E3)
  for ( int sx : { -1, 1 } )
  {
    for ( int sy : { -1, 1 } )
    {
      for ( int sz : { -1, 1 } )
      {
        const Vec3 cornerCenter(static_cast<float>(sx) * core,
                                static_cast<float>(sy) * core,
                                static_cast<float>(sz) * core);
        const bool positiveParity = ( ( sx * sy * sz ) > 0 );
        const Vec3 E1 = positiveParity ? Vec3(static_cast<float>(sx), 0.f, 0.f) : Vec3(0.f, 0.f, static_cast<float>(sz));
        const Vec3 E2 = Vec3(0.f, static_cast<float>(sy), 0.f);
        const Vec3 E3 = positiveParity ? Vec3(0.f, 0.f, static_cast<float>(sz)) : Vec3(static_cast<float>(sx), 0.f, 0.f);

        const int stride = segments + 1;
        std::vector<int> gridVerts(stride * stride, -1);

        for ( int i = 0; i <= segments; ++i )
        {
          float cosTheta = 0.f, sinTheta = 0.f;
          exactTrig(i, cosTheta, sinTheta);

          for ( int j = 0; j <= segments; ++j )
          {
            float cosPhi = 0.f, sinPhi = 0.f;
            exactTrig(j, cosPhi, sinPhi);

            const Vec3 horiz = E1 * cosPhi + E3 * sinPhi;
            const Vec3 n = E2 * cosTheta + horiz * sinTheta;
            const Vec3 pos = cornerCenter + n * bevelRadius;
            gridVerts[i * stride + j] = addVertex(pos, n, computeBoxUV(pos, n));
          }
        }

        for ( int i = 0; i < segments; ++i )
        {
          for ( int j = 0; j < segments; ++j )
          {
            const int a = gridVerts[i * stride + j];
            const int b = gridVerts[i * stride + ( j + 1 )];
            const int c = gridVerts[( i + 1 ) * stride + ( j + 1 )];
            const int d = gridVerts[( i + 1 ) * stride + j];

            if ( 0 == i )
            {
              emitTri(a, c, d);
            }
            else
            {
              emitTri(a, b, d);
              emitTri(b, c, d);
            }
          }
        }
      }
    }
  }

  return new Mesh(iName, vertices, normals, uvs, indices);
}

// ----------------------------------------------------------------------------
// CreateQuad
// ----------------------------------------------------------------------------
Mesh * CreateQuad( const std::string & iName, float iHalfExtent )
{
  const float h = std::max(0.0001f, iHalfExtent);

  std::vector<Vec3> vertices;
  vertices.reserve(4);
  vertices.push_back(Vec3(-h, -h, 0.f));
  vertices.push_back(Vec3( h, -h, 0.f));
  vertices.push_back(Vec3( h,  h, 0.f));
  vertices.push_back(Vec3(-h,  h, 0.f));

  std::vector<Vec3> normals;
  normals.push_back(Vec3(0.f, 0.f, 1.f));

  std::vector<Vec2> uvs;
  uvs.reserve(4);
  uvs.push_back(Vec2(0.f, 0.f));
  uvs.push_back(Vec2(1.f, 0.f));
  uvs.push_back(Vec2(1.f, 1.f));
  uvs.push_back(Vec2(0.f, 1.f));

  std::vector<Vec3i> indices;
  indices.reserve(6);
  indices.push_back(Vec3i(0, 0, 0));
  indices.push_back(Vec3i(1, 0, 1));
  indices.push_back(Vec3i(2, 0, 2));

  indices.push_back(Vec3i(0, 0, 0));
  indices.push_back(Vec3i(2, 0, 2));
  indices.push_back(Vec3i(3, 0, 3));

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
