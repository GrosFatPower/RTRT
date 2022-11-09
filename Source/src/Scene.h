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

class Scene
{
public:
  Scene();
  virtual ~Scene();

  int AddTexture( const std::string & iFilename );

  int AddMaterial( Material & ioMaterial, const std::string & iName );

  std::string GetMaterialName( int MatID );

private:

  Camera                    _Camera;
  std::vector<Light>        _Lights;
  std::vector<Material>     _Materials;
  std::map<int,std::string> _MaterialsName;
  std::vector<MeshInstance> _MeshInstances;

  std::vector<Shape*>       _Shapes;
  std::vector<Texture*>     _Textures;

  friend class Loader;
};

}

#endif /* _Scene_ */
