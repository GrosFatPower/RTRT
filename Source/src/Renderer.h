#ifndef _Renderer_
#define _Renderer_

namespace RTRT
{

class Scene;
class RenderSettings;

struct DirtyState
{
  bool _SceneCamera    = true;
  bool _SceneLights    = true;
  bool _SceneMaterials = true;
  bool _SceneInstances = true;
  bool _RenderSettings = true;
};

class Renderer
{
public:
  Renderer( Scene & iScene, RenderSettings & iSettings );
  virtual ~Renderer();

  virtual int Initialize() = 0;
  virtual int Update() = 0;
  virtual int Done() = 0;

  virtual int RenderToTexture() = 0;
  virtual int RenderToScreen() = 0;

  void HasModifiedSceneCamera   ( bool iState = true ) { _DirtyState._SceneCamera    = iState; };
  void HasModifiedSceneLights   ( bool iState = true ) { _DirtyState._SceneLights    = iState; };
  void HasModifiedSceneMaterials( bool iState = true ) { _DirtyState._SceneMaterials = iState; };
  void HasModifiedSceneInstances( bool iState = true ) { _DirtyState._SceneInstances = iState; };
  void HasModifiedRenderSettings( bool iState = true ) { _DirtyState._RenderSettings = iState; };

protected:

  bool Dirty() const { return ( _DirtyState._SceneCamera || _DirtyState._SceneLights || _DirtyState._SceneMaterials || _DirtyState._SceneInstances || _DirtyState._RenderSettings ); }
  void CleanStates() { _DirtyState._SceneCamera = _DirtyState._SceneLights = _DirtyState._SceneMaterials = _DirtyState._SceneInstances = _DirtyState._RenderSettings = false; }
  
  Scene          & _Scene;
  RenderSettings & _Settings;

  DirtyState       _DirtyState;
};

}

#endif /* _Renderer_ */
