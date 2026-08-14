#include "DroppedFileUtils.h"

#include <algorithm>
#include <cctype>

namespace RTRT
{

namespace DroppedFileUtils
{

static std::string LowerExtension( const std::filesystem::path & iPath )
{
  std::string ext = iPath.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), []( unsigned char c ) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

std::filesystem::path NormalizeDroppedPath( const std::filesystem::path & iPath )
{
  std::error_code ec;
  std::filesystem::path filepath = std::filesystem::absolute(iPath, ec);
  if ( ec )
    filepath = iPath;
  return filepath.lexically_normal();
}

bool IsDroppedScenePath( const std::filesystem::path & iPath )
{
  const std::string ext = LowerExtension(iPath);
  return ( ".gltf" == ext ) || ( ".glb" == ext ) || ( ".scene" == ext );
}

bool IsDroppedPropPath( const std::filesystem::path & iPath )
{
  const std::string ext = LowerExtension(iPath);
  return ( ".gltf" == ext ) || ( ".glb" == ext );
}

std::string DisplayName( const std::filesystem::path & iPath )
{
  const std::string filename = iPath.filename().string();
  return filename.empty() ? iPath.string() : filename;
}

}

}
