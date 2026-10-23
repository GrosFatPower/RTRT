#ifndef _DroppedFileUtils_
#define _DroppedFileUtils_

#include <filesystem>
#include <string>

namespace RTRT
{

namespace DroppedFileUtils
{

std::filesystem::path NormalizeDroppedPath( const std::filesystem::path & iPath );
bool IsDroppedScenePath( const std::filesystem::path & iPath );
bool IsDroppedPropPath( const std::filesystem::path & iPath );
std::string DisplayName( const std::filesystem::path & iPath );

}

}

#endif /* _DroppedFileUtils_ */
