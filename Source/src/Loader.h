#ifndef _Loader_
#define _Loader_

#include <string>
#include <iostream>

namespace RTRT
{

class Scene;
class Mesh;
struct Material;
struct Light;
struct Camera;
struct RenderSettings;

class Loader
{
public:

  static bool LoadScene(const std::string & iFilename, Scene * oScene, RenderSettings & oRenderSettings);

private:

  static bool LoadFromSceneFile(const std::string & iFilename, Scene * oScene, RenderSettings & oRenderSettings);

  static int ParseMaterial( std::ifstream & iStr, const std::string & iPath, Material & oMaterial, Scene & ioScene );
  static int ParseLight( std::ifstream & iStr, Light & oLight );
  static int ParseCamera( std::ifstream & iStr, Camera & oCamera );
  static int ParseMeshData( std::ifstream & iStr, const std::string & iPath, Mesh & oMesh, Scene & ioScene );
  static int ParseRenderSettings( std::ifstream & iStr, RenderSettings & oSettings );
  static int ParseGLTF( std::ifstream & iStr, Scene & ioScene );

  Loader();
  Loader( const Loader &);
  Loader & operator=( const Loader &);
};

}

#endif /* _Loader_ */
