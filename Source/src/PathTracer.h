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
  static const TextureSlot _RenderTarget           = 0;
  static const TextureSlot _RenderTargetLowRes     = 1;
  static const TextureSlot _RenderTargetTile       = 2;
  static const TextureSlot _Accumulate             = 3;
  static const TextureSlot _Denoised               = 4;
  static const TextureSlot _Vertices               = 5;
  static const TextureSlot _Normals                = 6;
  static const TextureSlot _UVs                    = 7;
  static const TextureSlot _VertInd                = 8;
  static const TextureSlot _TexInd                 = 9;
  static const TextureSlot _TexArray               = 10;
  static const TextureSlot _MeshBBox               = 11;
  static const TextureSlot _MeshIdRange            = 12;
  static const TextureSlot _Materials              = 13;
  static const TextureSlot _TLASNodes              = 14;
  static const TextureSlot _TLASTransformsID       = 15;
  static const TextureSlot _TLASMeshMatID          = 16;
  static const TextureSlot _BLASNodes              = 17;
  static const TextureSlot _BLASNodesRange         = 18;
  static const TextureSlot _BLASPackedIndices      = 19;
  static const TextureSlot _BLASPackedIndicesRange = 20;
  static const TextureSlot _BLASPackedVertices     = 21;
  static const TextureSlot _BLASPackedNormals      = 22;
  static const TextureSlot _BLASPackedUVs          = 23;
  static const TextureSlot _EnvMap                 = 24;
  static const TextureSlot _Temporary              = 25;
};

class PathTracer : public Renderer
{
public:
  PathTracer( Scene & iScene, RenderSettings & iSettings );
  virtual ~PathTracer();

  virtual int Initialize();
  virtual int Update();
  virtual int Done();

  virtual int RenderToTexture();
  virtual int Denoise();
  virtual int RenderToScreen();
  virtual int RenderToFile( const std::filesystem::path & iFilePath );

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

  float RenderScale()       const { return ( _Settings._RenderScale * 0.01f ); }
  float LowResRenderScale() const { return ( RenderScale() * _Settings._LowResRatio ); }
  int RenderWidth()         const { return _Settings._RenderResolution.x; }
  int RenderHeight()        const { return _Settings._RenderResolution.y; }

  bool LowResPass()         const { return ( Dirty() && !_Settings._AutoScale ); }
  int LowResRenderWidth()   const { return std::max(int( _Settings._RenderResolution.x * LowResRenderScale() ), 32); }
  int LowResRenderHeight()  const { return std::max(int( _Settings._RenderResolution.y * LowResRenderScale() ), 32); }

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
  GLFrameBuffer _RenderTargetFBO       = { 0, { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTarget } };
  GLFrameBuffer _RenderTargetLowResFBO = { 0, { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetLowRes } };
  GLFrameBuffer _RenderTargetTileFBO   = { 0, { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetTile } };
  GLFrameBuffer _AccumulateFBO         = { 0, { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Accumulate } };

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
  GLTexture _DenoisedTEX         = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Denoised         };
  GLTexture _TexArrayTEX         = { 0, GL_TEXTURE_2D_ARRAY, PathTracerTexSlot::_TexArray   };
  GLTexture _MaterialsTEX        = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Materials        };
  GLTexture _TLASTransformsIDTEX = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_TLASTransformsID };
  GLTexture _EnvMapTEX           = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_EnvMap           };

  // Shaders
  std::unique_ptr<ShaderProgram> _PathTraceShader;
  std::unique_ptr<ShaderProgram> _AccumulateShader;
  std::unique_ptr<ShaderProgram> _DenoiserShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Tiled rendering
  Vec2i        _CurTile;
  Vec2i        _NbTiles;
  unsigned int _NbCompleteFrames = 0;

  // Accumulate
  unsigned int _FrameNum          = 1;
  unsigned int _AccumulatedFrames = 0;

  // Scene data
  int _NbTriangles     = 0;
  int _NbMeshInstances = 0;
};

}

#endif /* _PathTracer_ */
