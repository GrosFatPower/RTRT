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

  constexpr void Load(float iR, float iG, float iB, float iA) noexcept
  {
    _R = uint8_t(std::clamp(255.f * iR, 0.f, 255.f));
    _G = uint8_t(std::clamp(255.f * iG, 0.f, 255.f));
    _B = uint8_t(std::clamp(255.f * iB, 0.f, 255.f));
    _A = uint8_t(std::clamp(255.f * iA, 0.f, 255.f));
  }
  
  constexpr RGBA8() noexcept : _R(0), _G(0), _B(0), _A(0) {}
  constexpr RGBA8( uint8_t iR, uint8_t iG, uint8_t iB, uint8_t iA = 255 ) noexcept : _R(iR), _G(iG), _B(iB), _A(iA) { }
  constexpr RGBA8( Vec4b iVec ) noexcept  : _R(iVec.r), _G(iVec.g), _B(iVec.b), _A(iVec.a) { }

  RGBA8(Vec4 iRGBA32) noexcept { Load(iRGBA32.r, iRGBA32.g, iRGBA32.b, iRGBA32.a); }
  RGBA8(Vec3 iRGB32, float iA) noexcept  { Load(iRGB32.r, iRGB32.g, iRGB32.b, iA); }
  RGBA8(float iR, float iG, float iB, float iA) noexcept  { Load(iR, iG, iB, iA); }

  RGBA8 operator *( float iVal ) const;
  RGBA8 & operator *=( float iVal );
  RGBA8 operator +( float iVal ) const;
  RGBA8 & operator +=( float iVal );
};


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
