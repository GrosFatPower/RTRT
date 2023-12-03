#ifndef _Test3_
#define _Test3_

#include "BaseTest.h"
#include "RenderSettings.h"
#include <vector>
#include <string>
#include <deque>
#include "GL/glew.h"

struct GLFWwindow;

namespace RTRT
{

class QuadMesh;
class ShaderProgram;
class Scene;


class Test3 : public BaseTest
{
public:
  Test3( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test3();

  int Run();

  static const char * GetTestHeader();

private:

  int InitializeSceneFiles();
  int InitializeScene();
  int UpdateScene();
  void ClearSceneData();

  int InitializeUI();
  int DrawUI();

  int InitializeFrameBuffer();
  int RecompileShaders();
  int UpdateUniforms();

  int UpdateCPUTime();

  void AdjustRenderScale();

  float RenderScale() const { return ( _Settings._RenderScale * 0.01f ); }
  int RenderWidth()   const { return int( _Settings._RenderResolution.x * RenderScale() ); }
  int RenderHeight()  const { return int( _Settings._RenderResolution.y * RenderScale() ); }

  void RenderToTexture();
  void RenderToSceen();

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  QuadMesh      * _Quad      = nullptr;
  ShaderProgram * _RTTShader = nullptr;
  ShaderProgram * _RTSShader = nullptr;

  GLuint          _FrameBufferID                    = 0;
  GLuint          _VtxBufferID                      = 0;
  GLuint          _VtxNormBufferID                  = 0;
  GLuint          _VtxUVBufferID                    = 0;
  GLuint          _VtxIndBufferID                   = 0;
  GLuint          _TexIndBufferID                   = 0;
  GLuint          _MeshBBoxBufferID                 = 0;
  GLuint          _MeshIdRangeBufferID              = 0;
  // BVH
  GLuint          _TLASNodesBufferID                = 0;
  GLuint          _TLASMeshMatIDBufferID            = 0;
  GLuint          _BLASNodesBufferID                = 0;
  GLuint          _BLASNodesRangeBufferID           = 0;
  GLuint          _BLASPackedIndicesBufferID        = 0;
  GLuint          _BLASPackedIndicesRangeBufferID   = 0;
  GLuint          _BLASPackedVerticesBufferID       = 0;
  GLuint          _BLASPackedNormalsBufferID        = 0;
  GLuint          _BLASPackedUVsBufferID            = 0;

  GLuint          _ScreenTextureID                  = 0;
  GLuint          _SkyboxTextureID                  = 0;
  GLuint          _VtxTextureID                     = 0;
  GLuint          _VtxNormTextureID                 = 0;
  GLuint          _VtxUVTextureID                   = 0;
  GLuint          _VtxIndTextureID                  = 0;
  GLuint          _TexIndTextureID                  = 0;
  GLuint          _TexArrayTextureID                = 0;
  GLuint          _MeshBBoxTextureID                = 0;
  GLuint          _MeshIdRangeTextureID             = 0;
  // BVH
  GLuint          _TLASNodesTextureID               = 0;
  GLuint          _TLASMeshMatIDTextureID           = 0;
  GLuint          _TLASTransformsIDTextureID        = 0;
  GLuint          _BLASNodesTextureID               = 0;
  GLuint          _BLASNodesRangeTextureID          = 0;
  GLuint          _BLASPackedIndicesTextureID       = 0;
  GLuint          _BLASPackedIndicesRangeTextureID  = 0;
  GLuint          _BLASPackedVerticesTextureID      = 0;
  GLuint          _BLASPackedNormalsTextureID       = 0;
  GLuint          _BLASPackedUVsTextureID           = 0;

  long            _FrameNum             = 0;
  int             _AccumulatedFrames    = 0;
  float           _CPULoopTime          = 0.f;
  float           _FrameRate            = 0.f;
  float           _TimeDelta            = 0.f;
  float           _AverageDelta         = 0.f;
  std::deque<float> _LastDeltas;

  double          _MouseX               = 0.;
  double          _MouseY               = 0.;
  double          _OldMouseX            = 0.;
  double          _OldMouseY            = 0.;
  bool            _LeftClick            = false;
  bool            _RightClick           = false;
  bool            _MiddleClick          = false;

  bool            _KeyUp                = false;
  bool            _KeyDown              = false;
  bool            _KeyLeft              = false;
  bool            _KeyRight             = false;
  bool            _KeyEsc               = false;

  Scene         * _Scene = nullptr;
  std::vector<std::string>  _SceneFiles;
  std::vector<const char*>  _SceneNames;
  unsigned int              _CurSceneId = 0;
  bool                      _ReloadScene = true;

  RenderSettings  _Settings;
  bool            _SceneCameraModified    = false;
  bool            _SceneLightsModified    = false;
  bool            _SceneMaterialsModified = false;
  bool            _SceneInstancesModified = false;
  bool            _RenderSettingsModified = false;

  std::vector<std::string> _MaterialNames;
  int                      _SelectedMaterial = -1;
  std::vector<std::string> _PrimitiveNames;
  int                      _SelectedPrimitive = -1;
  int                      _NbTriangles = 0;
  int                      _NbMeshInstances = 0;
};

}

#endif /* _Test3_ */
