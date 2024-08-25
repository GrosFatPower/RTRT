#ifndef _EnvMap_
#define _EnvMap_

#include <string>
#include "MathUtil.h"

namespace RTRT
{

class EnvMap
{
public:
  EnvMap() {}
  virtual ~EnvMap();

  void Reset();

  bool Load( const std::string & iFilename );

  bool IsInitialized() const { return _IsInitialized; }

  int GetWidth() const { return _Width; }
  int GetHeight() const { return _Height; }
  float * GetRawData() const { return _RawData; }

  int GetTexID() const { return _TexID; }
  void SetTexID( int iTextID ) { _TexID = iTextID; }

  const std::string & Filename() const { return _Filename; }

private:
  bool          _IsInitialized = false;
  int           _TexID         = -1;
  int           _Width         = 0;
  int           _Height        = 0;

  std::string   _Filename      = "";
  float       * _RawData       = nullptr;

  EnvMap( const EnvMap & ); // not implemented
  EnvMap & operator=( const EnvMap & ); // not implemented
};

}

#endif /* _EnvMap_ */
