#ifndef _RGBA8_
#define _RGBA8_

#include "MathUtil.h"

#include "stdint.h"
#include <algorithm>

namespace RTRT
{

class RGBA8
{
public:
  uint8_t _R;
  uint8_t _G;
  uint8_t _B;
  uint8_t _A;
  
  RGBA8() = default;
  RGBA8( uint8_t iR, uint8_t iG, uint8_t iB, uint8_t iA = 255 ) : _R(iR), _G(iG), _B(iB), _A(iA) { }
  RGBA8( Vec4b iVec ) : _R(iVec.r), _G(iVec.g), _B(iVec.b), _A(iVec.a) { }
  RGBA8( Vec4 iRGBA32 );
  RGBA8( Vec3 iRGB32, float iA );
  RGBA8( float iR, float iG, float iB, float iA );

  void Load( float iR, float iG, float iB, float iA );

  RGBA8 operator *( float iVal ) const;
  RGBA8 & operator *=( float iVal );
  RGBA8 operator +( float iVal ) const;
  RGBA8 & operator +=( float iVal );
};


inline RGBA8::RGBA8( Vec4 iRGBA32 )
{
  Load(iRGBA32.r, iRGBA32.g, iRGBA32.b, iRGBA32.a);
}

inline RGBA8::RGBA8( Vec3 iRGB32, float iA )
{
  Load(iRGB32.r, iRGB32.g, iRGB32.b, iA);
}

inline RGBA8::RGBA8( float iR, float iG, float iB, float iA )
{
  Load(iA, iG, iB, iA);
}

inline void RGBA8::Load( float iR, float iG, float iB, float iA )
{
  _R = uint8_t(std::clamp(255.f * iR, 0.f, 255.f));
  _G = uint8_t(std::clamp(255.f * iG, 0.f, 255.f));
  _B = uint8_t(std::clamp(255.f * iB, 0.f, 255.f));
  _A = uint8_t(std::clamp(255.f * iA, 0.f, 255.f));
}

inline RGBA8 RGBA8::operator *( float iVal ) const
{
  return RGBA8( uint8_t( iVal * _R ), uint8_t( iVal * _G ), uint8_t( iVal * _B ) );
}

inline RGBA8 & RGBA8::operator *=( float iVal )
{
  _R = uint8_t( iVal * _R );
  _G = uint8_t( iVal * _G );
  _B = uint8_t( iVal * _B );

  return *this;
}

inline RGBA8 RGBA8::operator +( float iVal ) const
{
  return RGBA8( uint8_t( iVal + _R ), uint8_t( iVal + _G ), uint8_t( iVal + _B ) );
}

inline RGBA8 & RGBA8::operator +=( float iVal )
{
  _R = uint8_t( iVal + _R );
  _G = uint8_t( iVal + _G );
  _B = uint8_t( iVal + _B );

  return *this;
}

}

#endif /* _RGBA8_ */
