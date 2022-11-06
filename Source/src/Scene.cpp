#include "Scene.h"
#include "MeshData.h"
#include "Texture.h"

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

}
