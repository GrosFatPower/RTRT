#ifndef _RenderTestSIMDUtil_
#define _RenderTestSIMDUtil_

namespace RTRT
{

namespace Tests
{

namespace SIMDTestUtil
{

bool CheckSIMDTransforms();
bool CheckSIMDInterpolation();
bool CheckSIMDBarycentrics();
bool CheckSIMDVarying();
bool CheckSIMDLoadedSceneData( bool iQuiet );

}

}

}

#endif /* _RenderTestSIMDUtil_ */
