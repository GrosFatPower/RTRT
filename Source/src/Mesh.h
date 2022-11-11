#ifndef _Mesh_
#define _Mesh_

#include "Math.h"
#include <vector>
#include <string>

namespace RTRT
{

class Mesh
{
public:
  Mesh() {}
  virtual ~Mesh();

  bool Load( const std::string & iFilename );

  const std::string & Filename() const { return _Filename; }

  int GetMeshID() const { return _MeshID; }
  void SetMeshID( int iTextID ) { _MeshID = iTextID; }

private:

  int                _MeshID = -1;
  std::vector<Vec3>  _Vertices;
  std::vector<Vec3>  _Normals;
  std::vector<Vec2>  _UVs;

  int                _NbFaces = 0;
  std::vector<Vec3i> _Indices;

  std::string _Filename = "";
};

}

#endif /* _Mesh_ */
