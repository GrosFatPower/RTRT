#ifndef _Renderer_
#define _Renderer_

namespace RTRT
{

class Scene;
class RenderSettings;

enum class DirtyState
{
  Clean          = 0x00,
  SceneCamera    = 0x01,
  SceneLights    = 0x02,
  SceneMaterials = 0x04,
  SceneInstances = 0x08,
  SceneEnvMap    = 0x10,
  RenderSettings = 0x20
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

  void Notify( DirtyState iState ) { _DirtyStates |= (unsigned long)iState; };

  void SetDebugMode( const int iDebugMode );

protected:

  bool Dirty() const { return ( _DirtyStates != (unsigned long)DirtyState::Clean ); }
  void CleanStates() { _DirtyStates = (unsigned long)DirtyState::Clean; }
  
  Scene          & _Scene;
  RenderSettings & _Settings;

  unsigned long    _DirtyStates = 0xFF;

  int              _DebugMode = 0;
};

}

#endif /* _Renderer_ */
