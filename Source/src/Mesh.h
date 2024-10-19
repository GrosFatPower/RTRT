#ifndef _Mesh_
#define _Mesh_

#include "MathUtil.h"
#include "Primitive.h"
#include "GpuBvh.h"
#include <memory>
#include <vector>
#include <string>

namespace RTRT
{

class Mesh
{
public:
  Mesh();
  virtual ~Mesh();

  bool LoadOBJ( const std::string & iFilename );

  const std::string & Filename() const { return _Filename; }

  int GetMeshID() const { return _MeshID; }
  void SetMeshID( int iMeshID ) { _MeshID = iMeshID; }

  int GetNbFaces() const { return _NbFaces; }

  int BuildBvh();

  const std::vector<Vec3>  & GetVertices() const { return _Vertices; }
  const std::vector<Vec3>  & GetNormals()  const { return _Normals;  }
  const std::vector<Vec2>  & GetUVs()      const { return _UVs;      }
  const std::vector<Vec3i> & GetIndices()  const { return _Indices;  }

  std::shared_ptr<GpuBLAS> & GetBvh() { return _Bvh; }

  const Box & GetBoundingBox() const { return _BoundingBox; }

private:

  int                      _MeshID  = -1;
  int                      _NbFaces = 0;

  std::vector<Vec3>        _Vertices;
  std::vector<Vec3>        _Normals;
  std::vector<Vec2>        _UVs;
  std::vector<Vec3i>       _Indices;

  Box                      _BoundingBox;

  std::shared_ptr<GpuBLAS> _Bvh = nullptr;

  std::string              _Filename = "";
};

}

#endif /* _Mesh_ */
