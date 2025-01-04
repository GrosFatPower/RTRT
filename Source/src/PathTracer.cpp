#include "PathTracer.h"

#include "Scene.h"
#include "ShaderProgram.h"

#define TEX_UNIT(x) ( GL_TEXTURE0 + (int)x._Slot )

namespace RTRT
{

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
PathTracer::PathTracer( Scene & iScene, RenderSettings & iSettings )
: Renderer(iScene, iSettings)
{
  UpdateRenderResolution();
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
PathTracer::~PathTracer()
{
  glDeleteFramebuffers(1, &_RenderTargetFBO._ID);
  glDeleteFramebuffers(1, &_RenderTargetLowResFBO._ID);
  glDeleteFramebuffers(1, &_RenderTargetTileFBO._ID);
  glDeleteFramebuffers(1, &_AccumulateFBO._ID);

  glDeleteTextures(1, &_RenderTargetFBO._Tex._ID);
  glDeleteTextures(1, &_RenderTargetLowResFBO._Tex._ID);
  glDeleteTextures(1, &_RenderTargetTileFBO._Tex._ID);
  glDeleteTextures(1, &_RenderTargetTileFBO._Tex._ID);
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int PathTracer::RenderToTexture()
{
  this -> BindPathTraceTextures();

  // Path trace
  if ( LowResPass() )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._ID);
    glViewport(0, 0, LowResRenderWidth(), LowResRenderHeight());
  }
  else if ( TiledRendering() )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._ID);
    glViewport(0, 0, TileWidth(), TileHeight());
  }
  else
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._ID);
    glViewport(0, 0, RenderWidth(), RenderHeight());
  }

  _Quad.Render(*_PathTraceShader);

  // Accumulate
  this -> BindAccumulateTextures();

  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._ID);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  _Quad.Render(*_AccumulateShader);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToScreen
// ----------------------------------------------------------------------------
int PathTracer::RenderToScreen()
{
  this -> BindRenderToScreenTextures();

  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  _Quad.Render(*_RenderToScreenShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderResolution
// ----------------------------------------------------------------------------
int PathTracer::UpdateRenderResolution()
{
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * RenderScale());
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * RenderScale());

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeRenderTarget
// ----------------------------------------------------------------------------
int PathTracer::ResizeRenderTarget()
{
  UpdateRenderResolution();

  glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TileWidth(), TileHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, LowResRenderWidth(), LowResRenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_2D, _AccumulateFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderWidth(), 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._ID);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._ID);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._ID);
  glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._ID);
  glClear(GL_COLOR_BUFFER_BIT);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int PathTracer::InitializeFrameBuffers()
{
  UpdateRenderResolution();

  // Render target textures
  glGenTextures(1, &_RenderTargetFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenTextures(1, &_RenderTargetTileFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetTileFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TileWidth(), TileHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenTextures(1, &_RenderTargetLowResFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetLowResFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, LowResRenderWidth(), LowResRenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenTextures(1, &_AccumulateFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_AccumulateFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _AccumulateFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Render target Frame buffers
  glGenFramebuffers(1, &_RenderTargetFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glGenFramebuffers(1, &_RenderTargetTileFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glGenFramebuffers(1, &_RenderTargetLowResFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glGenFramebuffers(1, &_AccumulateFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _AccumulateFBO._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int PathTracer::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_RTRenderer.glsl");

  ShaderProgram * newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _PathTraceShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _AccumulateShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Postpro.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RenderToScreenShader.reset(newShader);

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadScene
// ----------------------------------------------------------------------------
int PathTracer::ReloadScene()
{
  if ( ( _Settings._TextureSize.x > 0 ) && ( _Settings._TextureSize.y > 0 ) )
    _Scene.CompileMeshData( _Settings._TextureSize, true, true );
  else
    return 1;

  _NbTriangles = _Scene.GetNbFaces();
  _NbMeshInstances = _Scene.GetNbMeshInstances();

  if ( _NbTriangles )
  {
    glPixelStorei(GL_PACK_ALIGNMENT, 1); // ??? Necessary

    glGenBuffers(1, &_VtxTBO._ID);
    glBindBuffer(GL_TEXTURE_BUFFER, _VtxTBO._ID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene.GetVertices().size(), &_Scene.GetVertices()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_VtxTBO._Tex._ID);
    glBindTexture(GL_TEXTURE_BUFFER, _VtxTBO._Tex._ID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _VtxTBO._ID);

    glGenBuffers(1, &_VtxNormTBO._ID);
    glBindBuffer(GL_TEXTURE_BUFFER, _VtxNormTBO._ID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene.GetNormals().size(), &_Scene.GetNormals()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_VtxNormTBO._Tex._ID);
    glBindTexture(GL_TEXTURE_BUFFER, _VtxNormTBO._Tex._ID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _VtxNormTBO._ID);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// UpdatePathTraceUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdatePathTraceUniforms()
{
  _PathTraceShader -> Use();


  _PathTraceShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateAccumulateUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateAccumulateUniforms()
{
  _AccumulateShader -> Use();


  _AccumulateShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderToScreenUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateRenderToScreenUniforms()
{
  _RenderToScreenShader -> Use();


  _RenderToScreenShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindPathTraceTextures
// ----------------------------------------------------------------------------
int PathTracer::BindPathTraceTextures()
{
  return 0;
}

// ----------------------------------------------------------------------------
// BindAccumulateTextures
// ----------------------------------------------------------------------------
int PathTracer::BindAccumulateTextures()
{
  if ( LowResPass() )
  {
    glActiveTexture(TEX_UNIT(_RenderTargetLowResFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID);
  }
  else if ( TiledRendering() )
  {
    glActiveTexture(TEX_UNIT(_RenderTargetTileFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID);
  }
  else
  {
    glActiveTexture(TEX_UNIT(_RenderTargetFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int PathTracer::BindRenderToScreenTextures()
{
  glActiveTexture(TEX_UNIT(_AccumulateFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _AccumulateFBO._Tex._ID);

  return 0;
}

// ----------------------------------------------------------------------------
// NextTile
// ----------------------------------------------------------------------------
void PathTracer::NextTile()
{
  _CurTile.x++;
  if ( _CurTile.x >= NbTiles().x )
  {
    _CurTile.x = 0;
    _CurTile.y--;
    if ( _CurTile.y < 0 )
    {
      _CurTile.x = 0;
      _CurTile.y = NbTiles().y - 1;
      _NbCompleteFrames++;
    }
  }
}

// ----------------------------------------------------------------------------
// ResetTiles
// ----------------------------------------------------------------------------
void PathTracer::ResetTiles()
{
  _CurTile.x = -1;
  _CurTile.y = NbTiles().y - 1;
  _NbCompleteFrames = 0;
}

}
