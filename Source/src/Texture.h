#ifndef _Texture_
#define _Texture_

#include <string>

namespace RTRT
{

class Texture
{
public:
  Texture() {}
  virtual ~Texture();

  bool Load( const std::string & iFilename, int iNbComponents = 4 );

  bool Resize( int iWidth, int iHeight );

  int GetWidth() const { return _Width; }
  int GetHeight() const { return _Height; }
  int GetNbComponents() const { return _NbComponents; }
  unsigned char * GetData() const { return _TexData; }

  const std::string & Filename() const { return _Filename; }

private:
  int             _Width = 0;
  int             _Height = 0;
  int             _NbComponents = 0;

  std::string     _Filename = "";
  unsigned char * _TexData = nullptr;

  Texture( const Texture & ); // not implemented
  Texture & operator=( const Texture & ); // not implemented
};

}

#endif /* _Texture_ */
