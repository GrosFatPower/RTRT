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
static const std::filesystem::path S_DataDir   = std::filesystem::path("..") / ".." / "Resources";
static const std::filesystem::path S_ImgDir    = std::filesystem::path("..") / ".." / "Resources" / "Img";
static const std::filesystem::path S_EnvMapDir = std::filesystem::path("..") / ".." / "Assets" / "HDR";

inline std::string GetShaderPath(const std::string& iShaderName)
{
  return (S_ShaderDir / iShaderName).string();
}

inline std::string GetAssetPath(const std::string& iAssetName)
{
  return (S_AssetsDir / iAssetName).string();
}

inline std::string GetDataPath(const std::string& iDataName)
{
  return (S_DataDir / iDataName).string();
}

inline std::string GetImgPath(const std::string& iImgName)
{
  return (S_ImgDir / iImgName).string();
}

inline std::string GetEnvMapPath(const std::string& iEnvMapName)
{
  return (S_EnvMapDir / iEnvMapName).string();
}

}

}

#endif
