#ifndef _ScopedOutputSilencer_
#define _ScopedOutputSilencer_

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace RTRT
{

inline int DuplicateFileDescriptor( int iDescriptor )
{
#if defined(_WIN32)
  return _dup(iDescriptor);
#else
  return dup(iDescriptor);
#endif
}

inline int RedirectFileDescriptor( int iSource, int iDestination )
{
#if defined(_WIN32)
  return _dup2(iSource, iDestination);
#else
  return dup2(iSource, iDestination);
#endif
}

inline void CloseFileDescriptor( int iDescriptor )
{
#if defined(_WIN32)
  _close(iDescriptor);
#else
  close(iDescriptor);
#endif
}

inline int OpenNullOutput()
{
#if defined(_WIN32)
  int descriptor = -1;
  return ( 0 == _sopen_s(&descriptor, "NUL", _O_WRONLY, _SH_DENYNO, 0) ) ? descriptor : -1;
#else
  return open("/dev/null", O_WRONLY);
#endif
}

inline int GetFileDescriptor( FILE * iFile )
{
#if defined(_WIN32)
  return _fileno(iFile);
#else
  return fileno(iFile);
#endif
}

class ScopedOutputSilencer
{
public:
  explicit ScopedOutputSilencer( bool iEnabled )
    : _Enabled(iEnabled)
  {
    if ( _Enabled )
    {
      _OldCout = std::cout.rdbuf(_Output.rdbuf());
      _OldCerr = std::cerr.rdbuf(_Output.rdbuf());
      std::fflush(stdout);
      std::fflush(stderr);
      _StdoutDescriptor = GetFileDescriptor(stdout);
      _StderrDescriptor = GetFileDescriptor(stderr);
      _OldStdout = DuplicateFileDescriptor(_StdoutDescriptor);
      _OldStderr = DuplicateFileDescriptor(_StderrDescriptor);
      _NullOutput = OpenNullOutput();
      if ( ( _OldStdout >= 0 ) && ( _OldStderr >= 0 ) && ( _NullOutput >= 0 ) )
      {
        RedirectFileDescriptor(_NullOutput, _StdoutDescriptor);
        RedirectFileDescriptor(_NullOutput, _StderrDescriptor);
      }
    }
  }

  ~ScopedOutputSilencer()
  {
    if ( _Enabled )
    {
      std::fflush(stdout);
      std::fflush(stderr);
      if ( _OldStdout >= 0 )
      {
        RedirectFileDescriptor(_OldStdout, _StdoutDescriptor);
        CloseFileDescriptor(_OldStdout);
      }
      if ( _OldStderr >= 0 )
      {
        RedirectFileDescriptor(_OldStderr, _StderrDescriptor);
        CloseFileDescriptor(_OldStderr);
      }
      if ( _NullOutput >= 0 )
        CloseFileDescriptor(_NullOutput);
      std::cout.rdbuf(_OldCout);
      std::cerr.rdbuf(_OldCerr);
    }
  }

private:
  bool _Enabled = false;
  std::ostringstream _Output;
  std::streambuf * _OldCout = nullptr;
  std::streambuf * _OldCerr = nullptr;
  int _StdoutDescriptor = -1;
  int _StderrDescriptor = -1;
  int _OldStdout = -1;
  int _OldStderr = -1;
  int _NullOutput = -1;
};

}

#endif /* _ScopedOutputSilencer_ */
