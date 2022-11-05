#ifndef _Shape_
#define _Shape_

#include "Math.h"
#include <vector>
#include <string>

namespace RTRT
{

class Shape
{
public:
  Shape() {}
  virtual ~Shape();

  bool LoadFromFile( const std::string & iFilename );

private:

  int                _NbVertices = 0;
  std::vector<Vec3>  _Vertices;
  std::vector<Vec3>  _Normals;
  std::vector<Vec2>  _UVs;

  int                _NbFaces = 0;
  std::vector<Vec3i> _Indices;

  std::string _Filename = "";

};

}

#endif /* _Shape_ */
