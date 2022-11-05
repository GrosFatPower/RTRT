#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#include <iostream>

namespace RTRT
{

Texture::~Texture()
{
  if ( _TexData )
    stbi_image_free(_TexData);
  _TexData = nullptr;
}

bool Texture::Load( const std::string & iFilename, int iNbComponents )
{
  int width = 0, height = 0;
  _TexData = stbi_load(iFilename.c_str(), &width, &height, nullptr, iNbComponents);
  if ( nullptr == _TexData )
  {
    std::cout << "Texture : Unable to load image" << iFilename << std::endl;
    return false;
  }

  _Width = width;
  _Height = height;
  _NbComponents = iNbComponents;
  _Filename = iFilename;

  return true;
}

bool Texture::Resize( int iWidth, int iHeight )
{
  if ( !iWidth || !iHeight || !_TexData )
    return false;

  unsigned char * resizedData = new unsigned char[iWidth * iHeight * _NbComponents];
  if ( stbir_resize_uint8(_TexData, _Width, _Height, 0, resizedData, iWidth, iHeight, 0, _NbComponents) )
  {
    stbi_image_free(_TexData);

    _Width = iWidth;
    _Height = iHeight;
    _TexData = resizedData;

    return true;
  }
  
  return false;
}

}
