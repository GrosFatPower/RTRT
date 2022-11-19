#ifndef _Scene_
#define _Scene_

#include "Light.h"
#include "Camera.h"
#include "Material.h"
#include "MeshInstance.h"
#include "Object.h"
#include "ObjectInstance.h"
#include <vector>
#include <map>

namespace RTRT
{

class Texture;
class Mesh;
struct Material;

class Scene
{
public:
  Scene();
  virtual ~Scene();

  int AddTexture( const std::string & iFilename );
  int AddMaterial( Material & ioMaterial, const std::string & iName );

  int AddMesh( const std::string & iFilename );
  int AddMeshInstance( MeshInstance & iMeshInstance );

  int AddObject( const Object & iObject );
  int AddObjectInstance( ObjectInstance & iObjectInstance );
  int AddObjectInstance( int iObjectID, int iMaterialID, const Mat4x4 & iTransform );

  void SetCamera( const Camera & iCamera ) { _Camera = iCamera; }
  Camera & GetCamera() { return _Camera; }

  void AddLight( const Light & iLight ) { _Lights.push_back(iLight); }
  Light * GetLight( unsigned int iIndex ) { return ( iIndex < _Lights.size() ) ? &_Lights[iIndex] : nullptr; }

  int FindMaterialID( const std::string & iMateralName );

  int GetNbLights()          { return _Lights.size();          }
  int GetNbMaterials()       { return _Materials.size();       }
  int GetNbTextures()        { return _Textures.size();        }
  int GetNbMeshes()          { return _Meshes.size();          }
  int GetNbMeshInstances()   { return _MeshInstances.size();   }
  int GetNbObjectInstances() { return _ObjectInstances.size(); }

  const std::vector<MeshInstance>   & GetMeshInstances()   { return _MeshInstances;   }
  const std::vector<ObjectInstance> & GetObjectInstances() { return _ObjectInstances; }

private:

  Camera                      _Camera;
  std::vector<Light>          _Lights;
  std::vector<Material>       _Materials;
  std::map<std::string,int>   _MaterialIDs;
  std::vector<MeshInstance>   _MeshInstances;
  std::vector<ObjectInstance> _ObjectInstances;

  std::vector<Texture*>       _Textures;
  std::vector<Mesh*>          _Meshes;
  std::vector<Object*>        _Objects;
};

}

#endif /* _Scene_ */
