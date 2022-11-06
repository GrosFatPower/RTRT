#ifndef _Scene_
#define _Scene_

#include "Light.h"
#include "Camera.h"
#include "Material.h"
#include "MeshInstance.h"
#include <vector>

namespace RTRT
{

class Shape;
class Texture;

class Scene
{
public:
  Scene();
  virtual ~Scene();

private:

  Camera                    _Camera;
  std::vector<Light>        _Lights;
  std::vector<Material>     _Materials;
  std::vector<MeshInstance> _MeshInstances;

  std::vector<Shape*>       _Shapes;
  std::vector<Texture*>     _Textures;

  friend class Loader;
};

}

#endif /* _Scene_ */
