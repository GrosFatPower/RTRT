#ifndef _Test4_
#define _Test4_

#include "BaseTest.h"
#include "RenderSettings.h"
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
class Light;

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

  struct Varying
  {
    Vec3             _WorldPos;
    Vec2             _UV;
    Vec3             _Normal;
    Vec4             _Color;
  };

  struct Uniform
  {
    const Material * _Material       = nullptr;
    const Texture  * _Texture        = nullptr;
    const Light    * _Light          = nullptr;
    const Texture  * _SkyBox         = nullptr;
    float            _SkyBoxRotation = 0.f;
  };

  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void MousebuttonCallback(GLFWwindow * window, int button, int action, int mods);
  static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

  int UpdateCPUTime();
  int InitializeUI();
  int InitializeFrameBuffer();
  int InitializeSceneFiles();
  int InitializeScene();

  int RecompileShaders();
  int UpdateUniforms();
  int UpdateImage();
  int UpdateTextures();
  int UpdateScene();

  int RenderBackground( float iTop, float iRight );
  int RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP );

  void RenderToTexture();
  void RenderToSceen();
  void DrawUI();

  void ProcessInput();

  void ResizeImageBuffers();

  float EdgeFunction(const Vec3 & iV1, const Vec3 & iV2, const Vec3 & iV3);

  void VertexShader( const Vertex & iVertex, const Mat4x4 iMVP, Vec4 & oVertexPosition, Varying & oAttrib );

  void FragmentShader_Scene( const Vec4 & iFragCoord, const Varying & iAttrib, Vec4 & oColor );

  Vec4 SampleSkybox( const Vec3 & iDir );

  std::unique_ptr<QuadMesh> _Quad;
  std::unique_ptr<Scene>    _Scene;
  std::vector<std::string>  _SceneFiles;
  std::vector<const char*>  _SceneNames;
  unsigned int              _CurSceneId = 0;
  bool                      _ReloadScene = true;

  std::unique_ptr<ShaderProgram> _RTTShader;
  std::unique_ptr<ShaderProgram> _RTSShader;

  GLuint             _FrameBufferID   = 0;
  GLuint             _ImageTextureID  = 0;
  GLuint             _ScreenTextureID = 0;

  RenderSettings     _Settings;

  int                _ColorDepthOrNormalsBuffer = 0;
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


inline float Test4::EdgeFunction(const Vec3 & iV0, const Vec3 & iV1, const Vec3 & iV2) { 
  return (iV1.x - iV0.x) * (iV2.y - iV0.y) - (iV1.y - iV0.y) * (iV2.x - iV0.x); } // Counter-Clockwise edge function
//  return (iV2.x - iV0.x) * (iV1.y - iV0.y) - (iV2.y - iV0.y) * (iV1.x - iV0.x); } // Clockwise edge function

}

#endif /* _Test4_ */
