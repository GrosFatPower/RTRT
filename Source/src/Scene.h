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

class Shape;
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

  int FindMaterialID( const std::string & iMateralName );

private:

  Camera                    _Camera;
  std::vector<Light>        _Lights;
  std::vector<Material>     _Materials;
  std::map<std::string,int> _MaterialIDs;
  std::vector<MeshInstance> _MeshInstances;

  std::vector<Shape*>       _Shapes;
  std::vector<Texture*>     _Textures;
  std::vector<Mesh*>        _Meshes;

  friend class Loader;
};

}

#endif /* _Scene_ */
