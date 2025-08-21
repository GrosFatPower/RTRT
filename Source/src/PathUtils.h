#ifndef _PathUtils_
#define _PathUtils_

#include <string>
#include <filesystem>

namespace RTRT
{

namespace PathUtils
{

static const std::filesystem::path S_AssetsDir = std::filesystem::path("..") / ".." / "Assets";
static const std::filesystem::path S_ShaderDir = std::filesystem::path("..") / ".." / "Shaders";
static const std::filesystem::path S_DataDir   = std::filesystem::path("..") / ".." / "Data";

inline std::string GetShaderPath(const std::string& iShaderName)
{
  return (S_ShaderDir / iShaderName).string();
}

inline std::string GetAssetPath(const std::string& iAssetName)
{
  return (S_AssetsDir / iAssetName).string();
}

//inline std::string GetDataPath(const std::string& iDataName)
//{
//  std::filesystem::path dataDir = std::filesystem::path("..") / ".." / "Data";
//  return (S_DataDir / iDataName).string();
//}

}

}

#endif
