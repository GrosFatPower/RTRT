#ifndef _Scene_
#define _Scene_

#include "Light.h"
#include "Camera.h"
#include "Material.h"
#include "MeshInstance.h"
#include <vector>
#include <map>

namespace RTRT
{

class Texture;
class Material;
class Mesh;

class Scene
{
public:
  Scene();
  virtual ~Scene();

  int AddTexture( const std::string & iFilename );
  int AddMesh( const std::string & iFilename );
  int AddMaterial( Material & ioMaterial, const std::string & iName );
  int AddMeshInstance( MeshInstance & iMeshInstance );

  void SetCamera( const Camera & iCamera ) { _Camera = iCamera; }
  void AddLight( const Light & iLight ) { _Lights.push_back(iLight); }

  int FindMaterialID( const std::string & iMateralName );

  int GetNbLights()    { return _Lights.size();    }
  int GetNbMaterials() { return _Materials.size(); }
  int GetNbTextures()  { return _Textures.size();  }
  int GetNbMeshes()    { return _Meshes.size();    }

private:

  Camera                    _Camera;
  std::vector<Light>        _Lights;
  std::vector<Material>     _Materials;
  std::map<std::string,int> _MaterialIDs;
  std::vector<MeshInstance> _MeshInstances;

  std::vector<Texture*>     _Textures;
  std::vector<Mesh*>        _Meshes;
};

}

#endif /* _Scene_ */
