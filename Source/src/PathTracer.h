#ifndef _PathTracer_
#define _PathTracer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"

#include "GL/glew.h"

namespace RTRT
{

class Scene;

class PathTracer : public Renderer
{
public:
  PathTracer( Scene & iScene, RenderSettings & iSettings );
  virtual ~PathTracer();

  virtual int RenderToTexture();
  virtual int RenderToScreen();

public:

  enum class TextureSlot
  {
    RenderTarget = 0,
    RenderTargetLowRes,
    RenderTargetTile,
    Accumulate,
    Vertices,
    Normals,
    UVs,
    VertInd,
    TexInd,
    TexArray,
    MeshBBox,
    MeshIdRange,
    Materials,
    TLASNodes,
    TLASTransformsID,
    TLASMeshMatID,
    BLASNodes,
    BLASNodesRange,
    BLASPackedIndices,
    BLASPackedIndicesRange,
    BLASPackedVertices,
    BLASPackedNormals,
    BLASPackedUVs,
    EnvMap
  };

  struct GLTexture
  {
    GLuint            _ID;
    const TextureSlot _Slot;
  };

  struct GLFrameBuffer
  {
    GLuint    _ID;
    GLTexture _Tex;
  };

  struct GLTextureBuffer
  {
    GLuint    _ID;
    GLTexture _Tex;
  };

  void DeleteTEX( GLTexture & ioTEX );
  void DeleteFBO( GLFrameBuffer & ioFBO );
  void DeleteTBO( GLTextureBuffer & ioTBO );

protected:

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int RecompileShaders();

  int UnloadScene();
  int ReloadScene();

  int UpdatePathTraceUniforms();
  int UpdateAccumulateUniforms();
  int UpdateRenderToScreenUniforms();

  int BindPathTraceTextures();
  int BindAccumulateTextures();
  int BindRenderToScreenTextures();

  float RenderScale()       const { return ( _Settings._RenderScale * 0.01f ); }
  float LowResRenderScale() const { return ( RenderScale() * _Settings._LowResRatio ); }
  int RenderWidth()         const { return _Settings._RenderResolution.x; }
  int RenderHeight()        const { return _Settings._RenderResolution.y; }

  bool Dirty()              const { return ( false ); } // ToDo
  bool LowResPass()         const { return ( false ); } // ToDo
  int LowResRenderWidth()   const { return int( _Settings._RenderResolution.x * LowResRenderScale() ); }
  int LowResRenderHeight()  const { return int( _Settings._RenderResolution.y * LowResRenderScale() ); }

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
  GLFrameBuffer _RenderTargetFBO       = { 0, { 0, TextureSlot::RenderTarget } };
  GLFrameBuffer _RenderTargetLowResFBO = { 0, { 0, TextureSlot::RenderTargetLowRes } };
  GLFrameBuffer _RenderTargetTileFBO   = { 0, { 0, TextureSlot::RenderTargetTile } };
  GLFrameBuffer _AccumulateFBO         = { 0, { 0, TextureSlot::Accumulate } };

  // Texture buffers
  GLTextureBuffer _VtxTBO                     = { 0, { 0, TextureSlot::Vertices               } };
  GLTextureBuffer _VtxNormTBO                 = { 0, { 0, TextureSlot::Normals                } };
  GLTextureBuffer _VtxUVTBO                   = { 0, { 0, TextureSlot::UVs                    } };
  GLTextureBuffer _VtxIndTBO                  = { 0, { 0, TextureSlot::VertInd                } };
  GLTextureBuffer _TexIndTBO                  = { 0, { 0, TextureSlot::TexInd                 } };
  GLTextureBuffer _MeshBBoxTBO                = { 0, { 0, TextureSlot::MeshBBox               } };
  GLTextureBuffer _MeshIdRangeTBO             = { 0, { 0, TextureSlot::MeshIdRange            } };
  GLTextureBuffer _TLASNodesTBO               = { 0, { 0, TextureSlot::TLASNodes              } };
  GLTextureBuffer _TLASMeshMatIDTBO           = { 0, { 0, TextureSlot::TLASMeshMatID          } };
  GLTextureBuffer _BLASNodesTBO               = { 0, { 0, TextureSlot::BLASNodes              } };
  GLTextureBuffer _BLASNodesRangeTBO          = { 0, { 0, TextureSlot::BLASNodesRange         } };
  GLTextureBuffer _BLASPackedIndicesTBO       = { 0, { 0, TextureSlot::BLASPackedIndices      } };
  GLTextureBuffer _BLASPackedIndicesRangeTBO  = { 0, { 0, TextureSlot::BLASPackedIndicesRange } };
  GLTextureBuffer _BLASPackedVerticesTBO      = { 0, { 0, TextureSlot::BLASPackedVertices     } };
  GLTextureBuffer _BLASPackedNormalsTBO       = { 0, { 0, TextureSlot::BLASPackedNormals      } };
  GLTextureBuffer _BLASPackedUVsTBO           = { 0, { 0, TextureSlot::BLASPackedUVs          } };

  // Textures
  GLTexture _TexArrayTEX         = { 0, TextureSlot::TexArray         };
  GLTexture _MaterialsTEX        = { 0, TextureSlot::Materials        };
  GLTexture _TLASTransformsIDTEX = { 0, TextureSlot::TLASTransformsID };
  GLTexture _EnvMapTEX           = { 0, TextureSlot::EnvMap };

  // Shaders
  std::unique_ptr<ShaderProgram> _PathTraceShader;
  std::unique_ptr<ShaderProgram> _AccumulateShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Tiled rendering
  Vec2i        _CurTile;
  Vec2i        _NbTiles;
  unsigned int _NbCompleteFrames = 0;

  // Accumulate
  unsigned int _AccumulatedFrames = 0;

  // Scene data
  int _NbTriangles     = 0;
  int _NbMeshInstances = 0;

  // DEBUG
  int _DebugMode = 0;
};

}

#endif /* _PathTracer_ */
