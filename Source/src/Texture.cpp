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

bool Texture::Load( const std::string & iFilename, int iNbComponents, TexFormat iFormat )
{
  int width = 0, height = 0;

  if ( _TexData )
    stbi_image_free(_TexData);
  _TexData = nullptr;

  if ( TexFormat::TEX_UNSIGNED_BYTE == iFormat )
    _TexData = stbi_load(iFilename.c_str(), &width, &height, nullptr, iNbComponents);
  else if ( TexFormat::TEX_FLOAT == iFormat )
    _TexData = stbi_loadf(iFilename.c_str(), &width, &height, nullptr, iNbComponents);

  if ( nullptr == _TexData )
  {
    std::cout << "Texture : Unable to load image" << iFilename << std::endl;
    return false;
  }

  _Width = width;
  _Height = height;
  _NbComponents = iNbComponents;
  _Filename = iFilename;
  _Format = iFormat;

  return true;
}

bool Texture::Resize( int iWidth, int iHeight )
{
  if ( !iWidth || !iHeight || !_TexData )
    return false;

  if ( TexFormat::TEX_UNSIGNED_BYTE == _Format )
  {
    unsigned char * resizedData = new unsigned char[iWidth * iHeight * _NbComponents];
    if ( stbir_resize_uint8((unsigned char *)_TexData, _Width, _Height, 0, resizedData, iWidth, iHeight, 0, _NbComponents) )
    {
      stbi_image_free(_TexData);

      _Width = iWidth;
      _Height = iHeight;
      _TexData = resizedData;

      return true;
    }
  }
  else if ( TexFormat::TEX_FLOAT == _Format )
  {
    float * resizedData = new float[iWidth * iHeight * _NbComponents];
    if ( stbir_resize_float((float *)_TexData, _Width, _Height, 0, resizedData, iWidth, iHeight, 0, _NbComponents) )
    {
      stbi_image_free(_TexData);

      _Width = iWidth;
      _Height = iHeight;
      _TexData = resizedData;

      return true;
    }
  }
  
  return false;
}

}
