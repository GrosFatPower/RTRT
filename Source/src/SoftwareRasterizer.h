#ifndef _SoftwareRasterizer_
#define _SoftwareRasterizer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"

#include "GL/glew.h"

#include <memory>

namespace RTRT
{

struct RasterTexSlot
{
  static const TextureSlot _RenderTarget       = 0;
  static const TextureSlot _RenderTargetLowRes = 1;
  static const TextureSlot _RenderTargetTile   = 2;
  static const TextureSlot _EnvMap             = 3;
  static const TextureSlot _Temporary          = 4;
};

class Scene;

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

protected:

  int UpdateRenderResolution();
  int ResizeRenderTarget();

  int InitializeFrameBuffers();
  int RecompileShaders();

  int UnloadScene();
  int ReloadScene();
  int ReloadEnvMap();

  int UpdateRenderToScreenUniforms();
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

protected:

  QuadMesh _Quad;

  // Frame buffers
  GLFrameBuffer _RenderTargetFBO = { 0, { 0, GL_TEXTURE_2D, RasterTexSlot::_RenderTarget } };

  // Textures
  GLTexture _EnvMapTEX = { 0, RasterTexSlot::_EnvMap };

  // Shaders
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;

  // Tiled rendering
  Vec2i        _CurTile;
  Vec2i        _NbTiles;
  unsigned int _NbCompleteFrames = 0;

  unsigned int _FrameNum = 1;

  // Scene data
  int _NbTriangles     = 0;
  int _NbMeshInstances = 0;
};

}

#endif /* _SoftwareRasterizer_ */
