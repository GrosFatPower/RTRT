#ifndef _Loader_
#define _Loader_

#include <string>
#include <iostream>

namespace RTRT
{

class Scene;
class MeshData;
class Material;
class Light;
class Camera;

class Loader
{
public:

  static bool LoadScene(const std::string & iFilename, Scene * oScene);

  static bool LoadMeshData( const std::string & iFilename, MeshData * oMeshData );

private:

  static bool LoadFromSceneFile(const std::string & iFilename, Scene * oScene);

  static int ParseMaterial( std::ifstream & iStr, Material & oMaterial );
  static int ParseLight( std::ifstream & iStr, Light & oLight );
  static int ParseCamera( std::ifstream & iStr, Camera & oCamera );
  static int ParseMeshData( std::ifstream & iStr, MeshData & oMeshData );
  static int ParseRenderer( std::ifstream & iStr/*, RenderOtions & oRenderer*/ );
  static int ParseGLTF( std::ifstream & iStr, Scene & ioScene );

  Loader();
  Loader( const Loader &);
  Loader & operator=( const Loader &);
};

}

#endif /* _Loader_ */
