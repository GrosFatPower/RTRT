#include "Texture.h"
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif
#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#endif
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

Vec4 Texture::Sample( float iU, float iV )
{
  Vec4 sample(0.f);

  if ( _TexData )
  {
    float u = ( iU - std::floor(iU) ) * ( _Width - 1 );
    float v = ( iV - std::floor(iV) ) * ( _Height - 1 );

    int x = (int)std::floor(u);
    int y = (int)std::floor(v);

    int index = x * _NbComponents + y * _Width * _NbComponents;
    if ( _NbComponents >= 1 )
    {
      if ( _Format == TexFormat::TEX_FLOAT )
        sample.x = ((float*)_TexData)[ index ];
      else if ( _Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.x = ((unsigned char *)_TexData)[ index ] / 255;
    }
    if ( _NbComponents >= 2 )
    {
      if ( _Format == TexFormat::TEX_FLOAT )
        sample.y = ((float*)_TexData)[ index + 1 ];
      else if ( _Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.y = ((unsigned char *)_TexData)[ index + 1 ] / 255;
    }
    if ( _NbComponents >= 3 )
    {
      if ( _Format == TexFormat::TEX_FLOAT )
        sample.z = ((float*)_TexData)[ index + 2 ];
      else if ( _Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.z = ((unsigned char *)_TexData)[ index + 2 ] / 255;
    }
    if ( _NbComponents >= 4 )
    {
      if ( _Format == TexFormat::TEX_FLOAT )
        sample.w = ((float*)_TexData)[ index + 3 ];
      else if ( _Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.w = ((unsigned char *)_TexData)[ index + 3 ] / 255;
    }
  }

  return sample;
}

}
