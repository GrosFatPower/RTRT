#include "RenderTestGLTFUtil.h"

#include "Loader.h"
#include "Mesh.h"
#include "PathUtils.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "RenderTestOutputUtil.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace RTRT
{
namespace Tests
{
namespace GLTFTestUtil
{

bool CheckStaticImport( bool iQuiet )
{
  Scene scene;
  RenderSettings settings;
  bool loaded = false;
  {
    ScopedOutputSilencer outputSilencer(iQuiet);
    loaded = Loader::LoadScene(PathUtils::GetAssetPath("GLTFTests/StaticImport.gltf"), scene, settings);
  }
  if ( !loaded )
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

  const std::vector<Material> & materials = scene.GetMaterials();
  if ( materials.empty() || ( std::abs(materials[0]._Roughness - .36f) > .0001f ) || ( std::abs(materials[0]._Metallic - .25f) > .0001f ) )
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

bool CheckObjImport( bool iQuiet )
{
  Scene directScene;
  RenderSettings settings;
  bool directLoaded = false;
  {
    ScopedOutputSilencer outputSilencer(iQuiet);
    directLoaded = Loader::LoadScene(PathUtils::GetAssetPath("OBJTests/Materials.obj"), directScene, settings);
  }
  if ( !directLoaded ) return false;
  if ( ( 2 != directScene.GetNbMeshes() ) || ( 2 != directScene.GetNbMeshInstances() ) || ( directScene.FindMaterialID("OBJ_Materials_Red") < 0 ) || ( directScene.FindMaterialID("OBJ_Materials_Blue") < 0 ) ) return false;
  for ( Mesh * mesh : directScene.GetMeshes() )
  {
    if ( !mesh || ( 3 != mesh -> GetVertices().size() ) || ( 3 != mesh -> GetNormals().size() ) || ( 3 != mesh -> GetIndices().size() ) ) return false;
  }

  Scene overrideScene;
  bool overrideLoaded = false;
  {
    ScopedOutputSilencer outputSilencer(iQuiet);
    overrideLoaded = Loader::LoadScene(PathUtils::GetAssetPath("OBJTests/Override.scene"), overrideScene, settings);
  }
  if ( !overrideLoaded ) return false;
  const int overrideID = overrideScene.FindMaterialID("override");
  if ( ( overrideID < 0 ) || ( 2 != overrideScene.GetNbMeshInstances() ) ) return false;
  for ( const MeshInstance & instance : overrideScene.GetMeshInstances() )
  {
    if ( ( instance._MaterialID != overrideID ) || ( std::abs(instance._Transform[3][0] - 2.f) > .0001f ) ) return false;
  }
  return true;
}

bool CheckAnimatedImport( bool iQuiet )
{
  Scene scene;
  RenderSettings settings;
  bool loaded = false;
  {
    ScopedOutputSilencer outputSilencer(iQuiet);
    loaded = Loader::LoadScene(PathUtils::GetAssetPath("an-animated-cat/scene.gltf"), scene, settings);
  }
  if ( !loaded )
  {
    std::cerr << "Unable to load animated GLTF asset an-animated-cat/scene.gltf." << std::endl;
    return false;
  }

  if ( ( 1 != scene.GetAnimations().size() ) || ( "Take 001" != scene.GetAnimations()[0]._Name ) || ( scene.GetAnimations()[0]._Duration <= 0.f ) )
    return false;
  if ( ( 1 != scene.GetSkins().size() ) || ( 62 != scene.GetSkins()[0]._JointNodeIndices.size() ) )
    return false;
  if ( ( 3 != scene.GetNbMeshes() ) || !scene.HasSkinnedMeshes() )
    return false;

  for ( const Mesh * mesh : scene.GetMeshes() )
  {
    if ( !mesh || !mesh -> IsSkinned() || mesh -> GetVertices().empty() )
      return false;
  }

  {
    ScopedOutputSilencer outputSilencer(iQuiet);
    scene.CompileMeshData(Vec2i(0), false, true, false);
  }

  if ( !scene.EvaluateAnimationAtTime(0, 0.0f) )
    return false;
  const std::vector<Vec3> vertsAtT0 = scene.GetMeshes()[0] -> GetVertices();

  if ( !scene.EvaluateAnimationAtTime(0, 0.5f) )
    return false;
  const std::vector<Vec3> & vertsAtT05 = scene.GetMeshes()[0] -> GetVertices();
  if ( vertsAtT0.size() != vertsAtT05.size() )
    return false;

  float maxDelta = 0.0f;
  for ( size_t i = 0; i < vertsAtT0.size(); ++i )
    maxDelta = std::max(maxDelta, glm::length(vertsAtT05[i] - vertsAtT0[i]));
  if ( maxDelta <= 1e-4f )
    return false;

  if ( 0 != scene.RefitBLASData() )
    return false;

  if ( !scene.ResetAnimationsToInitialPose() )
    return false;

  return std::abs(scene.GetAnimationState()._CurrentTime) < 1e-6f;
}

}
}
}
