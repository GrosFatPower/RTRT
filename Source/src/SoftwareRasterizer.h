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

#include <array>
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
class SoftwareFragmentShader;

struct SoftwareRasterizerStats
{
  unsigned long long _InputInstances = 0;
  unsigned long long _VisibleInstances = 0;
  unsigned long long _RejectedInstances = 0;
  unsigned long long _AvoidedVertices = 0;
  unsigned long long _AvoidedTriangles = 0;
  unsigned long long _ChangedInstances = 0;
  unsigned long long _TransformedVertices = 0;
  unsigned long long _RefreshedVertices = 0;
  unsigned long long _RefreshedTriangles = 0;
  unsigned long long _InputTriangles = 0;
  unsigned long long _ClippedTriangles = 0;
  unsigned long long _BinnedTriangles = 0;
  unsigned long long _DepthWinningPixels = 0;
  unsigned long long _ShadedPixels = 0;
  unsigned long long _CoveredPixels = 0;
  unsigned long long _TileJobs = 0;
  unsigned long long _CopiedBytes = 0;
  unsigned long long _HitBufferBytes = 0;
};

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
  virtual int ReadbackFinalColor( RenderImage & oImage ) override;
  virtual int GetRenderPassTimings( std::vector<RenderPassTiming> & oTimings ) const override;

  virtual SoftwareRasterizer* AsSoftwareRasterizer() override { return this; }

  bool GetEnableSIMD() const { return _EnableSIMD; }
  void SetEnableSIMD(bool enabled) { _EnableSIMD = enabled; }
  const char * GetSIMDMode() const;

  unsigned int GetTileSize() const { return _TileSize; }
  int SetTileSize( unsigned int iTileSize );
  const SoftwareRasterizerStats & GetStats() const { return _Stats; }
  bool GetEnableIncrementalRefresh() const { return _EnableIncrementalRefresh; }
  void SetEnableIncrementalRefresh( bool iEnabled ) { _EnableIncrementalRefresh = iEnabled; }
  bool GetEnableCompactHits() const { return _EnableCompactHits; }
  void SetEnableCompactHits( bool iEnabled ) { _EnableCompactHits = iEnabled; }
  bool GetEnableDirectColorWrites() const { return _EnableDirectColorWrites; }
  void SetEnableDirectColorWrites( bool iEnabled ) { _EnableDirectColorWrites = iEnabled; }
  bool GetEnableFrustumCulling() const { return _EnableFrustumCulling; }
  void SetEnableFrustumCulling( bool iEnabled ) { _EnableFrustumCulling = iEnabled; }
  bool GetEnablePBOUpload() const { return _EnablePBOUpload; }
  void SetEnablePBOUpload( bool iEnabled ) { _EnablePBOUpload = iEnabled; }

  void SetGenerateMipMaps(bool iGenerate);
  bool GetGenerateMipMaps() const { return _GenerateMipMaps; }

protected:

  struct CompiledInstanceRange;

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int InitializeStats();
  int UpdateStats();
  int RecompileShaders();

  int UpdateNumberOfWorkers(bool iForce = false);

  int UnloadScene();
  int ReloadScene();
  int RefreshSceneInstanceTransforms();
  int RefreshAllSceneInstanceTransforms();
  bool CanRefreshSceneInstanceTransforms() const;
  int ReloadEnvMap();

  int UpdateTextures();
  int UpdateImageBuffer();

  int UpdateRenderToTextureUniforms();
  int UpdateRenderToScreenUniforms();
  int BindRenderToTextureTextures();
  int BindRenderToScreenTextures();

  void ResizeTileMap();
  void ResetTiles();
  void CopyTileToMainBuffer(const RasterData::Tile& iTile);
  void CopyTileToMainBuffer1x(const RasterData::Tile& iTile);
  bool TiledRendering()     const { return _Settings._TiledRendering; }
  int TileWidth()           const { return (_Settings._TileResolution.x > 0) ? (_Settings._TileResolution.x) : (64); }
  int TileHeight()          const { return (_Settings._TileResolution.y > 0) ? (_Settings._TileResolution.y) : (64); }
  Vec2i NbTiles()           const { return Vec2i(std::ceil(((float)RenderWidth()) / _Settings._TileResolution.x), std::ceil(((float)RenderHeight()) / _Settings._TileResolution.y)); }

  Vec4 SampleEnvMap(const Vec3& iDir);

  void UpdateMipMaps();

  int RenderBackground(float iTop, float iRight);
  void RenderBackgroundRows(int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY);
  void RenderBackground(Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY, RasterData::Tile& ioTile);
  int RenderUncoveredBackground(float iTop, float iRight);

  int RenderScene();

  int ProcessVertices();
  void ProcessVertices(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd);

  int ClipTriangles(const Mat4x4& iRasterM);
  void ClipTriangles(const Mat4x4& iRasterM, int iThreadBin, int iStartInd, int iEndInd);

  int Rasterize();
  int Rasterize(int iThreadBin, int iStartY, int iEndY);
  int Rasterize(RasterData::Tile& ioTile);

  int ProcessFragments();
  void ProcessFragments(int iThreadBin, const RasterData::DefaultUniform & iUniforms);

  void BinTrianglesToTiles(unsigned int iBufferIndex);
  void ProcessFragments(RasterData::Tile& ioTile, const RasterData::DefaultUniform& iUniforms);

#ifdef SIMD_AVX2
  void CopyTileToMainBuffer8x(const RasterData::Tile& iTile);
  void ProcessVerticesAVX2(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd);
  static void VertexShaderAVX2(const Vec4& iVertexPos, const Vec2& iUV, const Vec3 iNormal, const __m256 iMVP[4], RasterData::ProjectedVertex& oProjectedVertex);
  int RasterizeAVX2(RasterData::Tile& ioTile);
#endif

#ifdef SIMD_ARM_NEON
  void CopyTileToMainBuffer4x(const RasterData::Tile& iTile);
  void ProcessVerticesARM(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd);
  static void VertexShaderARM(const Vec4& iVertexPos, const Vec2& iUV, const Vec3 iNormal, const float32x4_t iMVP[4], RasterData::ProjectedVertex& oProjectedVertex);
  int RasterizeARM(RasterData::Tile& ioTile);
#endif

  void ComputeLOD( RasterData::RasterTriangle & ioRasterTri );
  void UpdateInstanceBounds( CompiledInstanceRange & ioRange );
  bool IsInstanceVisible( const CompiledInstanceRange & iRange, const Mat4x4 & iViewProjection ) const;
  void BeginTimer( int iTimerID );
  void EndTimer( int iTimerID );
  double ReadTimer( int iTimerID );

protected:

  struct RasterSourceVertex
  {
    int _MeshInstanceID = -1;
    int _MeshID         = -1;
    int _VertexID       = -1;
    int _NormalID       = -1;
  };

  struct CompiledInstanceRange
  {
    int    _MeshID = -1;
    int    _MaterialID = -1;
    int    _VertexStart = 0;
    int    _VertexCount = 0;
    int    _TriangleStart = 0;
    int    _TriangleCount = 0;
    bool   _Visible = false;
    Mat4x4 _Transform = Mat4x4(1.f);
    AABB<Vec3> _WorldBounds;
  };

  QuadMesh _Quad;

  // Frame buffers
  GLFrameBuffer _RenderTargetFBO;

  // Textures
  GLTexture _RenderTargetTEX = { 0, GL_TEXTURE_2D, RasterTexSlot::_RenderTarget, GL_RGBA32F, GL_RGBA, GL_FLOAT };
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
  unsigned int _TileSize = 64;
  bool _EnableIncrementalRefresh = true;
  bool _EnableCompactHits = false;
  bool _EnableDirectColorWrites = false;
  bool _EnableFrustumCulling = true;
  bool _EnablePBOUpload = false;

  // Textures filtering
  bool _GenerateMipMaps = false;

  // Frame data
  unsigned int _FrameNum = 1;
  unsigned int _NbCompleteFrames = 0;
  RasterData::FrameBuffer _ImageBuffer;
  std::array<GLuint, 2> _UploadPBOs = { 0, 0 };
  std::array<GLsync, 2> _UploadFences = { nullptr, nullptr };
  size_t _UploadPBOSize = 0;
  unsigned int _UploadPBOIndex = 0;

  enum TimingID
  {
    TimingInstanceRefresh = 0,
    TimingFrameClear,
    TimingBackground,
    TimingUniformUpdate,
    TimingProcessVertices,
    TimingClipTriangles,
    TimingRasterize,
    TimingProcessFragments,
    TimingRenderScene,
    TimingColorUpload,
    TimingCopyToRenderTarget,
    TimingCompositeScreen,
    TimingCount
  };

  std::array<double, TimingCount> _PassTimes = {};
  std::array<bool, TimingCount>   _PassEnabled = {};
  std::array<bool, TimingCount>   _TimerWritten = {};
  std::array<std::array<GLuint, 2>, TimingCount> _TimerIDs = {};
  SoftwareRasterizerStats _Stats;

  // Scene data
  int                                                  _CachedMeshInstanceCount = 0;
  int                                                  _CachedVisibleMeshInstanceCount = 0;
  std::vector<RasterData::Vertex>                      _VertexBuffer;
  std::vector<RasterSourceVertex>                      _VertexSources;
  std::vector<RasterData::Triangle>                    _Triangles;
  std::vector<CompiledInstanceRange>                   _InstanceRanges;
  std::vector<int>                                     _VisibleInstanceRanges;
  std::vector<unsigned char>                           _TriangleVisible;
  std::vector<RasterData::ProjectedVertex>             _ProjVerticesBuf;
  std::mutex                                           _ProjVerticesMutex;
  std::vector<std::vector<RasterData::RasterTriangle>> _RasterTrianglesBuf;
  std::vector< std::vector<RasterData::Fragment>>      _Fragments;
};

}

#endif /* _SoftwareRasterizer_ */
