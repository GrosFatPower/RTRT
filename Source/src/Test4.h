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

  enum class ShadingType
  {
    Flat = 0,
    Phong
  };

  struct Vertex
  {
    Vec4             _Position;
    Vec3             _Normal;
    Vec2             _UV;
    Vec4             _Color;
  };

  struct Triangle
  {
    Vertex           _Vert[3];
    Vec3             _Normal;
    const Material * _Material = nullptr;
    const Texture  * _Texture  = nullptr;
  };

  struct Varying
  {
    Vec3             _WorldPos;
    Vec2             _VertCoords[3];
    //Vec3             _W;
    Vec2             _UV;
    Vec3             _Normal;
    Vec4             _Color;
  };

  struct Uniform
  {
    const Material     * _Material       = nullptr;
    const Texture      * _Texture        = nullptr;
    const Texture      * _SkyBox         = nullptr;
    std::vector<Light>   _Lights;
    float                _SkyBoxRotation = 0.f;
  };

  struct BGThreadData
  {
    Test4             * _this;
    std::vector<Vec4> & _ColorBuffer;
    Vec3                _BottomLeft;
    Vec3                _DX;
    Vec3                _DY;
    int                 _Width;
    int                 _Height;
    int                 _StartY;
    int                 _EndY;

    BGThreadData(Test4             * iThis,
                 std::vector<Vec4> & iColorBuffer,
                 Vec3              & iBottomLeft,
                 Vec3              & iDX,
                 Vec3              & iDY,
                 int                 iWidth,
                 int                 iHeight,
                 int                 iStartY,
                 int                 iEndY)
    : _this(iThis)
    , _ColorBuffer(iColorBuffer)
    , _BottomLeft(iBottomLeft)
    , _DX(iDX), _DY(iDY)
    , _Width(iWidth), _Height(iHeight)
    , _StartY(iStartY), _EndY(iEndY){};
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
  static void RenderBackgroundRows( BGThreadData iTD );
  int RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP );

  void RenderToTexture();
  void RenderToSceen();
  void DrawUI();

  void ProcessInput();

  void ResizeImageBuffers();

  float EdgeFunction(const Vec3 & iV0, const Vec3 & iV1, const Vec3 & iV2) const;
  float EdgeFunction(const Vec2 & iV0, const Vec2 & iV1, const Vec2 & iV2) const;
  float EdgeFunction_Optim1(const float iA, const float iB, const float iC, const Vec3 & iV2) const ;
  float EdgeFunction_Optim1(const float iA, const float iB, const float iC, const Vec2 & iV2) const ;
  void EdgeFunction_PreComputeABC( const Vec3 & iV1, const Vec3 & iV2, float & oA, float &oB, float & oC ) const;
  void EdgeFunction_PreComputeABC( const Vec2 & iV1, const Vec2 & iV2, float & oA, float &oB, float & oC ) const;


  void VertexShader( const Vertex & iVertex, const Mat4x4 iMVP, Vec4 & oVertexPosition, Varying & oAttrib );

  void FragmentShader_Color( const Vec4 & iFragCoord, const Varying & iAttrib, Vec4 & oColor );

  void FragmentShader_Depth( const Vec4 & iFragCoord, const Varying & iAttrib, Vec4 & oColor );

  void FragmentShader_Normal( const Vec4 & iFragCoord, const Varying & iAttrib, Vec4 & oColor );

  float DistanceToSegment( const Vec2 iA, const Vec2 iB, const Vec2 iP );

  Vec4 SampleSkybox( const Vec3 & iDir );

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

  std::vector<Triangle>     _Triangles;

  std::unique_ptr<ShaderProgram> _RTTShader;
  std::unique_ptr<ShaderProgram> _RTSShader;

  GLuint             _FrameBufferID   = 0;
  GLuint             _ImageTextureID  = 0;
  GLuint             _ScreenTextureID = 0;

  RenderSettings     _Settings;

  int                _NbThreadsMax = 1;
  int                _NbThreads = 1;
  int                _ColorDepthOrNormalsBuffer = 0;
  bool               _ShowWires = false;
  bool               _BilinearSampling = true;
  ShadingType        _ShadingType = ShadingType::Phong;
  std::vector<Vec4>  _ColorBuffer;
  std::vector<float> _DepthBuffer;

  KeyState           _KeyState;
  MouseState         _MouseState;

  Uniform            _Uniforms;

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


inline float Test4::EdgeFunction(const Vec3 & iV0, const Vec3 & iV1, const Vec3 & iV2) const { 
  return (iV1.x - iV0.x) * (iV2.y - iV0.y) - (iV1.y - iV0.y) * (iV2.x - iV0.x); } // Counter-Clockwise edge function
//  return (iV2.x - iV0.x) * (iV1.y - iV0.y) - (iV2.y - iV0.y) * (iV1.x - iV0.x); } // Clockwise edge function

inline float Test4::EdgeFunction(const Vec2 & iV0, const Vec2 & iV1, const Vec2 & iV2) const { 
  return (iV1.x - iV0.x) * (iV2.y - iV0.y) - (iV1.y - iV0.y) * (iV2.x - iV0.x); } // Counter-Clockwise edge function
//  return (iV2.x - iV0.x) * (iV1.y - iV0.y) - (iV2.y - iV0.y) * (iV1.x - iV0.x); } // Clockwise edge function

inline float Test4::EdgeFunction_Optim1(const float iA, const float iB, const float iC, const Vec3 & iV2) const {
  return ( iA * iV2.x ) + ( iB * iV2.y ) + iC; } // A = (iV0.y - iV1.y), B = (iV1.x - iV0.x), C = ( iV0.x * iV1.y - iV0.y * iV1.x )

inline float Test4::EdgeFunction_Optim1(const float iA, const float iB, const float iC, const Vec2 & iV2) const {
  return ( iA * iV2.x ) + ( iB * iV2.y ) + iC; } // A = (iV0.y - iV1.y), B = (iV1.x - iV0.x), C = ( iV0.x * iV1.y - iV0.y * iV1.x )

inline void Test4::EdgeFunction_PreComputeABC( const Vec3 & iV0, const Vec3 & iV1, float & oA, float & oB, float & oC ) const { 
  oA = (iV0.y - iV1.y); oB = (iV1.x - iV0.x); oC = ( iV0.x * iV1.y ) - ( iV0.y * iV1.x ); }

inline void Test4::EdgeFunction_PreComputeABC( const Vec2 & iV0, const Vec2 & iV1, float & oA, float & oB, float & oC ) const { 
  oA = (iV0.y - iV1.y); oB = (iV1.x - iV0.x); oC = ( iV0.x * iV1.y ) - ( iV0.y * iV1.x ); }

}

#endif /* _Test4_ */
