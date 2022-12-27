#ifndef _Loader_
#define _Loader_

#include <string>
#include <iostream>

namespace RTRT
{

class Scene;
struct RenderSettings;

class Loader
{
public:

  static bool LoadScene(const std::string & iFilename, Scene *& oScene, RenderSettings & oRenderSettings);

private:

  static bool LoadFromSceneFile(const std::string & iFilename, Scene *& oScene, RenderSettings & oRenderSettings);

  static int ParseMaterial( std::ifstream & iStr, const std::string & iPath, const std::string & iMaterialName, Scene & ioScene );
  static int ParseLight( std::ifstream & iStr, Scene & ioScene );
  static int ParseCamera( std::ifstream & iStr, Scene & ioScene );
  static int ParseSphere( std::ifstream & iStr, Scene & ioScene );
  static int ParseBox( std::ifstream & iStr, Scene & ioScene );
  static int ParsePlane( std::ifstream & iStr, Scene & ioScene );
  static int ParseMeshData( std::ifstream & iStr, const std::string & iPath, Scene & ioScene );
  static int ParseRenderSettings( std::ifstream & iStr, RenderSettings & oSettings, Scene & ioScene );
  static int ParseGLTF( std::ifstream & iStr, Scene & ioScene );

  Loader();
  Loader( const Loader &);
  Loader & operator=( const Loader &);
};

}

#endif /* _Loader_ */
