#ifndef _NativeFileDialog_
#define _NativeFileDialog_

#include <filesystem>
#include <string>

namespace RTRT
{

enum class NativeFileDialogResult
{
  Selected,
  Cancelled,
  Error
};

NativeFileDialogResult OpenFileDialog( const char * iFilterList, const std::filesystem::path & iInitialDirectory, std::filesystem::path & oSelectedPath, std::string & oError );
NativeFileDialogResult SaveFileDialog( const char * iFilterList, const std::filesystem::path & iInitialDirectory, std::filesystem::path & oSelectedPath, std::string & oError );

}

#endif /* _NativeFileDialog_ */
