#ifndef _ProceduralMesh_
#define _ProceduralMesh_

#include <string>

namespace RTRT
{

class Mesh;

namespace ProceduralMesh
{

Mesh * CreateCube( const std::string & iName );
Mesh * CreateQuad( const std::string & iName, float iHalfExtent = 0.5f );
Mesh * CreateUVSphere( const std::string & iName, int iRings = 8, int iSegments = 16 );

}

}

#endif /* _ProceduralMesh_ */
