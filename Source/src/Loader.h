#ifndef _Loader_
#define _Loader_

#include "MathUtil.h"
#include <string>
#include <iostream>

namespace RTRT
{

class Scene;
struct RenderSettings;
class SceneDiagnostics;

class Loader
{
public:

  static bool LoadScene(const std::string & iFilename, Scene & oScene, RenderSettings & oRenderSettings);

private:

  static bool LoadFromSceneFile(const std::string & iFilename, Scene & oScene, RenderSettings & oRenderSettings);
  static bool LoadFromGLTF(const std::string & iGltfFilename, const Mat4x4 & iTransfoMat, Scene & ioScene, RenderSettings & ioRenderSettings, bool isBinary=false);
  static bool LoadFromOBJ(const std::string & iObjFilename, const Mat4x4 & iTransfoMat, Scene & ioScene, const std::string & iInstanceName="", int iMaterialOverride=-1);

  static int ParseMaterial( std::ifstream & iStr, const std::string & iPath, const std::string & iMaterialName, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseLight( std::ifstream & iStr, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseCamera( std::ifstream & iStr, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseSphere( std::ifstream & iStr, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseBox( std::ifstream & iStr, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParsePlane( std::ifstream & iStr, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseMeshData( std::ifstream & iStr, const std::string & iPath, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseRenderSettings( std::ifstream & iStr, const std::string & iPath, RenderSettings & oSettings, Scene & ioScene, SceneDiagnostics & ioDiagnostics, int iStartLine );
  static int ParseGLTF( std::ifstream & iStr, const std::string & iPath, Scene & ioScene, RenderSettings & ioSettings, SceneDiagnostics & ioDiagnostics, int iStartLine );

  Loader();
  Loader( const Loader &);
  Loader & operator=( const Loader &);
};

}

#endif /* _Loader_ */
