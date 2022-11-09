#include "Scene.h"
#include "MeshData.h"
#include "Texture.h"
#include <iostream>

namespace RTRT
{

Scene::Scene()
: _Camera(Vec3(0.f,0.f,-1.f), Vec3(0.f,0.f,0.f), 80.f)
{
}

Scene::~Scene()
{
  for (auto & shape : _Shapes)
    delete shape;
  _Shapes.clear();
  for (auto & texture : _Textures)
    delete texture;
  _Textures.clear();
}

int Scene::AddTexture( const std::string & iFilename )
{
  int texID = -1;

  for ( auto & tex : _Textures )
  {
    if ( tex && ( tex -> Filename() == iFilename ) )
    {
      texID = tex -> GetTexID();
      break;
    }
  }
  
  if ( texID < 0 )
  {
    Texture * texture = new Texture;

    std::cout << "Scene : Loading texture " << iFilename << std::endl;
    if ( texture -> Load(iFilename) )
    {
      texID = _Textures.size();
      texture -> SetTexID(texID);
      _Textures.push_back(texture);
    }
    else
    {
      delete texture;
      texture =  nullptr;
    }
  }

  if ( texID < 0 )
    std::cout << "Scene : ERROR. Unable to load texture " << iFilename << std::endl;

  return texID;
}

int Scene::AddMaterial( Material & ioMaterial, const std::string & iName )
{
  int matID = _Materials.size();
  ioMaterial._ID = (float)matID;
  _Materials.push_back(ioMaterial);

  _MaterialsName.insert({matID, iName});

  return matID;
}

std::string Scene::GetMaterialName( int MatID )
{
  return _MaterialsName[MatID];
}

}
