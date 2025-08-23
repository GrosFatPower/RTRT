#ifndef _SoftwareRasterizer_
#define _SoftwareRasterizer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"
#include "RGBA8.h"
#include "RasterData.h"

#include "GL/glew.h"

#include <memory>
#include <mutex>

namespace RTRT
{

struct RasterTexSlot
{
  static const TextureSlot _RenderTarget       = 0;
  static const TextureSlot _ColorBuffer        = 1;
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
struct Material;
class Texture;
struct Light;

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

  virtual SoftwareRasterizer * AsSoftwareRasterizer() { return this; }

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

  void ResetTiles();
  void CopyTileToMainBuffer( const RasterData::Tile & iTile );
  bool TiledRendering()     const { return _Settings._TiledRendering; }
  int TileWidth()           const { return ( _Settings._TileResolution.x > 0 ) ? ( _Settings._TileResolution.x ) : ( 64 ); }
  int TileHeight()          const { return ( _Settings._TileResolution.y > 0 ) ? ( _Settings._TileResolution.y ) : ( 64 ); }
  Vec2i NbTiles()           const { return Vec2i(std::ceil(((float)RenderWidth())/_Settings._TileResolution.x), std::ceil(((float)RenderHeight())/_Settings._TileResolution.y)); }

  Vec4 SampleEnvMap( const Vec3 & iDir );

  static void VertexShader( const Vec4 & iVertexPos, const Vec2 & iUV, const Vec3 iNormal, const Mat4x4 iMVP, RasterData::ProjectedVertex & oProjectedVertex );
  static void FragmentShader_Color( const RasterData::Fragment & iFrag, RasterData::Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Depth( const RasterData::Fragment & iFrag, RasterData::Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Normal( const RasterData::Fragment & iFrag, RasterData::Uniform & iUniforms, Vec4 & oColor );
  static void FragmentShader_Wires( const RasterData::Fragment & iFrag, const Vec3 iVertCoord[3], RasterData::Uniform & iUniforms, Vec4 & oColor );

  int RenderBackground( float iTop, float iRight );
  void RenderBackgroundRows( int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY );
  void RenderBackground( Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY, RasterData::Tile& ioTile );

  int RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP, const Mat4x4 & iRasterM );
  int ProcessVertices( const Mat4x4 & iMV, const Mat4x4 & iP );
  void ProcessVertices( const Mat4x4 & iMVP, int iStartInd, int iEndInd );
  int ClipTriangles( const Mat4x4 & iRasterM );
  void ClipTriangles( const Mat4x4 & iRasterM, int iThreadBin, int iStartInd, int iEndInd );
  int ProcessFragments();
  void ProcessFragments( int iStartY, int iEndY );

  void BinTrianglesToTiles();
  void BinTrianglesToTiles( unsigned int iBufferIndex );
  void ProcessFragments( RasterData::Tile & ioTile );

protected:

  QuadMesh _Quad;

  // Frame buffers
  GLFrameBuffer _RenderTargetFBO;

  // Textures
  GLTexture _ColorBufferTEX = { 0, GL_TEXTURE_2D, RasterTexSlot::_ColorBuffer, GL_RGBA8,  GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture _EnvMapTEX      = { 0, GL_TEXTURE_2D, RasterTexSlot::_EnvMap,      GL_RGB32F, GL_RGB,  GL_FLOAT         };

  // Shaders
  std::unique_ptr<ShaderProgram> _RenderToTextureShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Multi-threading
  unsigned int _NbJobs = 1;

  // Tile rendering
  int _TileCountX, _TileCountY;
  std::vector<RasterData::Tile> _Tiles;
  static constexpr int TILE_SIZE = 64;

  // Frame data
  unsigned int _FrameNum = 1;
  unsigned int _NbCompleteFrames = 0;
  RasterData::FrameBuffer _ImageBuffer;

  // Scene data
  std::vector<RasterData::Vertex>                      _Vertices;
  std::vector<RasterData::Triangle>                    _Triangles;
  std::vector<RasterData::ProjectedVertex>             _ProjVerticesBuf;
  std::mutex                                           _ProjVerticesMutex;
  std::vector<std::vector<RasterData::RasterTriangle>> _RasterTrianglesBuf;
};

}

#endif /* _SoftwareRasterizer_ */
