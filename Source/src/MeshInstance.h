#ifndef _MeshInstance_
#define _MeshInstance_

#include "MathUtil.h"
#include <string>

namespace RTRT
{

struct MeshInstance
{
  std::string _Filename;
  int         _MeshID;
  int         _MaterialID;
  Mat4x4      _Transform;

  MeshInstance( const std::string & iFilename, int iMeshID, int iMaterialID, const Mat4x4 & iTransform )
  : _Filename(iFilename), _MeshID(iMeshID), _MaterialID(iMaterialID), _Transform(iTransform) {}
};

}

#endif /* _MeshInstance_ */
