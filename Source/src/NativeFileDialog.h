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

NativeFileDialogResult OpenSceneFileDialog( const std::filesystem::path & iInitialDirectory, std::filesystem::path & oSelectedPath, std::string & oError );

}

#endif /* _NativeFileDialog_ */
