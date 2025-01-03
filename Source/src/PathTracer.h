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
  RenderTargetTile
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

  float RenderScale()       const { return ( _Settings._RenderScale * 0.01f ); }
  float LowResRenderScale() const { return ( RenderScale() * _Settings._LowResRatio ); }
  int RenderWidth()         const { return _Settings._RenderResolution.x; }
  int RenderHeight()        const { return _Settings._RenderResolution.y; }
  int TileWidth()           const { return ( _Settings._TileResolution.x > 0 ) ? ( _Settings._TileResolution.x ) : ( 64 ); }
  int TileHeight()          const { return ( _Settings._TileResolution.y > 0 ) ? ( _Settings._TileResolution.y ) : ( 64 ); }
  int LowResRenderWidth()   const { return int( _Settings._RenderResolution.x * LowResRenderScale() ); }
  int LowResRenderHeight()  const { return int( _Settings._RenderResolution.y * LowResRenderScale() ); }

protected:

  QuadMesh _Quad;

  // Frame buffers
  GLuint   _ID_RenderTargetFBO       = 0;
  GLuint   _ID_RenderTargetLowResFBO = 0;
  GLuint   _ID_RenderTargetTileFBO   = 0;

  // Textures
  GLuint   _ID_RenderTargetTex       = 0;
  GLuint   _ID_RenderTargetLowResTex = 0;
  GLuint   _ID_RenderTargetTileTex   = 0;

  // Shaders
  std::unique_ptr<ShaderProgram> _PathTraceShader;
  std::unique_ptr<ShaderProgram> _AccumulateShader;
  std::unique_ptr<ShaderProgram> _RenderToScreenShader;
};

}

#endif /* _PathTracer_ */
