#include "EnvMap.h"
#include "stb_image.h"
#include <iostream>

namespace RTRT
{

EnvMap::~EnvMap()
{
  Reset();
}

void EnvMap::Reset()
{
  if ( _RawData )
    stbi_image_free(_RawData);
  _RawData = nullptr;

  DeleteTab(_CDF);

  _IsInitialized = false;
  _Handle        = -1;
  _Width         = 0;
  _Height        = 0;
  _Filename      = "";
  _TotalWeight   = 0.f;
}

bool EnvMap::Load( const std::string & iFilename )
{
  Reset();

  int width = 0, height = 0;
  _RawData = stbi_loadf(iFilename.c_str(), &width, &height, nullptr, 3);

  if ( nullptr == _RawData )
  {
    std::cout << "EnvMap : Unable to load image" << iFilename << std::endl;
    return false;
  }

  _Width         = width;
  _Height        = height;
  _Filename      = iFilename;
  _IsInitialized = true;

  BuildCDF();

  return true;
}

void EnvMap::BuildCDF()
{
  DeleteTab(_CDF);
  _TotalWeight = 0.f;

  if ( !_RawData || !_Width || !_Height )
    return;

  // Gather weights for CDF
  std::vector<float> weights;
  weights.resize(_Width * _Height);
  for ( int y = 0; y < _Height; ++y )
  {
    for ( int x = 0; x < _Width; ++x )
    {
      int index = x + y * _Width;
      weights[index] = MathUtil::Luminance(Vec3(_RawData[index * 3 + 0], _RawData[index * 3 + 1], _RawData[index * 3 + 2]));
    }
  }

  // Build CDF
  _CDF = new float[_Width * _Height];
  _CDF[0] = weights[0];

  for ( int i = 1; i < ( _Width * _Height ); ++i )
    _CDF[i] = _CDF[i - 1] + weights[i];

  _TotalWeight = _CDF[_Width * _Height - 1];
}

Vec4 EnvMap::Sample( int iX, int iY ) const
{
  Vec4 sample(1.f);

  if ( _RawData )
  {
    int index = ( iX + iY * _Width ) * 3;
    sample.x = _RawData[ index ];
    sample.y = _RawData[ index + 1 ];
    sample.z = _RawData[ index + 2 ];
  }

  return sample;
}

Vec4 EnvMap::Sample( Vec2 iUV ) const
{
  iUV.x = ( iUV.x - std::floor(iUV.x) ) * ( _Width - 1 );
  iUV.y = ( iUV.y - std::floor(iUV.y) ) * ( _Height - 1 );

  int x = (int)std::floor(iUV.x);
  int y = (int)std::floor(iUV.y);

  return Sample(x, y);
}

Vec4 EnvMap::BiLinearSample( Vec2 iUV ) const
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

  return glm::mix( glm::mix(Samples[0], Samples[1], uf), glm::mix(Samples[2], Samples[3], uf), vf );
}


}
