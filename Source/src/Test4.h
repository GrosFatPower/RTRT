#ifndef _Test4_
#define _Test4_

#include "BaseTest.h"
#include "RenderSettings.h"
#include "Light.h"
#include "MathUtil.h"
#include <vector>
#include <string>
#include <deque>
#include <memory>
#include <mutex>
#include <filesystem> // C++17
#include "GL/glew.h"

struct GLFWwindow;

namespace RTRT
{

class QuadMesh;
class ShaderProgram;
class Scene;
class Material;
class Texture;

class Test4 : public BaseTest
{
public:
  Test4( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight );
  virtual ~Test4();

  int Run();

  static const char * GetTestHeader();

public:

  struct Varying
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;

    Varying operator*(float t) const
    {
      auto copy = *this;
      copy._WorldPos *= t;
      copy._Normal   *= t;
      copy._UV       *= t;
      return copy;
    }

    Varying operator+(const Varying & iRhs) const
    {
      Varying copy = *this;

      copy._WorldPos += iRhs._WorldPos;
      copy._Normal   += iRhs._Normal;
      copy._UV       += iRhs._UV;

      return copy;
    }
  };

  struct Uniform
  {
    const std::vector<Material> * _Materials = nullptr;
    const std::vector<Texture*> * _Textures  = nullptr;
    std::vector<Light>            _Lights;
    Vec3                          _CameraPos;
    bool                          _BilinearSampling = true;
  };

  struct Vertex
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;

    bool operator==(const Vertex & iRhs) const
    {
      return ( ( _WorldPos == iRhs._WorldPos )
            && ( _Normal   == iRhs._Normal   )
            && ( _UV       == iRhs._UV       ) );
    }
  };

  struct Triangle
  {
    int   _Indices[3];
    Vec3  _Normal;
    int   _MatID;
  };

  struct ProjectedVertex
  {
    Vec4    _ProjPos;
    Varying _Attrib;
  };

  struct RasterTriangle
  {
    int        _Indices[3];
    Vec3       _V[3];
    float      _InvW[3];
    float      _InvArea;
    AABB<Vec2> _BBox;
    Vec3       _Normal;
    int        _MatID;
  };

  struct Fragment
  {
    Vec3    _FragCoords;
    int     _MatID;
    Varying _Attrib;
  };

private:

  struct KeyState
  {
    bool _KeyUp       = false;
    bool _KeyDown     = false;
    bool _KeyLeft     = false;
    bool _KeyRight    = false;
    bool _KeyLeftCTRL = false;
    bool _KeyEsc      = false;
  };

  struct MouseState
  {
    double _MouseX      = 0.;
    double _MouseY      = 0.;
    double _OldMouseX   = 0.;
    double _OldMouseY   = 0.;
    bool   _LeftClick   = false;
    bool   _RightClick  = false;
    bool   _MiddleClick = false;
  };

  struct FrameBuffer
  {
    std::vector<Vec4>  _ColorBuffer;
    std::vector<float> _DepthBuffer;
  };

  enum class ShadingType
  {
    Flat = 0,
    Phong
  };

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  int UpdateCPUTime();
  int InitializeUI();
  int InitializeFrameBuffer();
  int InitializeSceneFiles();
  int InitializeScene();
  int InitializeBackgroundFiles();
  int InitializeBackground();

  int RecompileShaders();
  int UpdateUniforms();
  int UpdateImage();
  int UpdateTextures();
  int UpdateScene();

  int RenderBackground( float iTop, float iRight );
  void RenderBackgroundRows( int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY );

  int RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP, const Mat4x4 & iRasterM );
  int ProcessVertices( const Mat4x4 & iMV, const Mat4x4 & iP );
  void ProcessVertices( const Mat4x4 & iMVP, int iStartInd, int iEndInd );
  int ClipTriangles( const Mat4x4 & iRasterM );
  void ClipTriangles( const Mat4x4 & iRasterM, int iThreadBin, int iStartInd, int iEndInd );
  int ProcessFragments();
  void ProcessFragments( int iStartY, int iEndY );

  void RenderToTexture();
  void RenderToSceen();
  bool RenderToFile( const std::filesystem::path & iFilepath );
  void DrawUI();

  void ProcessInput();

  void ResizeImageBuffers();

  Vec4 SampleSkybox( const Vec3 & iDir );

  static void VertexShader( const Vec4 & iVertexPos, const Vec2 & iUV, const Vec3 iNormal, const Mat4x4 iMVP, ProjectedVertex & oProjectedVertex );
  static void FragmentShader_Color( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Depth( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Normal( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Wires( const Fragment & iFrag, const Vec3 iVertCoord[3], Uniform & iUniforms, Vec4 & oColor );

  std::unique_ptr<QuadMesh> _Quad;
  std::unique_ptr<Scene>    _Scene;
  std::vector<std::string>  _SceneFiles;
  std::vector<const char*>  _SceneNames;
  std::vector<std::string>  _BackgroundFiles;
  std::vector<const char*>  _BackgroundNames;
  unsigned int              _CurSceneId = 0;
  unsigned int              _CurBackgroundId = 0;
  bool                      _ReloadScene = true;
  bool                      _ReloadBackground = true;
  bool                      _UpdateFrameBuffers = false;

  std::vector<Vertex>            _Vertices;
  std::vector<Triangle>          _Triangles;
  std::vector<ProjectedVertex>   _ProjVerticesBuf;
  std::mutex                     _ProjVerticesMutex;
  std::vector<RasterTriangle>  * _RasterTrianglesBuf = nullptr;
  int                          * _NbRasterTriPerBuf  = nullptr;

  std::unique_ptr<ShaderProgram> _RTTShader;
  std::unique_ptr<ShaderProgram> _RTSShader;

  GLuint             _FrameBufferID   = 0;
  GLuint             _ImageTextureID  = 0;
  GLuint             _ScreenTextureID = 0;

  RenderSettings     _Settings;

  FrameBuffer        _ImageBuffer;

  int                _NbThreadsMax              = 1;
  int                _NbThreads                 = 1;
  int                _ColorDepthOrNormalsBuffer = 0;
  bool               _ShowWires                 = false;
  bool               _BilinearSampling          = true;
  bool               _WBuffer                   = false;
  ShadingType        _ShadingType               = ShadingType::Phong;

  int                _SkyboxTexId = -1;

  KeyState           _KeyState;
  MouseState         _MouseState;

  // Frame rate
  double             _CPULoopTime          = 0.;
  double             _RenderImgElapsed     = 0.;
  double             _RenderBgdElapsed     = 0.;
  double             _RenderScnElapsed     = 0.;
  double             _RTTElapsed           = 0.;
  double             _RTSElapsed           = 0.;
  double             _FrameRate            = 0.;
  double             _TimeDelta            = 0.;
  double             _AverageDelta         = 0.;
  double             _AccuDelta            = 0.;
  int                _NbFrames             = 0;
};

}

#endif /* _Test4_ */
