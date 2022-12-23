#ifndef _ObjectInstance_
#define _ObjectInstance_

#include "Math.h"
#include "Primitive.h"

namespace RTRT
{

struct PrimitiveInstance
{
  int         _PrimID;
  int         _MaterialID;
  Mat4x4      _Transform;

  PrimitiveInstance( int iPrimID, int iMaterialID, const Mat4x4 & iTransform )
  : _PrimID(iPrimID), _MaterialID(iMaterialID), _Transform(iTransform) {}
};

}

#endif /* _ObjectInstance_ */
