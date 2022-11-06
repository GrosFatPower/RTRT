#ifndef _Loader_
#define _Loader_

#include <string>

namespace RTRT
{

class Scene;
class MeshData;

class Loader
{
public:

  static bool LoadScene(const std::string & iFilename, Scene * oScene);

  static bool LoadMesh( const std::string & iFilename, MeshData * oMeshData );

private:

  static bool LoadFromSceneFile(const std::string & iFilename, Scene * oScene);

  Loader();
  Loader( const Loader &);
  Loader & operator=( const Loader &);
};

}

#endif /* _Loader_ */
