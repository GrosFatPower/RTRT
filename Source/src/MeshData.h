#ifndef _MeshData_
#define _MeshData_

#include "Math.h"
#include <vector>
#include <string>

namespace RTRT
{

class MeshData
{
public:
  MeshData() {}
  virtual ~MeshData();

private:

  std::vector<Vec3>  _Vertices;
  std::vector<Vec3>  _Normals;
  std::vector<Vec2>  _UVs;

  int                _NbFaces = 0;
  std::vector<Vec3i> _Indices;

  std::string _Filename = "";

  friend class Loader;
};

}

#endif /* _MeshData_ */
