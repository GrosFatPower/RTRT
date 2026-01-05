#include "Texture.h"
#include <iostream>
#include <cmath>
#include <algorithm>

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

// free mip data helper
static void FreeMipData(std::vector<void*>& mipData)
{
  for (void* p : mipData)
    if (p) stbi_image_free(p);
  mipData.clear();
}

// helper to sample a single texel from a given mip level; returns vec4
static Vec4 SampleTexelFromLevel(const void* levelPtr, int levelWidth, int levelHeight, int nbComponents, bool isFloat, int x, int y)
{
  Vec4 sample(1.f);
  x = std::clamp(x, 0, levelWidth - 1);
  y = std::clamp(y, 0, levelHeight - 1);
  size_t idx = (static_cast<size_t>(x) + static_cast<size_t>(y) * levelWidth) * nbComponents;
  if (isFloat)
  {
    const float* src = static_cast<const float*>(levelPtr);
    if (nbComponents >= 1) sample.x = src[idx + 0];
    if (nbComponents >= 2) sample.y = src[idx + 1];
    if (nbComponents >= 3) sample.z = src[idx + 2];
    if (nbComponents >= 4) sample.w = src[idx + 3]; else sample.w = 1.f;
  }
  else
  {
    const unsigned char* src = static_cast<const unsigned char*>(levelPtr);
    sample.x = (nbComponents >= 1) ? (src[idx + 0] / 255.0f) : 0.f;
    sample.y = (nbComponents >= 2) ? (src[idx + 1] / 255.0f) : 0.f;
    sample.z = (nbComponents >= 3) ? (src[idx + 2] / 255.0f) : 0.f;
    sample.w = (nbComponents >= 4) ? (src[idx + 3] / 255.0f) : 1.f;
  }
  return sample;
}

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
  FreeMipData(_MipData);
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
  // default behavior: base-level bilinear
  return BiLinearSample(iUV, 0.f);
}

Vec4 Texture::BiLinearSample( Vec2 iUV, float iLOD ) const
{
  // if no mipmaps available, fall back to classic implementation
  if (_MipLevels <= 0)
  {
    // existing code
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
    return glm::mix( glm::mix(Samples[0], Samples[1], uf), glm::mix(Samples[2], Samples[3], uf), vf );
  }

  // choose mip level from LOD: clamp to available integer levels, do simple linear between two levels if fractional
  float clampedLOD = std::clamp(iLOD, 0.0f, static_cast<float>(_MipLevels - 1));
  int level0 = static_cast<int>(std::floor(clampedLOD));
  int level1 = std::min(level0 + 1, _MipLevels - 1);
  float frac = clampedLOD - level0;

  auto sampleAtLevel = [&](int level)->Vec4
  {
    int lw = _MipWidths[level];
    int lh = _MipHeights[level];
    // convert uv to texel space for that level
    float u = ( iUV.x - std::floor(iUV.x) ) * ( lw - 1 );
    float v = ( iUV.y - std::floor(iUV.y) ) * ( lh - 1 );
    int x = (int)std::floor(u);
    int y = (int)std::floor(v);
    float uf = u - x;
    float vf = v - y;

    const void* levelPtr = _MipData[level];
    Vec4 s00 = SampleTexelFromLevel(levelPtr, lw, lh, _NbComponents, _Format == TexFormat::TEX_FLOAT, x, y);
    Vec4 s10 = SampleTexelFromLevel(levelPtr, lw, lh, _NbComponents, _Format == TexFormat::TEX_FLOAT, x+1, y);
    Vec4 s01 = SampleTexelFromLevel(levelPtr, lw, lh, _NbComponents, _Format == TexFormat::TEX_FLOAT, x, y+1);
    Vec4 s11 = SampleTexelFromLevel(levelPtr, lw, lh, _NbComponents, _Format == TexFormat::TEX_FLOAT, x+1, y+1);

    return glm::mix(glm::mix(s00, s10, uf), glm::mix(s01, s11, uf), vf);
  };

  Vec4 c0 = sampleAtLevel(level0);
  if (level0 == level1 || frac <= 0.0001f)
    return c0;
  Vec4 c1 = sampleAtLevel(level1);
  return glm::mix(c0, c1, frac);
}

void Texture::GenerateMipMaps()
{
  // free existing mip data first (except level 0 which is in _TexData)
  FreeMipData(_MipData);
  _MipWidths.clear();
  _MipHeights.clear();
  _MipLevels = 0;

  if (!_TexData || _Width <= 0 || _Height <= 0)
    return;

  // level 0 uses existing _TexData pointer; for simplicity we will copy level0 to _MipData[0]
  const bool isFloat = (_Format == TexFormat::TEX_FLOAT);
  const int nc = _NbComponents;

  // make level0 copy (so all levels live in _MipData and can be freed uniformly)
  size_t level0Size = static_cast<size_t>(_Width) * _Height * nc;
  if (isFloat)
  {
    float* data0 = (float*)stbi__malloc(level0Size * sizeof(float));
    memcpy(data0, _TexData, level0Size * sizeof(float));
    _MipData.push_back(data0);
  }
  else
  {
    unsigned char* data0 = (unsigned char*)stbi__malloc(level0Size * sizeof(unsigned char));
    memcpy(data0, _TexData, level0Size * sizeof(unsigned char));
    _MipData.push_back(data0);
  }
  _MipWidths.push_back(_Width);
  _MipHeights.push_back(_Height);

  int w = _Width;
  int h = _Height;

  // generate next levels until 1x1
  while (w > 1 || h > 1)
  {
    int nw = std::max(1, w / 2);
    int nh = std::max(1, h / 2);

    size_t dstSize = static_cast<size_t>(nw) * nh * nc;
    if (isFloat)
    {
      float* dst = (float*)stbi__malloc(dstSize * sizeof(float));
      // stbir_resize_float expects src pointer, src w,h, dst pointer etc.
      // use previous level as source:
      float* src = static_cast<float*>(_MipData.back());
      stbir_resize_float(src, w, h, 0, dst, nw, nh, 0, nc);
      _MipData.push_back(dst);
    }
    else
    {
      unsigned char* dst = (unsigned char*)stbi__malloc(dstSize * sizeof(unsigned char));
      unsigned char* src = static_cast<unsigned char*>(_MipData.back());
      stbir_resize_uint8(src, w, h, 0, dst, nw, nh, 0, nc);
      _MipData.push_back(dst);
    }

    _MipWidths.push_back(nw);
    _MipHeights.push_back(nh);

    w = nw;
    h = nh;
  }

  _MipLevels = static_cast<int>(_MipData.size());
}

}
