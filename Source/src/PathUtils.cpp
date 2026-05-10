#include "PathUtils.h"

#include <filesystem>
#include <iostream>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace RTRT
{

namespace PathUtils
{

namespace fs = std::filesystem;

static fs::path S_RepoRoot;
static fs::path S_ExecutableDir;
static bool S_DidWarnMissingRepoRoot = false;

// ----------------------------------------------------------------------------
// TryGetExecutablePath
// ----------------------------------------------------------------------------
static fs::path TryGetExecutablePath()
{
#if defined(__APPLE__)
  uint32_t bufferSize = 0;
  _NSGetExecutablePath(nullptr, &bufferSize);
  if ( !bufferSize )
    return fs::path();

  std::vector<char> buffer(bufferSize + 1u, '\0');
  if ( 0 == _NSGetExecutablePath(buffer.data(), &bufferSize) )
    return fs::path(buffer.data());

  return fs::path();
#elif defined(__linux__)
  std::vector<char> buffer(4096, '\0');
  ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1u);
  if ( length <= 0 )
    return fs::path();

  buffer[(size_t)length] = '\0';
  return fs::path(buffer.data());
#elif defined(_WIN32)
  std::vector<char> buffer(MAX_PATH, '\0');
  DWORD length = GetModuleFileNameA(nullptr, buffer.data(), (DWORD)buffer.size());
  if ( !length || ( length >= buffer.size() ) )
    return fs::path();

  return fs::path(buffer.data());
#else
  return fs::path();
#endif
}

// ----------------------------------------------------------------------------
// IsValidRepoRoot
// ----------------------------------------------------------------------------
static bool IsValidRepoRoot( const fs::path & iPath )
{
  std::error_code ec;
  return fs::exists( iPath / "Assets", ec ) &&
         fs::exists( iPath / "Shaders", ec ) &&
         fs::exists( iPath / "Resources", ec );
}

// ----------------------------------------------------------------------------
// FindRepoRootFrom
// ----------------------------------------------------------------------------
static fs::path FindRepoRootFrom( fs::path iStart )
{
  std::error_code ec;

  if ( iStart.empty() )
    return fs::path();

  if ( fs::is_regular_file( iStart, ec ) )
    iStart = iStart.parent_path();

  iStart = fs::weakly_canonical( iStart, ec );
  if ( ec )
    return fs::path();

  fs::path curPath = iStart;
  while ( !curPath.empty() )
  {
    if ( IsValidRepoRoot(curPath) )
      return curPath;

    fs::path parent = curPath.parent_path();
    if ( parent == curPath )
      break;

    curPath = parent;
  }

  return fs::path();
}

// ----------------------------------------------------------------------------
// ResolveRepoRoot
// ----------------------------------------------------------------------------
static fs::path ResolveRepoRoot()
{
  if ( !S_RepoRoot.empty() && IsValidRepoRoot(S_RepoRoot) )
    return S_RepoRoot;

  if ( S_ExecutableDir.empty() )
  {
    std::error_code ec;
    fs::path exePath = fs::weakly_canonical(TryGetExecutablePath(), ec);
    if ( !ec && !exePath.empty() )
      S_ExecutableDir = exePath.parent_path();
  }

  std::vector<fs::path> searchRoots;
  searchRoots.emplace_back(fs::current_path());

  if ( !S_ExecutableDir.empty() )
    searchRoots.emplace_back(S_ExecutableDir);

  for ( const fs::path & root : searchRoots )
  {
    fs::path repoRoot = FindRepoRootFrom(root);
    if ( !repoRoot.empty() )
    {
      S_RepoRoot = repoRoot;
      return S_RepoRoot;
    }
  }

  if ( !S_DidWarnMissingRepoRoot )
  {
    std::cerr << "PathUtils : Failed to locate repo root containing Assets/Shaders/Resources."
              << " Current working directory is " << fs::current_path().string();
    if ( !S_ExecutableDir.empty() )
      std::cerr << " executable directory is " << S_ExecutableDir.string();
    std::cerr << std::endl;
    S_DidWarnMissingRepoRoot = true;
  }

  return fs::current_path();
}

// ----------------------------------------------------------------------------
// BuildPath
// ----------------------------------------------------------------------------
static std::string BuildPath( const fs::path & iBaseDir, const std::string & iName )
{
  return ( iBaseDir / iName ).lexically_normal().string();
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
void Initialize( const char * iArgv0 )
{
  std::error_code ec;
  fs::path exePath;

  if ( iArgv0 && iArgv0[0] )
  {
    exePath = fs::path(iArgv0);
    if ( exePath.is_relative() )
      exePath = fs::absolute(exePath, ec);

    exePath = fs::weakly_canonical(exePath, ec);
  }

  if ( ec || exePath.empty() )
  {
    ec.clear();
    exePath = fs::weakly_canonical(TryGetExecutablePath(), ec);
  }

  if ( ec || exePath.empty() )
    return;

  if ( fs::is_regular_file(exePath, ec) )
    S_ExecutableDir = exePath.parent_path();
  else
    S_ExecutableDir = exePath;

  fs::path repoRoot = FindRepoRootFrom(S_ExecutableDir);
  if ( !repoRoot.empty() )
    S_RepoRoot = repoRoot;
}

// ----------------------------------------------------------------------------
// GetShaderPath
// ----------------------------------------------------------------------------
std::string GetShaderPath( const std::string & iShaderName )
{
  return BuildPath( ResolveRepoRoot() / "Shaders", iShaderName );
}

// ----------------------------------------------------------------------------
// GetAssetPath
// ----------------------------------------------------------------------------
std::string GetAssetPath( const std::string & iAssetName )
{
  return BuildPath( ResolveRepoRoot() / "Assets", iAssetName );
}

// ----------------------------------------------------------------------------
// GetDataPath
// ----------------------------------------------------------------------------
std::string GetDataPath( const std::string & iDataName )
{
  return BuildPath( ResolveRepoRoot() / "Resources", iDataName );
}

// ----------------------------------------------------------------------------
// GetImgPath
// ----------------------------------------------------------------------------
std::string GetImgPath( const std::string & iImgName )
{
  return BuildPath( ResolveRepoRoot() / "Resources" / "Img", iImgName );
}

// ----------------------------------------------------------------------------
// GetEnvMapPath
// ----------------------------------------------------------------------------
std::string GetEnvMapPath( const std::string & iEnvMapName )
{
  return BuildPath( ResolveRepoRoot() / "Assets" / "HDR", iEnvMapName );
}

}

}
