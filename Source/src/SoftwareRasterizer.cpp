#include "SoftwareRasterizer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "ShaderProgram.h"

#include <string>
#include <iostream>
#include <execution>

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "stb_image_write.h"


constexpr std::execution::parallel_policy policy = std::execution::par;

namespace fs = std::filesystem;

namespace RTRT
{

// ----------------------------------------------------------------------------
// METHODS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
SoftwareRasterizer::SoftwareRasterizer( Scene & iScene, RenderSettings & iSettings )
: Renderer(iScene, iSettings)
{
  UpdateRenderResolution();
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
SoftwareRasterizer::~SoftwareRasterizer()
{
  GLUtil::DeleteFBO(_RenderTargetFBO);

  GLUtil::DeleteTEX(_ColorBufferTEX);

  UnloadScene();
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Initialize()
{
  if ( 0 != ReloadScene() )
  {
    std::cout << "SoftwareRasterizer : Failed to load scene !" << std::endl;
    return 1;
  }

  if ( ( 0 != RecompileShaders() ) || !_RenderToTextureShader || !_RenderToScreenShader )
  {
    std::cout << "SoftwareRasterizer : Shader compilation failed !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeFrameBuffers() )
  {
    std::cout << "SoftwareRasterizer : Failed to initialize frame buffers !" << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Update()
{
  //if ( Dirty() )
  //{
  //  this -> ResetTiles();
  //  _NbCompleteFrames = 0;
  //}
  //else if ( TiledRendering() )
  //  this -> NextTile();

  if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    this -> ResizeRenderTarget();

  if ( _DirtyStates & (unsigned long)DirtyState::SceneEnvMap )
    this -> ReloadEnvMap();

  this -> UpdateTextures();

  this -> UpdateRenderToTextureUniforms();
  this -> UpdateRenderToScreenUniforms();

  return 0;
}

// ----------------------------------------------------------------------------
// Done
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Done()
{
  _FrameNum++;

  //if ( !Dirty() && !TiledRendering() )
    _NbCompleteFrames++;

  CleanStates();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateTextures()
{
  glBindTexture(GL_TEXTURE_2D, _ColorBufferTEX._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_ImageBuffer._ColorBuffer[0]);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}


// ----------------------------------------------------------------------------
// UpdateRenderToTextureUniforms
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateRenderToTextureUniforms()
{
  _RenderToTextureShader -> Use();

  _RenderToTextureShader -> SetUniform("u_ImageTexture", (int)RasterTexSlot::_ColorBuffer);

  _RenderToTextureShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToTextureTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::BindRenderToTextureTextures()
{
  GLUtil::ActivateTexture(_ColorBufferTEX);

  GLUtil::ActivateTexture(_RenderTargetFBO._Tex);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderToTexture()
{
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._ID);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  this -> BindRenderToTextureTextures();

  _Quad.Render(*_RenderToTextureShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderToScreenUniforms
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateRenderToScreenUniforms()
{
  _RenderToScreenShader -> Use();

  _RenderToScreenShader -> SetUniform("u_ScreenTexture", (int)RasterTexSlot::_RenderTarget);
  _RenderToScreenShader -> SetUniform("u_RenderRes", (float)_Settings._WindowResolution.x, (float)_Settings._WindowResolution.y);
  _RenderToScreenShader -> SetUniform("u_Gamma", _Settings._Gamma);
  _RenderToScreenShader -> SetUniform("u_Exposure", _Settings._Exposure);
  _RenderToScreenShader -> SetUniform("u_ToneMapping", 0);
  _RenderToScreenShader -> SetUniform("u_FXAA", (_Settings._FXAA ?  1 : 0 ));

  _RenderToScreenShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::BindRenderToScreenTextures()
{
  GLUtil::ActivateTexture(_RenderTargetFBO._Tex);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToScreen
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderToScreen()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  this -> BindRenderToScreenTextures();

  _Quad.Render(*_RenderToScreenShader);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderToFile( const fs::path & iFilePath )
{
  GLFrameBuffer temporaryFBO = { 0, { 0, RasterTexSlot::_Temporary } };

  // Temporary frame buffer
  glGenTextures(1, &temporaryFBO._Tex._ID);
  glActiveTexture(GL_TEX_UNIT(temporaryFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, temporaryFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._WindowResolution.x, _Settings._WindowResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &temporaryFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, temporaryFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temporaryFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    GLUtil::DeleteTEX(temporaryFBO._Tex);
    return 1;
  }

  // Render to texture
  {
    glBindFramebuffer(GL_FRAMEBUFFER, temporaryFBO._ID);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

    glActiveTexture(GL_TEX_UNIT(temporaryFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, temporaryFBO._Tex._ID);
    this -> BindRenderToScreenTextures();

    _Quad.Render(*_RenderToScreenShader);
  }

  // Retrieve image et save to file
  int saved = 0;
  {
    int w = _Settings._WindowResolution.x;
    int h = _Settings._WindowResolution.y;
    unsigned char * frameData = new unsigned char[w * h * 4];

    glActiveTexture(GL_TEX_UNIT(temporaryFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, temporaryFBO._Tex._ID);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData);
    stbi_flip_vertically_on_write( true );
    saved = stbi_write_png(iFilePath.string().c_str(), w, h, 4, frameData, w * 4);

    DeleteTab(frameData);
  }

  if ( saved && fs::exists(iFilePath) )
    std::cout << "Frame saved in " << fs::absolute(iFilePath) << std::endl;
  else
    std::cout << "ERROR : Failed to save screen capture in " << fs::absolute(iFilePath) << std::endl;

  // Clean
  GLUtil::DeleteFBO(temporaryFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderResolution
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateRenderResolution()
{
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * RenderScale());
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * RenderScale());

  _ImageBuffer._ColorBuffer.resize(RenderWidth() * RenderHeight());
  _ImageBuffer._DepthBuffer.resize(RenderWidth() * RenderHeight());

  // TMP
  const Vec4b fillColor(uint8_t(_Settings._BackgroundColor.x * 255), uint8_t(_Settings._BackgroundColor.y * 255), uint8_t(_Settings._BackgroundColor.z * 255), 255);
  std::fill(policy, _ImageBuffer._ColorBuffer.begin(), _ImageBuffer._ColorBuffer.end(), fillColor);
  std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), 1.f);

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeRenderTarget
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ResizeRenderTarget()
{
  UpdateRenderResolution();

  GLUtil::ResizeFBO(_RenderTargetFBO, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int SoftwareRasterizer::InitializeFrameBuffers()
{
  UpdateRenderResolution();

  // Render target textures
  glGenTextures(1, &_RenderTargetFBO._Tex._ID);
  glActiveTexture(GL_TEX_UNIT(_RenderTargetFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);
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

  // Color buffer Texture
  glGenTextures(1, &_ColorBufferTEX._ID);
  glActiveTexture(GL_TEX_UNIT(_ColorBufferTEX));
  glBindTexture(GL_TEXTURE_2D, _ColorBufferTEX._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_ImageBuffer._ColorBuffer[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_drawTexture.glsl");

  ShaderProgram * newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RenderToTextureShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Postprocess.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RenderToScreenShader.reset(newShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UnloadScene()
{
  _NbTriangles = 0;
  _NbMeshInstances = 0;

  GLUtil::DeleteTEX(_EnvMapTEX);

  _FrameNum = 0;

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadScene
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ReloadScene()
{
  UnloadScene();

  if ( ( _Settings._TextureSize.x > 0 ) && ( _Settings._TextureSize.y > 0 ) )
    _Scene.CompileMeshData( _Settings._TextureSize, false, false );
  else
    return 1;

  _NbTriangles = _Scene.GetNbFaces();
  _NbMeshInstances = _Scene.GetNbMeshInstances();

  //this -> ReloadEnvMap();

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadEnvMap
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ReloadEnvMap()
{
  GLUtil::DeleteTEX(_EnvMapTEX);

  if ( _Scene.GetEnvMap().IsInitialized() )
  {
    glGenTextures(1, &_EnvMapTEX._ID);
    glBindTexture(GL_TEXTURE_2D, _EnvMapTEX._ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, _Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight(), 0, GL_RGB, GL_FLOAT, _Scene.GetEnvMap().GetRawData());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    _Scene.GetEnvMap().SetTexID(_EnvMapTEX._ID);
  }
  else
    _Settings._EnableSkybox = false;

  return 0;
}

// ----------------------------------------------------------------------------
// NextTile
// ----------------------------------------------------------------------------
void SoftwareRasterizer::NextTile()
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
void SoftwareRasterizer::ResetTiles()
{
  _CurTile.x = -1;
  _CurTile.y = NbTiles().y - 1;
  _NbCompleteFrames = 0;
}

}
