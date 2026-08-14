#ifndef _RenderTestImageUtil_
#define _RenderTestImageUtil_

#include "RenderTestFramework.h"

namespace RTRT
{

namespace Tests
{

bool WritePFM( const std::filesystem::path & iPath, const RenderImage & iImage );
bool ReadPFM( const std::filesystem::path & iPath, RenderImage & oImage );
bool WriteDiagnosticPNG( const std::filesystem::path & iPath, const RenderImage & iImage, float iScale );
bool WriteDiffPNG( const std::filesystem::path & iPath, const RenderImage & iActual, const RenderImage & iExpected );
bool CompareImages( const RenderImage & iActual, const RenderImage & iExpected, float iPixelErrorThreshold, ImageMetrics & oMetrics );
bool MatchesThresholds( const ImageMetrics & iMetrics, const RenderTestCase & iTestCase );

}

}

#endif /* _RenderTestImageUtil_ */
