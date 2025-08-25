#include "Texture.h"
#include <iostream>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif
#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#pragma warning(push)
#pragma warning( disable : 4996 )
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma warning(pop)
#endif

namespace RTRT
{

Texture::Texture(const std::string & iTexName, unsigned char * iTexData, int iWidth, int iHeight, int iNbComponents)
: _Filename(iTexName)
, _Width(iWidth)
, _Height(iHeight)
, _NbComponents(iNbComponents)
{
  _TexData = new unsigned char[iWidth * iHeight * iNbComponents];
  memcpy(_TexData, iTexData, iWidth * iHeight * iNbComponents);
}

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

Vec4 Texture::Sample(int iX, int iY) const
{
  Vec4 sample(1.f);

  if (_TexData)
  {
    int index = (iX + iY * _Width) * _NbComponents;

    if (_Format == TexFormat::TEX_FLOAT)
    {
      memcpy(&sample.x, &((float*)_TexData)[index], _NbComponents * sizeof(float));
    }
    else if (_Format == TexFormat::TEX_UNSIGNED_BYTE)
    {
      unsigned char SampleUC[4] = { 255, 255, 255, 255 };
      memcpy(&SampleUC, &((unsigned char*)_TexData)[index], _NbComponents * sizeof(unsigned char));
      sample.x = SampleUC[0] / 255.f;
      sample.y = SampleUC[1] / 255.f;
      sample.z = SampleUC[2] / 255.f;
      sample.w = SampleUC[3] / 255.f;
    }
  }

  return sample;
}

void Texture::Sample(int iX, int iY, int iSpan, Vec4* oSamples) const
{
  if (_TexData)
  {
    int index = (iX + iY * _Width) * _NbComponents;

    if (_Format == TexFormat::TEX_FLOAT)
    {
      float SamplesF[4] = { 1.f, 1.f, 1.f, 1.f };
      memcpy(&SamplesF, &((float*)_TexData)[index], _NbComponents * sizeof(float) * iSpan);
      
      for ( int i = 0; i < iSpan; ++i )
      {
        oSamples[i].x = SamplesF[i * _NbComponents + 0];
        if (_NbComponents >= 2)
          oSamples[i].y = SamplesF[i * _NbComponents + 1];
        if (_NbComponents >= 3)
          oSamples[i].z = SamplesF[i * _NbComponents + 2];
        if (_NbComponents >= 4)
          oSamples[i].w = SamplesF[i * _NbComponents + 3];
      }
    }
    else if (_Format == TexFormat::TEX_UNSIGNED_BYTE)
    {
      unsigned char SampleUC[4] = { 255, 255, 255, 255 };
      memcpy(&SampleUC, &((unsigned char*)_TexData)[index], _NbComponents * sizeof(unsigned char) * iSpan);

      for (int i = 0; i < iSpan; ++i)
      {
        oSamples[i].x = SampleUC[i * _NbComponents + 0] / 255.f;
        if (_NbComponents >= 2)
          oSamples[i].y = SampleUC[i * _NbComponents + 1] / 255.f;
        if (_NbComponents >= 3)
          oSamples[i].z = SampleUC[i * _NbComponents + 2] / 255.f;
        if (_NbComponents >= 4)
          oSamples[i].w = SampleUC[i * _NbComponents + 3] / 255.f;
      }
    }
  }
}

/*
Vec4 Texture::Sample(int iX, int iY) const
{
  Vec4 sample(1.f);

  if (_TexData)
  {
    int index = (iX + iY * _Width) * _NbComponents;
    if (_NbComponents >= 1)
    {
      if (_Format == TexFormat::TEX_FLOAT)
        sample.x = ((float*)_TexData)[index];
      else if (_Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.x = ((unsigned char*)_TexData)[index] / 255.f;
    }
    if (_NbComponents >= 2)
    {
      if (_Format == TexFormat::TEX_FLOAT)
        sample.y = ((float*)_TexData)[index + 1];
      else if (_Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.y = ((unsigned char*)_TexData)[index + 1] / 255.f;
    }
    if (_NbComponents >= 3)
    {
      if (_Format == TexFormat::TEX_FLOAT)
        sample.z = ((float*)_TexData)[index + 2];
      else if (_Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.z = ((unsigned char*)_TexData)[index + 2] / 255.f;
    }
    if (_NbComponents >= 4)
    {
      if (_Format == TexFormat::TEX_FLOAT)
        sample.w = ((float*)_TexData)[index + 3];
      else if (_Format == TexFormat::TEX_UNSIGNED_BYTE)
        sample.w = ((unsigned char*)_TexData)[index + 3] / 255.f;
    }
  }

  return sample;
}
*/

Vec4 Texture::Sample( Vec2 iUV ) const
{
  iUV.x = ( iUV.x - std::floor(iUV.x) ) * ( _Width - 1 );
  iUV.y = ( iUV.y - std::floor(iUV.y) ) * ( _Height - 1 );

  int x = (int)std::floor(iUV.x);
  int y = (int)std::floor(iUV.y);

  return Sample(x, y);
}

Vec4 Texture::BiLinearSample( Vec2 iUV ) const
{
  iUV.x = ( iUV.x - std::floor(iUV.x) ) * ( _Width - 1 );
  iUV.y = ( iUV.y - std::floor(iUV.y) ) * ( _Height - 1 );

  int x = (int)std::floor(iUV.x);
  int y = (int)std::floor(iUV.y);

  float uf = iUV.x - x;
  float vf = iUV.y - y;

  Vec4 Samples[4];
  Samples[0] = Sample(x, y);
  Samples[1] = Sample(std::min(x+1,_Width-1), y);
  Samples[2] = Sample(x, std::min(y+1,_Height-1));
  Samples[3] = Sample(std::min(x+1,_Width-1), std::min(y+1,_Height-1));
  /*
  int span = (x < (_Width - 1)) ? 2 : 1;
  Sample(x, y, span, &Samples[0]);
  Sample(x, std::min(y + 1, _Height - 1), span, &Samples[2]);
  */

  return glm::mix( glm::mix(Samples[0], Samples[1], uf), glm::mix(Samples[2], Samples[3], uf), vf );
}

}
