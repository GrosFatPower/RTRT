#ifndef _Texture_
#define _Texture_

#include <string>
#include "MathUtil.h"

namespace RTRT
{

enum class TexFormat
{
  TEX_UNSIGNED_BYTE = 0,
  TEX_FLOAT
};

class Texture
{
public:
  Texture() {}
  virtual ~Texture();

  bool Load( const std::string & iFilename, int iNbComponents = 4, TexFormat iFormat = TexFormat::TEX_UNSIGNED_BYTE );

  bool Resize( int iWidth, int iHeight );

  int GetWidth() const { return _Width; }
  int GetHeight() const { return _Height; }
  int GetNbComponents() const { return _NbComponents; }
  unsigned char * GetUCData() const { return ( TexFormat::TEX_UNSIGNED_BYTE == _Format ) ? ( (unsigned char *)_TexData ) : ( nullptr ); }
  float * GetFData() const { return ( TexFormat::TEX_FLOAT == _Format ) ? ( (float *)_TexData ) : ( nullptr ); }

  int GetTexID() const { return _TexID; }
  void SetTexID( int iTextID ) { _TexID = iTextID; }

  const std::string & Filename() const { return _Filename; }

  Vec4 Sample( int iX, int iY ) const;
  Vec4 Sample( Vec2 iUV ) const;
  Vec4 BiLinearSample( Vec2 iUV ) const;

private:
  int             _TexID        = -1;
  int             _Width        = 0;
  int             _Height       = 0;
  int             _NbComponents = 0;
  TexFormat       _Format       = TexFormat::TEX_UNSIGNED_BYTE;

  std::string     _Filename = "";
  void          * _TexData = nullptr;

  Texture( const Texture & ); // not implemented
  Texture & operator=( const Texture & ); // not implemented
};

}

#endif /* _Texture_ */
