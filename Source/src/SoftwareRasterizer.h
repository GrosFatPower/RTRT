#ifndef _SoftwareRasterizer_
#define _SoftwareRasterizer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"
#include "RGBA8.h"
#include "RasterData.h"
#include "SIMDUtils.h"

#include "GL/glew.h"

#include <memory>
#include <mutex>

namespace RTRT
{

struct RasterTexSlot
{
  static const TextureSlot _RenderTarget = 0;
  static const TextureSlot _ColorBuffer  = 1;
  static const TextureSlot _EnvMap       = 3;
  static const TextureSlot _Temporary    = 4;
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
  SoftwareRasterizer(Scene& iScene, RenderSettings& iSettings);
  virtual ~SoftwareRasterizer();

  virtual int Initialize() override;
  virtual int Update() override;
  virtual int Done() override;

  virtual int RenderToTexture() override;
  virtual int RenderToScreen() override;
  virtual int RenderToFile(const std::filesystem::path& iFilePath) override;

  virtual SoftwareRasterizer* AsSoftwareRasterizer() override { return this; }

  bool GetEnableSIMD() const { return _EnableSIMD; }
  void SetEnableSIMD(bool enabled) { _EnableSIMD = enabled; }

protected:

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int RecompileShaders();

  int UpdateNumberOfWorkers(bool iForce = false);

  int UnloadScene();
  int ReloadScene();
  int ReloadEnvMap();

  int UpdateTextures();
  int UpdateImageBuffer();

  int UpdateRenderToTextureUniforms();
  int UpdateRenderToScreenUniforms();
  int BindRenderToTextureTextures();
  int BindRenderToScreenTextures();

  float RenderScale()       const { return (_Settings._RenderScale * 0.01f); }
  int RenderWidth()         const { return _Settings._RenderResolution.x; }
  int RenderHeight()        const { return _Settings._RenderResolution.y; }

  void ResetTiles();
  void CopyTileToMainBuffer(const RasterData::Tile& iTile);
  void CopyTileToMainBuffer1x(const RasterData::Tile& iTile);
  bool TiledRendering()     const { return _Settings._TiledRendering; }
  int TileWidth()           const { return (_Settings._TileResolution.x > 0) ? (_Settings._TileResolution.x) : (64); }
  int TileHeight()          const { return (_Settings._TileResolution.y > 0) ? (_Settings._TileResolution.y) : (64); }
  Vec2i NbTiles()           const { return Vec2i(std::ceil(((float)RenderWidth()) / _Settings._TileResolution.x), std::ceil(((float)RenderHeight()) / _Settings._TileResolution.y)); }

  Vec4 SampleEnvMap(const Vec3& iDir);

  int RenderBackground(float iTop, float iRight);
  void RenderBackgroundRows(int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY);
  void RenderBackground(Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY, RasterData::Tile& ioTile);

  int RenderScene();
  int ProcessVertices();
  void ProcessVertices(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd);
  int ClipTriangles(const Mat4x4& iRasterM);
  void ClipTriangles(const Mat4x4& iRasterM, int iThreadBin, int iStartInd, int iEndInd);
  int ProcessFragments();
  void ProcessFragments(int iStartY, int iEndY);

  void BinTrianglesToTiles(unsigned int iBufferIndex);
  void ProcessFragments(RasterData::Tile& ioTile);

#ifdef SIMD_AVX2
  void CopyTileToMainBuffer8x(const RasterData::Tile& iTile);
  void ProcessVerticesAVX2(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd);
  static void VertexShaderAVX2(const Vec4& iVertexPos, const Vec2& iUV, const Vec3 iNormal, const __m256 iMVP[4], RasterData::ProjectedVertex& oProjectedVertex);
#endif

#ifdef SIMD_ARM_NEON
  void ProcessVerticesARM(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd);
  static void VertexShaderARM(const Vec4& iVertexPos, const Vec2& iUV, const Vec3 iNormal, const float32x4_t iMVP[4], RasterData::ProjectedVertex& oProjectedVertex);
#endif

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

  // SIMD
  bool _EnableSIMD = false;

  // Tile rendering
  int _TileCountX, _TileCountY;
  std::vector<RasterData::Tile> _Tiles;
  static constexpr int TILE_SIZE = 64;

  // Frame data
  unsigned int _FrameNum = 1;
  unsigned int _NbCompleteFrames = 0;
  RasterData::FrameBuffer _ImageBuffer;

  // Scene data
  std::vector<RasterData::Vertex>                      _VertexBuffer;
  std::vector<RasterData::Triangle>                    _Triangles;
  std::vector<RasterData::ProjectedVertex>             _ProjVerticesBuf;
  std::mutex                                           _ProjVerticesMutex;
  std::vector<std::vector<RasterData::RasterTriangle>> _RasterTrianglesBuf;
};

}

#endif /* _SoftwareRasterizer_ */
