#ifndef _PathTracer_
#define _PathTracer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"

#include "GL/glew.h"

namespace RTRT
{

enum class TextureSlot
{
  RenderTarget = 0,
  RenderTargetLowRes,
  RenderTargetTile,
  Accumulate
};

class Scene;

class PathTracer : public Renderer
{
public:
  PathTracer( Scene & iScene, RenderSettings & iSettings );
  virtual ~PathTracer();

  virtual int RenderToTexture();
  virtual int RenderToScreen();

protected:

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int RecompileShaders();

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
  GLuint   _ID_RenderTargetFBO       = 0;
  GLuint   _ID_RenderTargetLowResFBO = 0;
  GLuint   _ID_RenderTargetTileFBO   = 0;
  GLuint   _ID_AccumulateFBO         = 0;

  // Textures
  GLuint   _ID_RenderTargetTex       = 0;
  GLuint   _ID_RenderTargetLowResTex = 0;
  GLuint   _ID_RenderTargetTileTex   = 0;
  GLuint   _ID_AccumulateTex         = 0;

  // Shaders
  std::unique_ptr<ShaderProgram> _PathTraceShader;
  std::unique_ptr<ShaderProgram> _AccumulateShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Tiled rendering
  Vec2i        _CurTile;
  Vec2i        _NbTiles;
  unsigned int _NbCompleteFrames = 0;

  // Scene data
  int _NbTriangles     = 0;
  int _NbMeshInstances = 0;
};

}

#endif /* _PathTracer_ */
