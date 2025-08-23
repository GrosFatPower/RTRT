#ifndef _EnvMap_
#define _EnvMap_

#include <string>
#include "MathUtil.h"

#include "GL/glew.h"

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
  float * GetCDF() const { return _CDF; }

  float GetTotalWeight() const { return _TotalWeight; }

  GLuint GetHandle() const { return _Handle; }
  void SetHandle( GLuint iHandle) { _Handle = iHandle; }

  Vec4 Sample( int iX, int iY ) const;
  Vec4 Sample( Vec2 iUV ) const;
  Vec4 BiLinearSample( Vec2 iUV ) const;

  const std::string & Filename() const { return _Filename; }

private:

  void BuildCDF();

  bool          _IsInitialized = false;
  GLuint        _Handle        = 0;
  int           _Width         = 0;
  int           _Height        = 0;

  std::string   _Filename      = "";
  float       * _RawData       = nullptr;
  float       * _CDF           = nullptr; // Cumulative distribution function
  float         _TotalWeight   = 0.f;

  EnvMap( const EnvMap & ); // not implemented
  EnvMap & operator=( const EnvMap & ); // not implemented
};

}

#endif /* _EnvMap_ */
