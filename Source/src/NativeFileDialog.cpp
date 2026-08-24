#include "NativeFileDialog.h"

#include <nfd.h>
#include <cstdlib>

namespace RTRT
{

static NativeFileDialogResult RunFileDialog( const char * iFilterList, const std::filesystem::path & iInitialDirectory, bool iSaveDialog, std::filesystem::path & oSelectedPath, std::string & oError )
{
  oSelectedPath.clear();
  oError.clear();

  if ( !iFilterList || !iFilterList[0] )
  {
    oError = "The file dialog requires at least one file extension.";
    return NativeFileDialogResult::Error;
  }

  std::filesystem::path initialDirectory = iInitialDirectory;
  std::error_code errorCode;
  if ( !initialDirectory.empty() )
  {
    initialDirectory = std::filesystem::weakly_canonical(initialDirectory, errorCode);
    if ( errorCode || !std::filesystem::is_directory(initialDirectory, errorCode) || errorCode )
      initialDirectory.clear();
  }

  // The Windows backend passes this value to SHCreateItemFromParsingName, which
  // expects the platform-native path representation rather than a generic URI-like path.
  const std::string initialDirectoryString = initialDirectory.string();
  nfdchar_t * selectedPath = nullptr;
  const nfdresult_t result = iSaveDialog
                            ? NFD_SaveDialog(iFilterList, initialDirectoryString.empty() ? nullptr : initialDirectoryString.c_str(), &selectedPath)
                            : NFD_OpenDialog(iFilterList, initialDirectoryString.empty() ? nullptr : initialDirectoryString.c_str(), &selectedPath);
  if ( NFD_OKAY == result )
  {
    if ( selectedPath )
    {
      oSelectedPath = std::filesystem::path(selectedPath);
      std::free(selectedPath);
      return NativeFileDialogResult::Selected;
    }
    oError = "The file dialog returned an empty path.";
    return NativeFileDialogResult::Error;
  }
  if ( NFD_CANCEL == result )
    return NativeFileDialogResult::Cancelled;

  const char * error = NFD_GetError();
  oError = error ? error : "Unable to open the native file dialog.";
  return NativeFileDialogResult::Error;
}

NativeFileDialogResult OpenFileDialog( const char * iFilterList, const std::filesystem::path & iInitialDirectory, std::filesystem::path & oSelectedPath, std::string & oError )
{
  return RunFileDialog(iFilterList, iInitialDirectory, false, oSelectedPath, oError);
}

NativeFileDialogResult SaveFileDialog( const char * iFilterList, const std::filesystem::path & iInitialDirectory, std::filesystem::path & oSelectedPath, std::string & oError )
{
  return RunFileDialog(iFilterList, iInitialDirectory, true, oSelectedPath, oError);
}

}
