#include "RenderTestGLTFUtil.h"

#include "Loader.h"
#include "Mesh.h"
#include "PathUtils.h"
#include "RenderSettings.h"
#include "Scene.h"

#include <cmath>
#include <iostream>

namespace RTRT
{
namespace Tests
{
namespace GLTFTestUtil
{

bool CheckStaticImport()
{
  Scene scene;
  RenderSettings settings;
  if ( !Loader::LoadScene(PathUtils::GetAssetPath("GLTFTests/StaticImport.gltf"), scene, settings) )
  {
    std::cerr << "Unable to load static GLTF test fixture." << std::endl;
    return false;
  }

  if ( ( 1 != scene.GetNbMeshes() ) || ( 1 != scene.GetNbMeshInstances() ) || ( 1 != scene.GetNbLights() ) || ( scene.GetNbMaterials() < 1 ) )
    return false;

  const std::vector<Mesh*> & meshes = scene.GetMeshes();
  if ( !meshes[0] || ( 3 != meshes[0] -> GetVertices().size() ) || ( 3 != meshes[0] -> GetNormals().size() ) || ( 3 != meshes[0] -> GetIndices().size() ) )
    return false;

  const MeshInstance & instance = scene.GetMeshInstances()[0];
  if ( ( std::abs(instance._Transform[3][0] - 1.f) > .0001f ) || ( std::abs(instance._Transform[3][1] - 2.f) > .0001f ) || ( std::abs(instance._Transform[3][2] - 3.f) > .0001f ) )
    return false;

  Light * light = scene.GetLight(0);
  const Camera & camera = scene.GetCamera();
  return light
      && ( LightType::DistantLight == static_cast<LightType>(light -> _Type) )
      && ( std::abs(light -> _Pos.z - 1.f) < .0001f )
      && ( std::abs(camera.GetPos().x - 1.f) < .0001f )
      && ( std::abs(camera.GetPos().y - 2.f) < .0001f )
      && ( std::abs(camera.GetPos().z - 3.f) < .0001f )
      && ( std::abs(camera.GetForward().z + 1.f) < .0001f );
}

}
}
}
