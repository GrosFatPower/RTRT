#ifndef _ObjectInstance_
#define _ObjectInstance_

#include "Math.h"
#include "Object.h"

namespace RTRT
{

struct ObjectInstance
{
  int         _ObjectID;
  int         _MaterialID;
  Mat4x4      _Transform;

  ObjectInstance( int iObjectID, int iMaterialID, const Mat4x4 & iTransform )
  : _ObjectID(iObjectID), _MaterialID(iMaterialID), _Transform(iTransform) {}
};

}

#endif /* _ObjectInstance_ */
