#ifndef _Util_
#define _Util_

#include <string>
#include <vector>

namespace RTRT
{

class Util
{
public:

static std::string FileName( const std::string iFilePath );

static int RetrieveFiles( const std::string & iDir, const std::vector<std::string> & iExtensions, std::vector<std::string> & oFilePaths, std::vector<std::string> * oFileNames = nullptr );

static int RetrieveSceneFiles( const std::string & iAssetsDir, std::vector<std::string> & oSceneFiles, std::vector<std::string> * oSceneNames = nullptr );

static int RetrieveBackgroundFiles( const std::string & iAssetsDir, std::vector<std::string> & oBackgroundFiles, std::vector<std::string> * oBackgroundNames = nullptr );

};

}

#endif /* _Util_ */
