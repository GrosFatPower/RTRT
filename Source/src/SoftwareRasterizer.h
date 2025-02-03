#ifndef _SoftwareRasterizer_
#define _SoftwareRasterizer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"
#include "RGBA8.h"

#include "GL/glew.h"

#include <memory>
#include <mutex>

namespace RTRT
{

struct RasterTexSlot
{
  static const TextureSlot _RenderTarget       = 0;
  static const TextureSlot _ColorBuffer        = 1;
  //static const TextureSlot _RenderTargetTile   = 2;
  static const TextureSlot _EnvMap             = 3;
  static const TextureSlot _Temporary          = 4;
};

enum class RasterDebugModes
{
  ColorBuffer = 0x00,
  DepthBuffer = 0x01,
  Normals     = 0x02,
  Wires       = 0x04
};


class ShaderProgram;
class Scene;
class Material;
class Texture;
class Light;

class SoftwareRasterizer : public Renderer
{
public:
  SoftwareRasterizer( Scene & iScene, RenderSettings & iSettings );
  virtual ~SoftwareRasterizer();

  virtual int Initialize();
  virtual int Update();
  virtual int Done();

  virtual int RenderToTexture();
  virtual int RenderToScreen();
  virtual int RenderToFile( const std::filesystem::path & iFilePath );

public:

  struct FrameBuffer
  {
    std::vector<RGBA8> _ColorBuffer;
    std::vector<float> _DepthBuffer;
  };

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

  enum class ShadingType
  {
    Flat = 0,
    Phong
  };

protected:

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int RecompileShaders();

  int UpdateNumberOfWorkers( bool iForce = false );

  int UnloadScene();
  int ReloadScene();
  int ReloadEnvMap();

  int UpdateTextures();
  int UpdateImageBuffer();

  int UpdateRenderToTextureUniforms();
  int UpdateRenderToScreenUniforms();
  int BindRenderToTextureTextures();
  int BindRenderToScreenTextures();

  float RenderScale()       const { return ( _Settings._RenderScale * 0.01f ); }
  int RenderWidth()         const { return _Settings._RenderResolution.x; }
  int RenderHeight()        const { return _Settings._RenderResolution.y; }

  bool TiledRendering()     const { return _Settings._TiledRendering; }
  int TileWidth()           const { return ( _Settings._TileResolution.x > 0 ) ? ( _Settings._TileResolution.x ) : ( 64 ); }
  int TileHeight()          const { return ( _Settings._TileResolution.y > 0 ) ? ( _Settings._TileResolution.y ) : ( 64 ); }
  Vec2i NbTiles()           const { return Vec2i(std::ceil(((float)RenderWidth())/_Settings._TileResolution.x), std::ceil(((float)RenderHeight())/_Settings._TileResolution.y)); }
  void  NextTile();
  void  ResetTiles();

  Vec4 SampleEnvMap( const Vec3 & iDir );

  static void VertexShader( const Vec4 & iVertexPos, const Vec2 & iUV, const Vec3 iNormal, const Mat4x4 iMVP, ProjectedVertex & oProjectedVertex );
  static void FragmentShader_Color( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Depth( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Normal( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Wires( const Fragment & iFrag, const Vec3 iVertCoord[3], Uniform & iUniforms, Vec4 & oColor );

  int RenderBackground( float iTop, float iRight );
  void RenderBackgroundRows( int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY );

  int RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP, const Mat4x4 & iRasterM );
  int ProcessVertices( const Mat4x4 & iMV, const Mat4x4 & iP );
  void ProcessVertices( const Mat4x4 & iMVP, int iStartInd, int iEndInd );
  int ClipTriangles( const Mat4x4 & iRasterM );
  void ClipTriangles( const Mat4x4 & iRasterM, int iThreadBin, int iStartInd, int iEndInd );
  int ProcessFragments();
  void ProcessFragments( int iStartY, int iEndY );

protected:

  QuadMesh _Quad;

  // Frame buffers
  GLFrameBuffer _RenderTargetFBO = { 0, { 0, GL_TEXTURE_2D, RasterTexSlot::_RenderTarget } };

  // Textures
  GLTexture _ColorBufferTEX = { 0, GL_TEXTURE_2D, RasterTexSlot::_ColorBuffer };
  GLTexture _EnvMapTEX      = { 0, GL_TEXTURE_2D, RasterTexSlot::_EnvMap      };

  // Shaders
  std::unique_ptr<ShaderProgram> _RenderToTextureShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Tiled rendering
  Vec2i        _CurTile;
  Vec2i        _NbTiles;
  unsigned int _NbCompleteFrames = 0;

  unsigned int _FrameNum = 1;

  // Frame data
  FrameBuffer _ImageBuffer;

  unsigned int _NbJobs                    = 1;
  int          _ColorDepthOrNormalsBuffer = 0;
  bool         _ShowWires                 = false;
  bool         _BilinearSampling          = true;
  bool         _WBuffer                   = false;
  ShadingType  _ShadingType               = ShadingType::Phong;

  // Scene data
  std::vector<Vertex>            _Vertices;
  std::vector<Triangle>          _Triangles;
  std::vector<ProjectedVertex>   _ProjVerticesBuf;
  std::mutex                     _ProjVerticesMutex;
  std::vector<RasterTriangle>  * _RasterTrianglesBuf = nullptr;
  int                          * _NbRasterTriPerBuf  = nullptr;
};

}

#endif /* _SoftwareRasterizer_ */
