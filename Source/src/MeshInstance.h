#ifndef _MeshInstance_
#define _MeshInstance_

#include "Math.h"
#include <string>

namespace RTRT
{

struct MeshInstance
{
  std::string _Filename;
  int         _MeshID;
  int         _MaterialID;
  Mat4x4      _Transform;

  MeshInstance( const std::string & iFilename, int iShapeID, int iMaterialID, const Mat4x4 & iTransform )
  : _Filename(iFilename), _MeshID(iShapeID), _MaterialID(iMaterialID), _Transform(iTransform) {}
};

}

#endif /* _MeshInstance_ */
