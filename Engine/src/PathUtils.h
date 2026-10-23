#ifndef _PathUtils_
#define _PathUtils_

#include <string>

namespace RTRT
{

namespace PathUtils
{

void Initialize( const char * iArgv0 = nullptr );

std::string GetShaderPath( const std::string & iShaderName );
std::string GetAssetPath( const std::string & iAssetName );
std::string GetBenchmarkPath( const std::string & iBenchmarkName );
std::string GetDataPath( const std::string & iDataName );
std::string GetImgPath( const std::string & iImgName );
std::string GetEnvMapPath( const std::string & iEnvMapName );

}

}

#endif
