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
    Vec3 _Color;
  };

  struct Vertex
  {
    Vec3 _WorldPos;
    Vec2 _UV;
    Vec3 _Normal;
    Vec3 _Color;

    bool operator==(const Vertex & iRhs) const
    {
      return ( ( _WorldPos == iRhs._WorldPos )
            && ( _Normal   == iRhs._Normal   )
            && ( _UV       == iRhs._UV       )
            && ( _Color    == iRhs._Color    ) );
    }

    Vertex operator*(float t) const
    {
      auto copy = *this;

      copy._WorldPos *= t;
      copy._Normal   *= t;
      copy._UV       *= t;
      copy._Color    *= t;

      return copy;
    }

    Vertex operator+(const Vertex & iRhs) const
    {
      auto copy = *this;

      copy._WorldPos *= iRhs._WorldPos;
      copy._Normal   *= iRhs._Normal;
      copy._UV       *= iRhs._UV;
      copy._Color    *= iRhs._Color;

      return copy;
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
    Vec3  _HomogeneousProjPos[3];
    Vec2  _V[3];
    float _InvW[3];
    float _InvArea;
    Vec2  _BBoxMin, _BBoxMax;
    Vec3  _Normal;
    int   _MatID;
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

  int RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP );
  int ProcessVertices( const Mat4x4 & iMV, const Mat4x4 & iP );
  void ProcessVertices( const Mat4x4 & iMVP, int iStartInd, int iEndInd );
  int ClipTriangles();
  void ClipTriangles( int iThreadBin, int iStartInd, int iEndInd );

  void RenderToTexture();
  void RenderToSceen();
  void DrawUI();

  void ProcessInput();

  void ResizeImageBuffers();

  Vec4 SampleSkybox( const Vec3 & iDir );

  static void VertexShader( const Vec4 & iVertexPos, const Vec2 & iUV, const Vec3 iNormal, const Vec3 iColor, const Mat4x4 iMVP, ProjectedVertex & oProjectedVertex );

  static float EdgeFunction(const Vec2 & iV0, const Vec2 & iV1, const Vec2 & iV2);

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
  std::vector<ProjectedVertex>   _ProjVertices;
  std::vector<RasterTriangle>  * _RasterTriangles = nullptr;

  std::unique_ptr<ShaderProgram> _RTTShader;
  std::unique_ptr<ShaderProgram> _RTSShader;

  GLuint             _FrameBufferID   = 0;
  GLuint             _ImageTextureID  = 0;
  GLuint             _ScreenTextureID = 0;

  RenderSettings     _Settings;

  FrameBuffer        _ImageBuffer;

  int                _NbThreadsMax = 1;
  int                _NbThreads = 1;
  int                _ColorDepthOrNormalsBuffer = 0;
  bool               _ShowWires = false;
  bool               _BilinearSampling = true;
  ShadingType        _ShadingType = ShadingType::Phong;

  int                _SkyboxTexId = -1;

  KeyState           _KeyState;
  MouseState         _MouseState;

  // Frame rate
  double             _CPULoopTime          = 0.f;
  double             _RenderImgElapsed     = 0.f;
  double             _RenderBgdElapsed     = 0.f;
  double             _RenderScnElapsed     = 0.f;
  double             _RTTElapsed           = 0.f;
  double             _RTSElapsed           = 0.f;
  double             _FrameRate            = 0.f;
  double             _TimeDelta            = 0.f;
  double             _AverageDelta         = 0.f;
  std::deque<double> _LastDeltas;
};

inline float Test4::EdgeFunction(const Vec2 & iV0, const Vec2 & iV1, const Vec2 & iV2) { 
  return (iV1.x - iV0.x) * (iV2.y - iV0.y) - (iV1.y - iV0.y) * (iV2.x - iV0.x); } // Counter-Clockwise edge function
//  return (iV2.x - iV0.x) * (iV1.y - iV0.y) - (iV2.y - iV0.y) * (iV1.x - iV0.x); } // Clockwise edge function

}

#endif /* _Test4_ */
