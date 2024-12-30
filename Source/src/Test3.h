#ifndef _Test3_
#define _Test3_

#include "BaseTest.h"
#include "RenderSettings.h"
#include <vector>
#include <string>
#include <deque>
#include <filesystem> // C++17
#include "GL/glew.h"

struct GLFWwindow;

namespace RTRT
{

class QuadMesh;
class ShaderProgram;
class Scene;

enum class TexType
{
  Render1 = 0,
  Render2,
  RenderLowRes,
  Skybox,
  Vertices,
  Normals,
  UVs,
  VertInd,
  TexInd,
  TexArray,
  MeshBBox,
  MeshIdRange,
  Materials,
  TLASNodes,
  TLASTransformsID,
  TLASMeshMatID,
  BLASNodes,
  BLASNodesRange,
  BLASPackedIndices,
  BLASPackedIndicesRange,
  BLASPackedVertices,
  BLASPackedNormals,
  BLASPackedUVs,
  Temporary,
  IMGUIMaterial
};

class Test3 : public BaseTest
{
public:
  Test3( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test3();

  int Run();

  static const char * GetTestHeader();

private:

  int InitializeSceneFiles();
  int InitializeBackgroundFiles();
  int InitializeScene();
  int UpdateScene();
  void ClearSceneData();

  int InitializeUI();
  int DrawUI();

  int InitializeFrameBuffers();
  int RecompileShaders();
  int UpdateUniforms();
  int ResizeTextures();

  int UpdateCPUTime();

  void AdjustRenderScale();

  float RenderScale()       const { return ( _Settings._RenderScale * 0.01f ); }
  float LowResRenderScale() const { return ( RenderScale() * _Settings._LowResRatio ); }
  int RenderWidth()         const { return int( _Settings._RenderResolution.x * RenderScale() ); }
  int RenderHeight()        const { return int( _Settings._RenderResolution.y * RenderScale() ); }
  int LowResRenderWidth()   const { return int( _Settings._RenderResolution.x * LowResRenderScale() ); }
  int LowResRenderHeight()  const { return int( _Settings._RenderResolution.y * LowResRenderScale() ); }

  void RenderToTexture();
  void RenderToSceen();
  bool RenderToFile( const std::filesystem::path & iFilepath );

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  QuadMesh      * _Quad      = nullptr;
  ShaderProgram * _RTTShader = nullptr;
  ShaderProgram * _RTSShader = nullptr;

  GLuint          _FrameBufferID                   = 0;
  GLuint          _FrameBufferLowResID             = 0;
  GLuint          _VtxBufferID                     = 0;
  GLuint          _VtxNormBufferID                 = 0;
  GLuint          _VtxUVBufferID                   = 0;
  GLuint          _VtxIndBufferID                  = 0;
  GLuint          _TexIndBufferID                  = 0;
  GLuint          _MeshBBoxBufferID                = 0;
  GLuint          _MeshIdRangeBufferID             = 0;
  // BVH                                          
  GLuint          _TLASNodesBufferID               = 0;
  GLuint          _TLASMeshMatIDBufferID           = 0;
  GLuint          _BLASNodesBufferID               = 0;
  GLuint          _BLASNodesRangeBufferID          = 0;
  GLuint          _BLASPackedIndicesBufferID       = 0;
  GLuint          _BLASPackedIndicesRangeBufferID  = 0;
  GLuint          _BLASPackedVerticesBufferID      = 0;
  GLuint          _BLASPackedNormalsBufferID       = 0;
  GLuint          _BLASPackedUVsBufferID           = 0;

  // Texture IDs
  GLuint          _RenderTexture1ID                = 0;
  GLuint          _RenderTexture2ID                = 0;
  GLuint          _RenderTextureLowResID           = 0;
  GLuint          _SkyboxTextureID                 = 0;
  GLuint          _VtxTextureID                    = 0;
  GLuint          _VtxNormTextureID                = 0;
  GLuint          _VtxUVTextureID                  = 0;
  GLuint          _VtxIndTextureID                 = 0;
  GLuint          _TexIndTextureID                 = 0;
  GLuint          _TexArrayTextureID               = 0;
  GLuint          _MeshBBoxTextureID               = 0;
  GLuint          _MeshIdRangeTextureID            = 0;
  GLuint          _MaterialsTextureID              = 0;
  // BVH
  GLuint          _TLASNodesTextureID              = 0;
  GLuint          _TLASMeshMatIDTextureID          = 0;
  GLuint          _TLASTransformsIDTextureID       = 0;
  GLuint          _BLASNodesTextureID              = 0;
  GLuint          _BLASNodesRangeTextureID         = 0;
  GLuint          _BLASPackedIndicesTextureID      = 0;
  GLuint          _BLASPackedIndicesRangeTextureID = 0;
  GLuint          _BLASPackedVerticesTextureID     = 0;
  GLuint          _BLASPackedNormalsTextureID      = 0;
  GLuint          _BLASPackedUVsTextureID          = 0;

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
  bool            _KeyR                 = false;

  Scene         * _Scene = nullptr;
  std::vector<std::string>  _SceneFiles;
  std::vector<const char*>  _SceneNames;
  std::vector<std::string>  _BackgroundFiles;
  std::vector<const char*>  _BackgroundNames;
  unsigned int              _CurSceneId = 0;
  unsigned int              _CurBackgroundId = 0;
  bool                      _ReloadScene = true;
  bool                      _ReloadBackground = true;
  int                       _SkyboxID    = -1;

  RenderSettings  _Settings;
  bool            _AccumulateFrames       = true;
  bool            _SceneCameraModified    = false;
  bool            _SceneLightsModified    = false;
  bool            _SceneMaterialsModified = false;
  bool            _SceneInstancesModified = false;
  bool            _RenderSettingsModified = false;
  bool            _Dirty                  = true;

  int                      _SelectedLight = -1;

  std::vector<std::string> _MaterialNames;
  int                      _SelectedMaterial = -1;
  std::vector<std::string> _PrimitiveNames;
  int                      _SelectedPrimitive = -1;
  int                      _NbTriangles = 0;
  int                      _NbMeshInstances = 0;

  // Frame rate
  long               _FrameNum             = 0;
  int                _AccumulatedFrames    = 0;
  double             _CPULoopTime          = 0.;
  double             _FrameRate            = 0.;
  double             _TimeDelta            = 0.;
  double             _AverageDelta         = 0.;
  double             _AccuDelta            = 0.;
  double             _LastAdjustmentTime   = 0.;
  int                _NbFrames             = 0;

  int                _DebugMode            = 0;
};

}

#endif /* _Test3_ */
