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

  _IsInitialized = false;
  _TexID         = -1;
  _Width         = 0;
  _Height        = 0;
  _Filename      = "";
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

  return true;
}

}
