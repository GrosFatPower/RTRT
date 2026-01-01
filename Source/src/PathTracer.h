#ifndef _PathTracer_
#define _PathTracer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"

#include "GL/glew.h"

#include <memory>

namespace RTRT
{

class Scene;

struct PathTracerTexSlot
{
  static const TextureSlot _RenderTarget            = 0;
  static const TextureSlot _RenderTargetNormals     = 1;
  static const TextureSlot _RenderTargetPos         = 2;
  static const TextureSlot _RenderTargetLowRes      = 3;
  static const TextureSlot _RenderTargetTile        = 4;
  static const TextureSlot _Accumulate              = 5;
  static const TextureSlot _AccumulateNormals       = 6;
  static const TextureSlot _AccumulatePos           = 7;
  static const TextureSlot _Denoised                = 8;
  static const TextureSlot _Vertices                = 9;
  static const TextureSlot _Normals                 = 10;
  static const TextureSlot _UVs                     = 11;
  static const TextureSlot _VertInd                 = 12;
  static const TextureSlot _TexInd                  = 13;
  static const TextureSlot _TexArray                = 14;
  static const TextureSlot _MeshBBox                = 15;
  static const TextureSlot _MeshIdRange             = 16;
  static const TextureSlot _Materials               = 17;
  static const TextureSlot _TLASNodes               = 18;
  static const TextureSlot _TLASTransformsID        = 19;
  static const TextureSlot _TLASMeshMatID           = 20;
  static const TextureSlot _BLASNodes               = 21;
  static const TextureSlot _BLASNodesRange          = 22;
  static const TextureSlot _BLASPackedIndices       = 23;
  static const TextureSlot _BLASPackedIndicesRange  = 24;
  static const TextureSlot _BLASPackedVertices      = 25;
  static const TextureSlot _BLASPackedNormals       = 26;
  static const TextureSlot _BLASPackedUVs           = 27;
  static const TextureSlot _EnvMap                  = 28;
  static const TextureSlot _EnvMapCDF               = 29;
  static const TextureSlot _Temporary               = 31;
};

class PathTracer : public Renderer
{
public:
  PathTracer( Scene & iScene, RenderSettings & iSettings );
  virtual ~PathTracer();

  virtual int Initialize() override;
  virtual int Update() override;
  virtual int Done() override;

  virtual int RenderToTexture() override;
  virtual int DenoiseOutput();
  virtual int RenderToScreen() override;
  virtual int RenderToFile( const std::filesystem::path & iFilePath ) override;

  unsigned int GetNbCompleteFrames()  const { return _NbCompleteFrames; }
  unsigned int GetFrameNum()          const { return _FrameNum; }
  double GetPathTraceTime()           const { return _PathTraceTime; }
  double GetAccumulateTime()          const { return _AccumulateTime; }
  double GetDenoiseTime()             const { return _DenoiseTime; }
  double GetRenderToScreenTime()      const { return _RenderToScreenTime; }

  virtual PathTracer * AsPathTracer() override { return this; }

protected:

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int RecompileShaders();

  int UnloadScene();
  int ReloadScene();
  int ReloadEnvMap();

  int UpdatePathTraceUniforms();
  int UpdateAccumulateUniforms();
  int UpdateDenoiserUniforms();
  int UpdateRenderToScreenUniforms();

  int BindPathTraceTextures();
  int BindAccumulateTextures();
  int BindDenoiserTextures();
  int BindRenderToScreenTextures();

  int InitializeStats();
  int UpdateStats();

  float LowResRenderScale() const { return ( RenderScale() * _Settings._LowResRatio ); }

  bool LowResPass()         const { return ( Dirty() && !_Settings._AutoScale ); }
  int LowResRenderWidth()   const { return std::max(int( _Settings._RenderResolution.x * LowResRenderScale() ), 32); }
  int LowResRenderHeight()  const { return std::max(int( _Settings._RenderResolution.y * LowResRenderScale() ), 32); }

  bool Denoise()            const { return (_Settings._Denoise && !LowResPass());}

  bool TiledRendering()     const { return _Settings._TiledRendering; }
  int TileWidth()           const { return ( _Settings._TileResolution.x > 0 ) ? ( _Settings._TileResolution.x ) : ( 64 ); }
  int TileHeight()          const { return ( _Settings._TileResolution.y > 0 ) ? ( _Settings._TileResolution.y ) : ( 64 ); }
  Vec2i NbTiles()           const { return Vec2i(std::ceil(((float)RenderWidth())/_Settings._TileResolution.x), std::ceil(((float)RenderHeight())/_Settings._TileResolution.y)); }
  Vec2  TileOffset()        const { return Vec2(_CurTile.x * InvNbTiles().x, _CurTile.y * InvNbTiles().y); }
  Vec2  InvNbTiles()        const { return Vec2(((float)_Settings._TileResolution.x)/RenderWidth(), ((float)_Settings._TileResolution.y)/RenderHeight()); }
  void  NextTile();
  void  ResetTiles();

protected:

  QuadMesh _Quad;

  // Frame buffers
  GLFrameBuffer _RenderTargetFBO;
  GLFrameBuffer _RenderTargetLowResFBO;
  GLFrameBuffer _RenderTargetTileFBO;
  GLFrameBuffer _AccumulateFBO;

  // Texture buffers
  GLTextureBuffer _VtxTBO                     = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_Vertices               } };
  GLTextureBuffer _VtxNormTBO                 = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_Normals                } };
  GLTextureBuffer _VtxUVTBO                   = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_UVs                    } };
  GLTextureBuffer _VtxIndTBO                  = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_VertInd                } };
  GLTextureBuffer _TexIndTBO                  = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_TexInd                 } };
  GLTextureBuffer _MeshBBoxTBO                = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_MeshBBox               } };
  GLTextureBuffer _MeshIdRangeTBO             = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_MeshIdRange            } };
  GLTextureBuffer _TLASNodesTBO               = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_TLASNodes              } };
  GLTextureBuffer _TLASMeshMatIDTBO           = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_TLASMeshMatID          } };
  GLTextureBuffer _BLASNodesTBO               = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASNodes              } };
  GLTextureBuffer _BLASNodesRangeTBO          = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASNodesRange         } };
  GLTextureBuffer _BLASPackedIndicesTBO       = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASPackedIndices      } };
  GLTextureBuffer _BLASPackedIndicesRangeTBO  = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASPackedIndicesRange } };
  GLTextureBuffer _BLASPackedVerticesTBO      = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASPackedVertices     } };
  GLTextureBuffer _BLASPackedNormalsTBO       = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASPackedNormals      } };
  GLTextureBuffer _BLASPackedUVsTBO           = { 0, { 0, GL_TEXTURE_BUFFER, PathTracerTexSlot::_BLASPackedUVs          } };

  // Textures
  GLTexture _DenoisedTEX         = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Denoised,         GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture _TexArrayTEX         = { 0, GL_TEXTURE_2D_ARRAY, PathTracerTexSlot::_TexArray,   GL_RGBA8,   GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture _MaterialsTEX        = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Materials,        GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture _TLASTransformsIDTEX = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_TLASTransformsID, GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture _EnvMapTEX           = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_EnvMap,           GL_RGB32F,  GL_RGB,  GL_FLOAT };
  GLTexture _EnvMapCDFTEX        = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_EnvMapCDF,        GL_R32F,    GL_RED,  GL_FLOAT };

  // Shaders
  std::unique_ptr<ShaderProgram> _PathTraceShader;
  std::unique_ptr<ShaderProgram> _AccumulateShader;
  std::unique_ptr<ShaderProgram> _DenoiserShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Tiled rendering
  Vec2i        _CurTile;
  Vec2i        _NbTiles;

  // Accumulate
  unsigned int _FrameNum          = 1;
  unsigned int _NbCompleteFrames  = 0;

  // Scene data
  int _NbTriangles     = 0;
  int _NbMeshInstances = 0;

  // Stats
  double _PathTraceTime      = 0.;
  double _AccumulateTime     = 0.;
  double _DenoiseTime        = 0.;
  double _RenderToScreenTime = 0.;

  GLuint _PathTraceTimeId[2]      = { 0, 0 };
  GLuint _AccumulateTimeId[2]     = { 0, 0 };
  GLuint _DenoiseTimeId[2]        = { 0, 0 };
  GLuint _RenderToScreenTimeId[2] = { 0, 0 };

};

}

#endif /* _PathTracer_ */
