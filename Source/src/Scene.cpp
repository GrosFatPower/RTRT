#include "Scene.h"
#include "Mesh.h"
#include "Texture.h"
#include <iostream>

namespace RTRT
{

Scene::Scene()
  : _Camera({0.f,0.f,-1.f}, {0.f,0.f,0.f}, 80.f)
{
}

Scene::~Scene()
{
  for (auto & texture : _Textures)
    delete texture;
  _Textures.clear();

  for (auto & mesh : _Meshes)
    delete mesh;
  _Meshes.clear();

  for (auto & object : _Objects)
    delete object;
  _Objects.clear();
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

int Scene::AddMesh( const std::string & iFilename )
{
  int meshID = -1;

  for ( auto & mesh : _Meshes )
  {
    if ( mesh && ( mesh -> Filename() == iFilename ) )
    {
      meshID = mesh -> GetMeshID();
      break;
    }
  }

  if ( meshID < 0 )
  {
    Mesh * newMesh = new Mesh;

    std::cout << "Scene : Loading mesh " << iFilename << std::endl;
    if ( newMesh -> Load(iFilename) )
    {
      meshID = _Meshes.size();
      newMesh -> SetMeshID(meshID);
      _Meshes.push_back(newMesh);
    }
    else
    {
      delete newMesh;
      newMesh =  nullptr;
    }
  }

  if ( meshID < 0 )
    std::cout << "Scene : ERROR. Unable to load mesh " << iFilename << std::endl;

  return meshID;
}

int Scene::AddMaterial( Material & ioMaterial, const std::string & iName )
{
  int matID = _Materials.size();
  ioMaterial._ID = (float)matID;
  _Materials.push_back(ioMaterial);

  _MaterialIDs.insert({iName, matID});

  return matID;
}

int Scene::AddMeshInstance( MeshInstance & iMeshInstance )
{
  int instanceID = _MeshInstances.size();
  _MeshInstances.push_back(iMeshInstance);
  return instanceID;
}

int Scene::FindMaterialID( const std::string & iMateralName )
{
  int matID = -1;

  auto search = _MaterialIDs.find(iMateralName);
  if ( search != _MaterialIDs.end() )
    matID = search -> second;

  return matID;
}

int Scene::AddObject( const Object & iObject )
{
  int objectID = _Objects.size();

  Object * newObject = new Object(iObject);
  newObject -> _ObjectID = objectID;
  _Objects.push_back(newObject);

  return objectID;
}

int Scene::AddObjectInstance( ObjectInstance & iObjectInstance )
{
  int instanceID = _ObjectInstances.size();
  _ObjectInstances.push_back(iObjectInstance);
  return instanceID;
}

int Scene::AddObjectInstance( int iObjectID, int iMaterialID, const Mat4x4 & iTransform )
{
  ObjectInstance instance(iObjectID, iMaterialID, iTransform);
  return AddObjectInstance(instance);
}

}
