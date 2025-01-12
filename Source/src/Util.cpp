/*
*/

#include "Util.h"

#include "tinydir.h"
#include <filesystem> // C++17

namespace fs = std::filesystem;

namespace RTRT
{

// ----------------------------------------------------------------------------
// FileName
// ----------------------------------------------------------------------------
std::string Util::FileName( const std::string iFilePath )
{
  fs::path path(iFilePath);

  return path.filename().string();
}

// ----------------------------------------------------------------------------
// RetrieveFiles
// ----------------------------------------------------------------------------
int Util::RetrieveFiles( const std::string & iDir, const std::vector<std::string> & iExtensions, std::vector<std::string> & oFilePaths, std::vector<std::string> * oFileNames )
{
  tinydir_dir dir;
  if ( 0 == tinydir_open_sorted(&dir, iDir.c_str()) )
  {
    for ( int i = 0; i < dir.n_files; ++i )
    {
      tinydir_file file;
      tinydir_readfile_n(&dir, &file, i);
  
      for ( const auto & searchedExt : iExtensions )
      {
        if ( searchedExt == file.extension )
        {
          oFilePaths.push_back(iDir + std::string(file.name));
          if ( oFileNames )
            oFileNames -> push_back(file.name);
          break;
        }
      }
    }
  
    tinydir_close(&dir);
    return 0;
  }

  return 1;
}

// ----------------------------------------------------------------------------
// RetrieveSceneFiles
// ----------------------------------------------------------------------------
int Util::RetrieveSceneFiles( const std::string & iAssetsDir, std::vector<std::string> & oSceneFiles, std::vector<std::string> * oSceneNames )
{
  std::vector<std::string> extensions;
  extensions.push_back("scene");
  extensions.push_back("gltf");
  extensions.push_back("glb");

  return Util::RetrieveFiles(iAssetsDir, extensions, oSceneFiles, oSceneNames);
}

// ----------------------------------------------------------------------------
// RetrieveBackgroundFiles
// ----------------------------------------------------------------------------
int Util::RetrieveBackgroundFiles( const std::string & iAssetsDir, std::vector<std::string> & oBackgroundFiles, std::vector<std::string> * oBackgroundNames )
{
  std::vector<std::string> extensions;
  extensions.push_back("hdr");
  extensions.push_back("HDR");

  return Util::RetrieveFiles(iAssetsDir, extensions, oBackgroundFiles, oBackgroundNames);
}

}
