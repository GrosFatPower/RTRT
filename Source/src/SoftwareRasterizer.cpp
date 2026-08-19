#pragma warning(disable : 4100) // unreferenced formal parameter

#include "SoftwareRasterizer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "ShaderProgram.h"
#include "SoftwareVertexShader.h"
#include "SoftwareFragmentShader.h"
#include "SutherlandHodgman.h"
#include "JobSystem.h"
#include "PathUtils.h"

#include <string>
#include <iostream>
#include <execution>
#include <unordered_map>
//#include <omp.h>
#include <thread>
#include <cstring>
#include <limits>

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "stb_image_write.h"


#if defined(_WIN32) || defined(_WIN64)
static constexpr auto& policy = std::execution::par;
#else
static constexpr auto& policy = std::execution::seq;
#endif

/**
 * The vertices vector contains a lot of duplicated vertex data,
 * because many vertices are included in multiple triangles.
 * We should keep only the unique vertices and use
 * the index buffer to reuse them whenever they come up.
 * https://en.cppreference.com/w/cpp/utility/hash
 */
namespace std
{
  template <>
  struct hash<RTRT::RasterData::Vertex>
  {
    size_t operator()(RTRT::RasterData::Vertex const& iV) const
    {
      return
        ((hash<Vec3>()(iV._WorldPos))
          ^ (hash<Vec3>()(iV._Normal))
          ^ (hash<Vec2>()(iV._UV)));
    }
  };
}

namespace fs = std::filesystem;
namespace rd = RTRT::RasterData;

namespace RTRT
{

static constexpr RGBA8 S_DefaultColor(0, 0, 0, (uint8_t)255);


// ----------------------------------------------------------------------------
// ComputeTriangleUVPartials
// Compute UV partials for a triangle (v0,v1,v2 screen coords and uvs)
// ----------------------------------------------------------------------------
int ComputeTriangleUVPartials(const Vec2 & iP0,  const Vec2 & iP1,  const Vec2 & iP2,
                               const Vec2 & iUV0, const Vec2 & iUV1, const Vec2 & iUV2,
                               float & odUdx, float & odUdy, float & odVdx, float & odVdy)
{
  float x1 = iP1.x - iP0.x, y1 = iP1.y - iP0.y;
  float x2 = iP2.x - iP0.x, y2 = iP2.y - iP0.y;
  float du1 = iUV1.x - iUV0.x, dv1 = iUV1.y - iUV0.y;
  float du2 = iUV2.x - iUV0.x, dv2 = iUV2.y - iUV0.y;

  float denom = x1 * y2 - x2 * y1;
  if ( fabs(denom) >= EPSILON )
  {
    float inv = 1.0f / denom;
    odUdx = (du1 * y2 - du2 * y1) * inv;
    odUdy = (-du1 * x2 + du2 * x1) * inv;
    odVdx = (dv1 * y2 - dv2 * y1) * inv;
    odVdy = (-dv1 * x2 + dv2 * x1) * inv;
    return 0;
  }

  odUdx = odUdy = odVdx = odVdy = 0.f;
  return 1;
};

// ----------------------------------------------------------------------------
// METHODS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
SoftwareRasterizer::SoftwareRasterizer(Scene& iScene, RenderSettings& iSettings)
  : Renderer(iScene, iSettings)
{
  UpdateRenderResolution();
  UpdateNumberOfWorkers(true);
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
SoftwareRasterizer::~SoftwareRasterizer()
{
  for ( GLsync & fence : _UploadFences )
  {
    if ( fence )
      glDeleteSync(fence);
    fence = nullptr;
  }
  if ( _UploadPBOs[0] || _UploadPBOs[1] )
    glDeleteBuffers(2, _UploadPBOs.data());
  for ( auto & timerIDs : _TimerIDs )
  {
    if ( timerIDs[0] )
      glDeleteQueries(1, &timerIDs[0]);
    if ( timerIDs[1] )
      glDeleteQueries(1, &timerIDs[1]);
  }

  GLUtil::DeleteFBO(_RenderTargetFBO);

  GLUtil::DeleteTEX(_RenderTargetTEX);
  GLUtil::DeleteTEX(_ColorBufferTEX);

  UnloadScene();
}

// ----------------------------------------------------------------------------
// GetSIMDMode
// ----------------------------------------------------------------------------
const char * SoftwareRasterizer::GetSIMDMode() const
{
  if ( !_EnableSIMD )
    return "Scalar";
#if defined(SIMD_AVX2)
  return "AVX2";
#elif defined(SIMD_ARM_NEON)
  return "NEON";
#else
  return "Scalar fallback";
#endif
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Initialize()
{
  if (0 != ReloadScene())
  {
    std::cout << "SoftwareRasterizer : Failed to load scene !" << std::endl;
    return 1;
  }

  if ((0 != RecompileShaders()) || !_RenderToTextureShader || !_RenderToScreenShader)
  {
    std::cout << "SoftwareRasterizer : Shader compilation failed !" << std::endl;
    return 1;
  }

  if (0 != InitializeFrameBuffers())
  {
    std::cout << "SoftwareRasterizer : Failed to initialize frame buffers !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeStats() )
  {
    std::cout << "SoftwareRasterizer : Failed to initialize frame statistics !" << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateNumberOfWorkers(bool iForce)
{
  if ((_NbJobs != _Settings._NbThreads) || iForce)
  {
    _NbJobs = std::min(_Settings._NbThreads, std::thread::hardware_concurrency());

    JobSystem::Get().Initialize(_NbJobs);

    _RasterTrianglesBuf.resize(_NbJobs);

    for (unsigned int i = 0; i < _NbJobs; ++i)
      _RasterTrianglesBuf[i].reserve(std::max(_Triangles.size() / _NbJobs, (size_t)1));

    _Fragments.resize(_NbJobs);
    _TransparentFragments.resize(_NbJobs);
    _MaskedTestedBuf.resize(_NbJobs, 0);
    _MaskedRejectedBuf.resize(_NbJobs, 0);

    for (auto& tile : _Tiles)
    {
      tile._RasterTrisBins.resize(_NbJobs);
      for (auto& bin : tile._RasterTrisBins)
        bin.reserve(100);
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Update()
{
  UpdateStats();
  for ( int timing = TimingInstanceRefresh; timing <= TimingColorUpload; ++timing )
  {
    if ( ( timing != TimingCopyToRenderTarget ) && ( timing != TimingCompositeScreen ) )
      _PassTimes[timing] = 0.;
  }
  _Stats._InputInstances = _Scene.GetNbMeshInstances();
  _Stats._VisibleInstances = _CachedVisibleMeshInstanceCount;
  _Stats._InputTriangles = _Triangles.size();
  _Stats._ChangedInstances = 0;
  _Stats._RefreshedVertices = 0;
  _Stats._RefreshedTriangles = 0;
  _Stats._ClippedTriangles = 0;
  _Stats._BinnedTriangles = 0;
  _Stats._DepthWinningPixels = 0;
  _Stats._CoveredPixels = 0;
  _Stats._ShadedPixels = 0;
  _Stats._TileJobs = 0;
  _Stats._CopiedBytes = 0;
  _Stats._HitBufferBytes = 0;
  _Stats._MaskedFragmentsTested = 0;
  _Stats._MaskedFragmentsRejected = 0;
  _Stats._TransparentHitsGenerated = 0;
  _Stats._TransparentHitsShaded = 0;
  _Stats._BlendHitsGenerated = 0;
  _Stats._TransmissionHitsGenerated = 0;
  _Stats._TransparentPixels = 0;
  _Stats._MaxTransparentLayers = 0;
  _Stats._TransparentHitBufferBytes = 0;
  _Stats._AverageTransparentLayers = 0.;

  const double updateStartTime = glfwGetTime();
  if (_DirtyStates & (unsigned long)DirtyState::RenderSettings)
  {
    this->ResizeRenderTarget();
    this->UpdateNumberOfWorkers();
  }

  if (_DirtyStates & (unsigned long)DirtyState::SceneEnvMap)
    this->ReloadEnvMap();

  if (_DirtyStates & (unsigned long)DirtyState::SceneInstances)
  {
    const double refreshStartTime = glfwGetTime();
    _PassEnabled[TimingInstanceRefresh] = true;
    if ( CanRefreshSceneInstanceTransforms() )
    {
      const int refreshResult = _EnableIncrementalRefresh ? this->RefreshSceneInstanceTransforms() : this->RefreshAllSceneInstanceTransforms();
      if ( 0 != refreshResult )
        return 1;
    }
    else if ( 0 != this->ReloadScene() )
      return 1;
    _PassTimes[TimingInstanceRefresh] = glfwGetTime() - refreshStartTime;
  }

  this->UpdateImageBuffer();

  const double uploadStartTime = glfwGetTime();
  _PassEnabled[TimingColorUpload] = true;
  this->UpdateTextures();
  _PassTimes[TimingColorUpload] = glfwGetTime() - uploadStartTime;
  _Stats._CopiedBytes += static_cast<std::uint64_t>(_ImageBuffer._ColorBuffer.size()) * sizeof(RGBA8);

  this->UpdateRenderToTextureUniforms();
  this->UpdateRenderToScreenUniforms();
  _PassEnabled[TimingUniformUpdate] = true;
  _PassTimes[TimingUniformUpdate] = glfwGetTime() - updateStartTime
    - _PassTimes[TimingInstanceRefresh]
    - _PassTimes[TimingFrameClear]
    - _PassTimes[TimingBackground]
    - _PassTimes[TimingRenderScene]
    - _PassTimes[TimingTransparentRasterize]
    - _PassTimes[TimingTransparentFragments]
    - _PassTimes[TimingColorUpload];
  _PassTimes[TimingUniformUpdate] = std::max(0., _PassTimes[TimingUniformUpdate]);

  return 0;
}

// ----------------------------------------------------------------------------
// Done
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Done()
{
  _FrameNum++;

  _NbCompleteFrames++;

  CleanStates();

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeStats
// ----------------------------------------------------------------------------
int SoftwareRasterizer::InitializeStats()
{
  _PassTimes.fill(0.);
  _PassEnabled.fill(false);
  _TimerWritten.fill(false);

  for ( auto & timerIDs : _TimerIDs )
  {
    if ( !timerIDs[0] )
      glGenQueries(1, &timerIDs[0]);
    if ( !timerIDs[1] )
      glGenQueries(1, &timerIDs[1]);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateStats
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateStats()
{
  if ( _TimerWritten[TimingCopyToRenderTarget] )
    _PassTimes[TimingCopyToRenderTarget] = ReadTimer(TimingCopyToRenderTarget);
  else
    _PassTimes[TimingCopyToRenderTarget] = 0.;

  if ( _TimerWritten[TimingCompositeScreen] )
    _PassTimes[TimingCompositeScreen] = ReadTimer(TimingCompositeScreen);
  else
    _PassTimes[TimingCompositeScreen] = 0.;

  _PassEnabled.fill(false);

  return 0;
}

// ----------------------------------------------------------------------------
// BeginTimer
// ----------------------------------------------------------------------------
void SoftwareRasterizer::BeginTimer( int iTimerID )
{
  if ( ( iTimerID < 0 ) || ( iTimerID >= TimingCount ) )
    return;

  _PassEnabled[iTimerID] = true;
  _TimerWritten[iTimerID] = true;
#if defined(__APPLE__)
  glBeginQuery(GL_TIME_ELAPSED, _TimerIDs[iTimerID][0]);
#else
  glQueryCounter(_TimerIDs[iTimerID][0], GL_TIMESTAMP);
#endif
}

// ----------------------------------------------------------------------------
// EndTimer
// ----------------------------------------------------------------------------
void SoftwareRasterizer::EndTimer( int iTimerID )
{
  if ( ( iTimerID < 0 ) || ( iTimerID >= TimingCount ) )
    return;

#if defined(__APPLE__)
  glEndQuery(GL_TIME_ELAPSED);
#else
  glQueryCounter(_TimerIDs[iTimerID][1], GL_TIMESTAMP);
#endif
}

// ----------------------------------------------------------------------------
// ReadTimer
// ----------------------------------------------------------------------------
double SoftwareRasterizer::ReadTimer( int iTimerID )
{
  if ( ( iTimerID < 0 ) || ( iTimerID >= TimingCount ) )
    return 0.;

  GLuint64 startTime = 0, endTime = 0, executionTime = 0;
  GLint resultAvailable = 0;

#if defined(__APPLE__)
  while ( !resultAvailable )
    glGetQueryObjectiv(_TimerIDs[iTimerID][0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
  glGetQueryObjectui64v(_TimerIDs[iTimerID][0], GL_QUERY_RESULT, &executionTime);
#else
  while ( !resultAvailable )
    glGetQueryObjectiv(_TimerIDs[iTimerID][0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
  glGetQueryObjectui64v(_TimerIDs[iTimerID][0], GL_QUERY_RESULT, &startTime);

  resultAvailable = 0;
  while ( !resultAvailable )
    glGetQueryObjectiv(_TimerIDs[iTimerID][1], GL_QUERY_RESULT_AVAILABLE, &resultAvailable);
  glGetQueryObjectui64v(_TimerIDs[iTimerID][1], GL_QUERY_RESULT, &endTime);

  executionTime = endTime - startTime;
#endif

  return (double)executionTime / 1000000000.;
}

// ----------------------------------------------------------------------------
// GetRenderPassTimings
// ----------------------------------------------------------------------------
int SoftwareRasterizer::GetRenderPassTimings( std::vector<RenderPassTiming> & oTimings ) const
{
  oTimings.clear();
  oTimings.push_back({ "Instance transform refresh", _PassTimes[TimingInstanceRefresh], false, _PassEnabled[TimingInstanceRefresh] });
  oTimings.push_back({ "Frame / tile clear", _PassTimes[TimingFrameClear], false, _PassEnabled[TimingFrameClear] });
  oTimings.push_back({ "Environment background", _PassTimes[TimingBackground], false, _PassEnabled[TimingBackground] });
  oTimings.push_back({ "Uniform / update overhead", _PassTimes[TimingUniformUpdate], false, _PassEnabled[TimingUniformUpdate] });
  oTimings.push_back({ "Process vertices", _PassTimes[TimingProcessVertices], false, _PassEnabled[TimingProcessVertices] });
  oTimings.push_back({ "Clip triangles", _PassTimes[TimingClipTriangles], false, _PassEnabled[TimingClipTriangles] });
  oTimings.push_back({ "Rasterize", _PassTimes[TimingRasterize], false, _PassEnabled[TimingRasterize] });
  oTimings.push_back({ "Process fragments", _PassTimes[TimingProcessFragments], false, _PassEnabled[TimingProcessFragments] });
  oTimings.push_back({ "Transparent rasterize", _PassTimes[TimingTransparentRasterize], false, _PassEnabled[TimingTransparentRasterize] });
  oTimings.push_back({ "Transparent shade / composite", _PassTimes[TimingTransparentFragments], false, _PassEnabled[TimingTransparentFragments] });
  oTimings.push_back({ "Render scene", _PassTimes[TimingRenderScene], false, _PassEnabled[TimingRenderScene], true });
  oTimings.push_back({ "CPU color-buffer upload", _PassTimes[TimingColorUpload], false, _PassEnabled[TimingColorUpload] });
  oTimings.push_back({ "Copy to render target", _PassTimes[TimingCopyToRenderTarget], true, _PassEnabled[TimingCopyToRenderTarget] });
  oTimings.push_back({ "Composite / screen", _PassTimes[TimingCompositeScreen], true, _PassEnabled[TimingCompositeScreen] });
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateTextures()
{
  if ( !_ColorBufferTEX._Handle || ( RenderWidth() <= 0 ) || ( RenderHeight() <= 0 ) || _ImageBuffer._ColorBuffer.empty() )
    return 0;

  const size_t expectedSize = static_cast<size_t>(RenderWidth()) * static_cast<size_t>(RenderHeight());
  if ( _ImageBuffer._ColorBuffer.size() < expectedSize )
    return 1;

  if ( _EnablePBOUpload )
  {
    const size_t uploadSize = expectedSize * sizeof(RGBA8);
    if ( !_UploadPBOs[0] )
      glGenBuffers(2, _UploadPBOs.data());
    if ( _UploadPBOSize != uploadSize )
    {
      for ( unsigned int index = 0; index < _UploadPBOs.size(); ++index )
      {
        if ( _UploadFences[index] )
        {
          glClientWaitSync(_UploadFences[index], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
          glDeleteSync(_UploadFences[index]);
          _UploadFences[index] = nullptr;
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _UploadPBOs[index]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, uploadSize, nullptr, GL_STREAM_DRAW);
      }
      _UploadPBOSize = uploadSize;
    }
    const unsigned int pboIndex = _UploadPBOIndex++ % 2;
    if ( _UploadFences[pboIndex] )
    {
      glClientWaitSync(_UploadFences[pboIndex], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
      glDeleteSync(_UploadFences[pboIndex]);
      _UploadFences[pboIndex] = nullptr;
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _UploadPBOs[pboIndex]);
    void * mapped = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, uploadSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if ( !mapped )
    {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
      return 1;
    }
    std::memcpy(mapped, _ImageBuffer._ColorBuffer.data(), uploadSize);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glActiveTexture(GL_TEXTURE0 + _ColorBufferTEX._Slot);
    glBindTexture(_ColorBufferTEX._Target, _ColorBufferTEX._Handle);
    glTexSubImage2D(_ColorBufferTEX._Target, 0, 0, 0, RenderWidth(), RenderHeight(), _ColorBufferTEX._DataFormat, _ColorBufferTEX._DataType, nullptr);
    _UploadFences[pboIndex] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  }
  else if ( !GLUtil::UpdateTexture2D(_ColorBufferTEX, RenderWidth(), RenderHeight(), &_ImageBuffer._ColorBuffer[0]) )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateImageBuffer
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateImageBuffer()
{
  int width = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;
  float ratio = width / float(height);

  float zNear, zFar = 1.f;
  if (_Settings._WBuffer)
    _Scene.GetCamera().GetZNearFar(zNear, zFar);
  const double clearStartTime = glfwGetTime();
  _PassEnabled[TimingFrameClear] = true;
  std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), zFar);

  float top, right;
  Mat4x4 P;
  _Scene.GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  ResetTiles();
  _PassTimes[TimingFrameClear] = glfwGetTime() - clearStartTime;

  const bool directColorWrites = TiledRendering() && _EnableCompactHits && _EnableDirectColorWrites;
  if ( !directColorWrites )
  {
    const double backgroundStartTime = glfwGetTime();
    _PassEnabled[TimingBackground] = true;
    RenderBackground(top, right);
    _PassTimes[TimingBackground] = glfwGetTime() - backgroundStartTime;
  }

  RenderScene();

  if ( directColorWrites )
  {
    const double backgroundStartTime = glfwGetTime();
    _PassEnabled[TimingBackground] = true;
    RenderUncoveredBackground(top, right);
    _PassTimes[TimingBackground] = glfwGetTime() - backgroundStartTime;
  }

  if ( _Settings._Transparency )
  {
    _PassEnabled[TimingTransparentRasterize] = true;
    const double transparentRasterStart = glfwGetTime();
    RasterizeTransparent();
    _PassTimes[TimingTransparentRasterize] = glfwGetTime() - transparentRasterStart;

    _PassEnabled[TimingTransparentFragments] = true;
    const double transparentShadeStart = glfwGetTime();
    ProcessTransparentFragments();
    _PassTimes[TimingTransparentFragments] = glfwGetTime() - transparentShadeStart;
  }
  else
  {
    _PassEnabled[TimingTransparentRasterize] = false;
    _PassEnabled[TimingTransparentFragments] = false;
    _PassTimes[TimingTransparentRasterize] = 0.;
    _PassTimes[TimingTransparentFragments] = 0.;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderToTextureUniforms
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateRenderToTextureUniforms()
{
  _RenderToTextureShader->Use();

  _RenderToTextureShader->SetUniform("u_ImageTexture", (int)RasterTexSlot::_ColorBuffer);

  _RenderToTextureShader->StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToTextureTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::BindRenderToTextureTextures()
{
  GLUtil::ActivateTexture(_ColorBufferTEX);

  GLUtil::ActivateTextures(_RenderTargetFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderToTexture()
{
  BeginTimer(TimingCopyToRenderTarget);

  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  this->BindRenderToTextureTextures();

  _Quad.Render(*_RenderToTextureShader);

  EndTimer(TimingCopyToRenderTarget);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderToScreenUniforms
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateRenderToScreenUniforms()
{
  _RenderToScreenShader->Use();

  _RenderToScreenShader->SetUniform("u_ScreenTexture", (int)RasterTexSlot::_RenderTarget);
  _RenderToScreenShader->SetUniform("u_RenderRes", (float)_Settings._WindowResolution.x, (float)_Settings._WindowResolution.y);
  _RenderToScreenShader->SetUniform("u_Gamma", _Settings._Gamma);
  _RenderToScreenShader->SetUniform("u_Exposure", _Settings._Exposure);
  _RenderToScreenShader->SetUniform("u_ToneMapping", _Settings._ToneMapping);
  _RenderToScreenShader->SetUniform("u_FXAA", (_Settings._FXAA ? 1 : 0));

  _RenderToScreenShader->StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::BindRenderToScreenTextures()
{
  GLUtil::ActivateTextures(_RenderTargetFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToScreen
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderToScreen()
{
  BeginTimer(TimingCompositeScreen);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  this->BindRenderToScreenTextures();

  _Quad.Render(*_RenderToScreenShader);

  EndTimer(TimingCompositeScreen);

  return 0;
}

// ----------------------------------------------------------------------------
// ReadbackFinalColor
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ReadbackFinalColor( RenderImage & oImage )
{
  if ( ( RenderWidth() <= 0 ) || ( RenderHeight() <= 0 ) )
    return 1;

  oImage._Width = RenderWidth();
  oImage._Height = RenderHeight();
  const size_t pixelCount = (size_t)oImage._Width * (size_t)oImage._Height;
  if ( _ImageBuffer._ColorBuffer.size() < pixelCount )
    return 1;

  oImage._Pixels.resize((size_t)oImage._Width * (size_t)oImage._Height * 4u);

  for ( int y = 0; y < oImage._Height; ++y )
  {
    const int sourceY = oImage._Height - 1 - y;
    for ( int x = 0; x < oImage._Width; ++x )
    {
      const RGBA8 & color = _ImageBuffer._ColorBuffer[(size_t)sourceY * (size_t)oImage._Width + (size_t)x];
      const size_t pixelIndex = ((size_t)y * (size_t)oImage._Width + (size_t)x) * 4u;
      oImage._Pixels[pixelIndex + 0] = color._R / 255.f;
      oImage._Pixels[pixelIndex + 1] = color._G / 255.f;
      oImage._Pixels[pixelIndex + 2] = color._B / 255.f;
      oImage._Pixels[pixelIndex + 3] = color._A / 255.f;
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderToFile(const fs::path& iFilePath)
{
  GLFrameBuffer temporaryFBO;
  GLTexture temporaryTEX = { 0, GL_TEXTURE_2D, RasterTexSlot::_Temporary };

  // Temporary frame buffer
  GLTextureDesc tempDesc;
  tempDesc._Target         = temporaryTEX._Target;
  tempDesc._Slot           = temporaryTEX._Slot;
  tempDesc._Width          = _Settings._WindowResolution.x;
  tempDesc._Height         = _Settings._WindowResolution.y;
  tempDesc._InternalFormat = GL_RGBA32F;
  tempDesc._DataFormat     = GL_RGBA;
  tempDesc._DataType       = GL_FLOAT;
  tempDesc._MinFilter      = GL_LINEAR;
  tempDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(tempDesc, temporaryTEX);

  GLFrameBufferDesc tempFBODesc;
  tempFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &temporaryTEX });
  if (!GLUtil::CreateFrameBuffer(tempFBODesc, temporaryFBO))
  {
    GLUtil::DeleteTEX(temporaryTEX);
    return 1;
  }

  // Render to texture
  {
    glBindFramebuffer(GL_FRAMEBUFFER, temporaryFBO._Handle);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

    glActiveTexture(GL_TEX_UNIT(temporaryTEX));
    glBindTexture(GL_TEXTURE_2D, temporaryTEX._Handle);
    this->BindRenderToScreenTextures();

    _Quad.Render(*_RenderToScreenShader);
  }

  // Retrieve image et save to file
  int saved = 0;
  {
    int w = _Settings._WindowResolution.x;
    int h = _Settings._WindowResolution.y;
    unsigned char* frameData = new unsigned char[w * h * 4];

    glActiveTexture(GL_TEX_UNIT(temporaryTEX));
    glBindTexture(GL_TEXTURE_2D, temporaryTEX._Handle);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData);
    stbi_flip_vertically_on_write(true);
    saved = stbi_write_png(iFilePath.string().c_str(), w, h, 4, frameData, w * 4);

    DeleteTab(frameData);
  }

  if (saved && fs::exists(iFilePath))
    std::cout << "Frame saved in " << fs::absolute(iFilePath) << std::endl;
  else
    std::cout << "ERROR : Failed to save screen capture in " << fs::absolute(iFilePath) << std::endl;

  // Clean
  GLUtil::DeleteFBO(temporaryFBO);
  GLUtil::DeleteTEX(temporaryTEX);

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

  ResizeTileMap();

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeRenderTarget
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ResizeRenderTarget()
{
  UpdateRenderResolution();

  GLUtil::ResizeFBO(_RenderTargetFBO, RenderWidth(), RenderHeight());
  if ( _ColorBufferTEX._Handle )
    GLUtil::ResizeTexture(_ColorBufferTEX, RenderWidth(), RenderHeight());

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int SoftwareRasterizer::InitializeFrameBuffers()
{
  UpdateRenderResolution();

  GLTextureDesc renderTargetDesc;
  renderTargetDesc._Target         = _RenderTargetTEX._Target;
  renderTargetDesc._Slot           = _RenderTargetTEX._Slot;
  renderTargetDesc._Width          = RenderWidth();
  renderTargetDesc._Height         = RenderHeight();
  renderTargetDesc._InternalFormat = _RenderTargetTEX._InternalFormat;
  renderTargetDesc._DataFormat     = _RenderTargetTEX._DataFormat;
  renderTargetDesc._DataType       = _RenderTargetTEX._DataType;
  renderTargetDesc._MinFilter      = GL_LINEAR;
  renderTargetDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(renderTargetDesc, _RenderTargetTEX);

  GLFrameBufferDesc renderTargetFBODesc;
  renderTargetFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_RenderTargetTEX });
  if (!GLUtil::CreateFrameBuffer(renderTargetFBODesc, _RenderTargetFBO))
    return 1;

  // Color buffer Texture
  GLTextureDesc colorBufferDesc;
  colorBufferDesc._Target         = _ColorBufferTEX._Target;
  colorBufferDesc._Slot           = _ColorBufferTEX._Slot;
  colorBufferDesc._Width          = RenderWidth();
  colorBufferDesc._Height         = RenderHeight();
  colorBufferDesc._InternalFormat = _ColorBufferTEX._InternalFormat;
  colorBufferDesc._DataFormat     = _ColorBufferTEX._DataFormat;
  colorBufferDesc._DataType       = _ColorBufferTEX._DataType;
  colorBufferDesc._MinFilter      = GL_LINEAR;
  colorBufferDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(colorBufferDesc, _ColorBufferTEX);

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("vertex_Default.glsl"));
  ShaderSource fragmentShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("fragment_drawTexture.glsl"));

  ShaderProgram* newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if (!newShader)
    return 1;
  _RenderToTextureShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("fragment_Postprocess.glsl"));
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if (!newShader)
    return 1;
  _RenderToScreenShader.reset(newShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UnloadScene()
{
  GLUtil::DeleteTEX(_EnvMapTEX);

  _FrameNum = 0;

  _CachedMeshInstanceCount = 0;
  _VertexBuffer.clear();
  _VertexSources.clear();
  _Triangles.clear();
  _InstanceRanges.clear();
  _ProjVerticesBuf.clear();
  for ( auto & rasterTriangles : _RasterTrianglesBuf )
    rasterTriangles.clear();
  for ( auto & fragments : _Fragments )
    fragments.clear();
  for ( auto & fragments : _TransparentFragments )
    fragments.clear();
  for ( auto & tile : _Tiles )
  {
    for ( auto & bin : tile._RasterTrisBins )
      bin.clear();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadScene
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ReloadScene()
{
  UnloadScene();

  const std::vector<MeshInstance> & meshInstances = _Scene.GetMeshInstances();
  const std::vector<Mesh*>        & meshes        = _Scene.GetMeshes();

  _CachedMeshInstanceCount = static_cast<int>(meshInstances.size());
  _CachedVisibleMeshInstanceCount = 0;
  _InstanceRanges.resize(meshInstances.size());
  _Stats._InputInstances = meshInstances.size();

  for ( int instID = 0; instID < static_cast<int>(meshInstances.size()); ++instID )
  {
    const MeshInstance & meshInst = meshInstances[instID];
    CompiledInstanceRange & instanceRange = _InstanceRanges[instID];
    instanceRange._MeshID = meshInst._MeshID;
    instanceRange._MaterialID = meshInst._MaterialID;
    instanceRange._Visible = meshInst._Visible;
    instanceRange._Transform = meshInst._Transform;
    instanceRange._VertexStart = static_cast<int>(_VertexBuffer.size());
    instanceRange._TriangleStart = static_cast<int>(_Triangles.size());
    if ( !meshInst._Visible )
      continue;

    _CachedVisibleMeshInstanceCount++;

    if ( ( meshInst._MeshID < 0 ) || ( meshInst._MeshID >= static_cast<int>(meshes.size()) ) )
      continue;

    Mesh * curMesh = meshes[meshInst._MeshID];
    if ( !curMesh || !curMesh -> GetNbFaces() )
      continue;

    const std::vector<Vec3>  & curVertices = curMesh -> GetVertices();
    const std::vector<Vec3>  & curNormals  = curMesh -> GetNormals();
    const std::vector<Vec2>  & curUVs      = curMesh -> GetUVs();
    const std::vector<Vec3i> & curIndices  = curMesh -> GetIndices();
    const Mat4x4 trInvTransfo = glm::transpose(glm::inverse(meshInst._Transform));

    std::unordered_map<rd::Vertex, int> VertexIDs;
    VertexIDs.reserve(curVertices.size());

    const int nbTris = static_cast<int>(curIndices.size() / 3);
    for ( int i = 0; i < nbTris; ++i )
    {
      rd::Triangle tri;

      Vec3i Index[3];
      Index[0] = curIndices[i * 3];
      Index[1] = curIndices[i * 3 + 1];
      Index[2] = curIndices[i * 3 + 2];

      rd::Vertex Vert[3];
      for ( int j = 0; j < 3; ++j )
      {
        Vec4 transformedVtx = meshInst._Transform * Vec4(curVertices[Index[j].x], 1.f);
        Vert[j]._WorldPos = Vec3(transformedVtx);

        if ( ( Index[j].z >= 0 ) && ( Index[j].z < static_cast<int>(curUVs.size()) ) )
          Vert[j]._UV = curUVs[Index[j].z];
        else
          Vert[j]._UV = Vec2(0.f);

        Vert[j]._Normal = Vec3(0.f);
      }

      const Vec3 vec1(Vert[1]._WorldPos - Vert[0]._WorldPos);
      const Vec3 vec2(Vert[2]._WorldPos - Vert[0]._WorldPos);
      tri._Normal = glm::normalize(glm::cross(vec1, vec2));

      for ( int j = 0; j < 3; ++j )
      {
        if ( ( Index[j].y >= 0 ) && ( Index[j].y < static_cast<int>(curNormals.size()) ) )
        {
          Vec4 transformedNormal = trInvTransfo * Vec4(curNormals[Index[j].y], 0.f);
          Vert[j]._Normal = glm::normalize(Vec3(transformedNormal));
        }
        else
          Vert[j]._Normal = tri._Normal;
      }

      tri._MatID = meshInst._MaterialID;

      for ( int j = 0; j < 3; ++j )
      {
        int idx = 0;
        if (0 == VertexIDs.count(Vert[j]))
        {
          idx = (int)_VertexBuffer.size();
          VertexIDs[Vert[j]] = idx;
          _VertexBuffer.push_back(Vert[j]);

          RasterSourceVertex sourceVertex;
          sourceVertex._MeshInstanceID = instID;
          sourceVertex._MeshID = meshInst._MeshID;
          sourceVertex._VertexID = Index[j].x;
          sourceVertex._NormalID = Index[j].y;
          _VertexSources.push_back(sourceVertex);
        }
        else
          idx = VertexIDs[Vert[j]];

        tri._Indices[j] = idx;
      }

      _Triangles.push_back(tri);
    }

    instanceRange._VertexCount = static_cast<int>(_VertexBuffer.size()) - instanceRange._VertexStart;
    instanceRange._TriangleCount = static_cast<int>(_Triangles.size()) - instanceRange._TriangleStart;
    UpdateInstanceBounds(instanceRange);
  }

  _Stats._VisibleInstances = _CachedVisibleMeshInstanceCount;
  _Stats._InputTriangles = _Triangles.size();
  _Stats._TransformedVertices = _VertexBuffer.size();
  _TriangleVisible.assign(_Triangles.size(), 1);

  this -> UpdateMipMaps();
  return 0;
}

// ----------------------------------------------------------------------------
// CanRefreshSceneInstanceTransforms
// ----------------------------------------------------------------------------
bool SoftwareRasterizer::CanRefreshSceneInstanceTransforms() const
{
  if ( _VertexSources.size() != _VertexBuffer.size() )
    return false;

  if ( _CachedMeshInstanceCount != _Scene.GetNbMeshInstances() )
    return false;

  if ( _InstanceRanges.size() != static_cast<size_t>(_CachedMeshInstanceCount) )
    return false;

  const std::vector<MeshInstance> & meshInstances = _Scene.GetMeshInstances();
  const std::vector<Mesh*>        & meshes        = _Scene.GetMeshes();

  int visibleMeshInstanceCount = 0;
  for ( const MeshInstance & meshInst : meshInstances )
  {
    if ( meshInst._Visible )
      visibleMeshInstanceCount++;
  }
  if ( visibleMeshInstanceCount != _CachedVisibleMeshInstanceCount )
    return false;

  for ( int instID = 0; instID < static_cast<int>(meshInstances.size()); ++instID )
  {
    const MeshInstance & meshInst = meshInstances[instID];
    const CompiledInstanceRange & instanceRange = _InstanceRanges[instID];
    if ( meshInst._Visible != instanceRange._Visible )
      return false;
    if ( meshInst._MeshID != instanceRange._MeshID )
      return false;
    if ( !meshInst._Visible )
      continue;
    if ( ( meshInst._MeshID < 0 ) || ( meshInst._MeshID >= static_cast<int>(meshes.size()) ) )
      return false;
    Mesh * curMesh = meshes[meshInst._MeshID];
    if ( !curMesh )
      return false;
  }

  return true;
}

// ----------------------------------------------------------------------------
// RefreshSceneInstanceTransforms
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RefreshSceneInstanceTransforms()
{
  const std::vector<MeshInstance> & meshInstances = _Scene.GetMeshInstances();
  const std::vector<Mesh*>        & meshes        = _Scene.GetMeshes();

  _Stats._ChangedInstances = 0;
  _Stats._RefreshedVertices = 0;
  _Stats._RefreshedTriangles = 0;

  for ( int instID = 0; instID < static_cast<int>(_InstanceRanges.size()); ++instID )
  {
    CompiledInstanceRange & instanceRange = _InstanceRanges[instID];
    const MeshInstance & meshInst = meshInstances[instID];
    const bool transformChanged = 0 != std::memcmp(&instanceRange._Transform, &meshInst._Transform, sizeof(Mat4x4));
    const bool materialChanged = instanceRange._MaterialID != meshInst._MaterialID;
    if ( !transformChanged && !materialChanged )
      continue;

    _Stats._ChangedInstances++;
    Mesh * curMesh = meshes[meshInst._MeshID];

    const std::vector<Vec3> & vertices = curMesh -> GetVertices();
    const std::vector<Vec3> & normals = curMesh -> GetNormals();
    const Mat4x4 trInvTransfo = transformChanged ? glm::transpose(glm::inverse(meshInst._Transform)) : Mat4x4(1.f);

    if ( transformChanged )
    {
      for ( int i = instanceRange._VertexStart; i < instanceRange._VertexStart + instanceRange._VertexCount; ++i )
      {
        const RasterSourceVertex & sourceVertex = _VertexSources[i];
        Vec4 transformedVtx = meshInst._Transform * Vec4(vertices[sourceVertex._VertexID], 1.f);
        _VertexBuffer[i]._WorldPos = Vec3(transformedVtx);
        if ( ( sourceVertex._NormalID >= 0 ) && ( sourceVertex._NormalID < static_cast<int>(normals.size()) ) )
        {
          Vec4 transformedNormal = trInvTransfo * Vec4(normals[sourceVertex._NormalID], 0.f);
          _VertexBuffer[i]._Normal = glm::normalize(Vec3(transformedNormal));
        }
      }
      _Stats._RefreshedVertices += instanceRange._VertexCount;
    }

    for ( int i = instanceRange._TriangleStart; i < instanceRange._TriangleStart + instanceRange._TriangleCount; ++i )
    {
      RasterData::Triangle & tri = _Triangles[i];
      if ( transformChanged )
      {
        const Vec3 & p0 = _VertexBuffer[tri._Indices[0]]._WorldPos;
        const Vec3 & p1 = _VertexBuffer[tri._Indices[1]]._WorldPos;
        const Vec3 & p2 = _VertexBuffer[tri._Indices[2]]._WorldPos;
        tri._Normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
      }
      if ( materialChanged )
        tri._MatID = meshInst._MaterialID;
    }
    _Stats._RefreshedTriangles += instanceRange._TriangleCount;
    instanceRange._Transform = meshInst._Transform;
    instanceRange._MaterialID = meshInst._MaterialID;
    if ( transformChanged )
      UpdateInstanceBounds(instanceRange);
  }

  _FrameNum = 0;

  return 0;
}

// ----------------------------------------------------------------------------
// RefreshAllSceneInstanceTransforms
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RefreshAllSceneInstanceTransforms()
{
  const std::vector<MeshInstance> & meshInstances = _Scene.GetMeshInstances();
  const std::vector<Mesh*> & meshes = _Scene.GetMeshes();
  _Stats._ChangedInstances = _CachedVisibleMeshInstanceCount;
  _Stats._RefreshedVertices = _VertexBuffer.size();
  _Stats._RefreshedTriangles = _Triangles.size();

  for ( int i = 0; i < static_cast<int>(_VertexBuffer.size()); ++i )
  {
    const RasterSourceVertex & sourceVertex = _VertexSources[i];
    const MeshInstance & meshInst = meshInstances[sourceVertex._MeshInstanceID];
    Mesh * mesh = meshes[sourceVertex._MeshID];
    const std::vector<Vec3> & vertices = mesh -> GetVertices();
    const std::vector<Vec3> & normals = mesh -> GetNormals();
    _VertexBuffer[i]._WorldPos = Vec3(meshInst._Transform * Vec4(vertices[sourceVertex._VertexID], 1.f));
    if ( sourceVertex._NormalID >= 0 && sourceVertex._NormalID < static_cast<int>(normals.size()) )
    {
      const Mat4x4 inverseTranspose = glm::transpose(glm::inverse(meshInst._Transform));
      _VertexBuffer[i]._Normal = glm::normalize(Vec3(inverseTranspose * Vec4(normals[sourceVertex._NormalID], 0.f)));
    }
  }

  for ( RasterData::Triangle & triangle : _Triangles )
  {
    const Vec3 & p0 = _VertexBuffer[triangle._Indices[0]]._WorldPos;
    const Vec3 & p1 = _VertexBuffer[triangle._Indices[1]]._WorldPos;
    const Vec3 & p2 = _VertexBuffer[triangle._Indices[2]]._WorldPos;
    triangle._Normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
  }
  for ( int instanceID = 0; instanceID < static_cast<int>(_InstanceRanges.size()); ++instanceID )
  {
    CompiledInstanceRange & range = _InstanceRanges[instanceID];
    range._Transform = meshInstances[instanceID]._Transform;
    range._MaterialID = meshInstances[instanceID]._MaterialID;
    if ( range._Visible )
      UpdateInstanceBounds(range);
  }
  _FrameNum = 0;
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateInstanceBounds
// ----------------------------------------------------------------------------
void SoftwareRasterizer::UpdateInstanceBounds( CompiledInstanceRange & ioRange )
{
  ioRange._WorldBounds = AABB<Vec3>();
  for ( int i = ioRange._VertexStart; i < ioRange._VertexStart + ioRange._VertexCount; ++i )
    ioRange._WorldBounds.Insert(_VertexBuffer[i]._WorldPos);
}

// ----------------------------------------------------------------------------
// IsInstanceVisible
// ----------------------------------------------------------------------------
bool SoftwareRasterizer::IsInstanceVisible( const CompiledInstanceRange & iRange, const Mat4x4 & iViewProjection ) const
{
  if ( !_EnableFrustumCulling || !iRange._Visible || !iRange._VertexCount )
    return iRange._Visible;
  if ( !std::isfinite(iRange._WorldBounds._Low.x) || !std::isfinite(iRange._WorldBounds._High.x) )
    return true;

  Vec3 corners[8];
  iRange._WorldBounds.Corners(corners);
  bool outside[6] = { true, true, true, true, true, true };
  for ( const Vec3 & corner : corners )
  {
    const Vec4 clip = iViewProjection * Vec4(corner, 1.f);
    outside[0] = outside[0] && ( clip.x < -clip.w );
    outside[1] = outside[1] && ( clip.x >  clip.w );
    outside[2] = outside[2] && ( clip.y < -clip.w );
    outside[3] = outside[3] && ( clip.y >  clip.w );
    outside[4] = outside[4] && ( clip.z < -clip.w );
    outside[5] = outside[5] && ( clip.z >  clip.w );
  }
  for ( bool planeOutside : outside )
  {
    if ( planeOutside )
      return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
// UpdateMipMaps
// ----------------------------------------------------------------------------
void SoftwareRasterizer::UpdateMipMaps()
{
  const auto & textures = _Scene.GetTextures();
  for (auto * tex : textures)
  {
    if ( tex )
    {
      if ( _Settings._GenerateMipMaps )
        tex -> GenerateMipMaps();
      else
        tex -> ClearMipMaps();
    }
  }
}

// ----------------------------------------------------------------------------
// SetGenerateMipMaps
// ----------------------------------------------------------------------------
void SoftwareRasterizer::SetGenerateMipMaps(bool iGenerate)
{
  if (_Settings._GenerateMipMaps == iGenerate)
    return;

  _Settings._GenerateMipMaps = iGenerate;

  this -> UpdateMipMaps();
}

// ----------------------------------------------------------------------------
// ResizeTileMap
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ResizeTileMap()
{
  _TileCountX = (RenderWidth() + _TileSize - 1) / _TileSize;
  _TileCountY = (RenderHeight() + _TileSize - 1) / _TileSize;
  _Tiles.resize(_TileCountX * _TileCountY);

  for (int ty = 0; ty < _TileCountY; ++ty)
  {
    for (int tx = 0; tx < _TileCountX; ++tx)
    {
      int tileIndex = ty * _TileCountX + tx;
      rd::Tile& curTile = _Tiles[tileIndex];

      curTile._X = tx * _TileSize;
      curTile._Y = ty * _TileSize;
      curTile._Width = std::min(static_cast<int>(_TileSize), RenderWidth() - curTile._X);
      curTile._Height = std::min(static_cast<int>(_TileSize), RenderHeight() - curTile._Y);

      curTile._LocalFB._ColorBuffer.resize(curTile._Width * curTile._Height);
      curTile._LocalFB._DepthBuffer.resize(curTile._Width * curTile._Height);

      curTile._Fragments.clear();
      curTile._Fragments.resize(curTile._Width * curTile._Height);
      for ( int y = 0; y < curTile._Height; ++y )
      {
        for ( int x = 0; x < curTile._Width; ++x )
        {
          int pixelIndex = y * curTile._Width + x;
          curTile._Fragments[pixelIndex]._PixelCoords = Vec2i(curTile._X + x, curTile._Y + y);
        }
      }

      curTile._CoveredPixels.clear();
      curTile._CoveredPixels.resize(curTile._Width * curTile._Height);
      std::fill(policy, curTile._CoveredPixels.begin(), curTile._CoveredPixels.end(), false);

      curTile._CompactHits.clear();
      curTile._CompactHits.resize(curTile._Width * curTile._Height);
      curTile._CompactHitGenerations.assign(curTile._Width * curTile._Height, 0);
      curTile._CoveredIndices.clear();
      curTile._CoveredIndices.reserve(curTile._Width * curTile._Height);
      curTile._TransparentHits.clear();
      curTile._TransparentHits.reserve(curTile._Width * curTile._Height / 4);
      curTile._CompactGeneration = 1;

      if (_NbJobs)
        curTile._RasterTrisBins.resize(_NbJobs);
    }
  }
}

// ----------------------------------------------------------------------------
// SetTileSize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::SetTileSize( unsigned int iTileSize )
{
  if ( !iTileSize || ( iTileSize % 8 ) != 0 ) // Must be a multiple of 8
    return 1;

  _TileSize = iTileSize;

  ResizeTileMap();

  return 0;
}

// ----------------------------------------------------------------------------
// ResetTiles
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ResetTiles()
{
  if (!TiledRendering())
    return;

  for (auto& tile : _Tiles)
  {
    tile._BinnedTriangles = 0;
    tile._DepthWins = 0;
    tile._CoveredCount = 0;
    tile._ShadedCount = 0;
    tile._MaskedTested = 0;
    tile._MaskedRejected = 0;
    tile._TransparentShaded = 0;
    tile._TransparentHits.clear();
    for (auto& bin : tile._RasterTrisBins)
    {
      bin.clear();
      //bin.reserve(100);
    }

    //tile._Fragments.clear();
    //tile._Fragments.reserve(tile._Width * tile._Height);

    if ( _EnableCompactHits )
    {
      ++tile._CompactGeneration;
      if ( 0 == tile._CompactGeneration )
      {
        std::fill(policy, tile._CompactHitGenerations.begin(), tile._CompactHitGenerations.end(), 0);
        tile._CompactGeneration = 1;
      }
      tile._CoveredIndices.clear();
    }
    else
      std::fill(policy, tile._CoveredPixels.begin(), tile._CoveredPixels.end(), false);

    if ( !_EnableDirectColorWrites || !_EnableCompactHits )
      std::fill(policy, tile._LocalFB._ColorBuffer.begin(), tile._LocalFB._ColorBuffer.end(), S_DefaultColor);
    std::fill(policy, tile._LocalFB._DepthBuffer.begin(), tile._LocalFB._DepthBuffer.end(), MAX_FLOAT);
  }

  _Stats._HitBufferBytes = 0;
  for ( const auto & tile : _Tiles )
  {
    _Stats._HitBufferBytes += _EnableCompactHits
      ? tile._CompactHits.size() * sizeof(rd::CompactHit) +
        tile._CompactHitGenerations.size() * sizeof(unsigned int) +
        tile._CoveredIndices.capacity() * sizeof(unsigned int)
      : tile._Fragments.size() * sizeof(rd::Fragment) + tile._CoveredPixels.size() / 8;
  }
}

//-----------------------------------------------------------------------------
// CopyTileToMainBuffer
//-----------------------------------------------------------------------------
void SoftwareRasterizer::CopyTileToMainBuffer(const RasterData::Tile& iTile)
{
  if (_EnableSIMD)
  {
#if defined(SIMD_AVX2)
    CopyTileToMainBuffer8x(iTile);
#elif defined(SIMD_ARM_NEON)
    CopyTileToMainBuffer4x(iTile);
#else
    CopyTileToMainBuffer1x(iTile);
#endif
  }
  else
  {
    CopyTileToMainBuffer1x(iTile);
  }
}

//-----------------------------------------------------------------------------
// CopyTileToMainBuffer1x
//-----------------------------------------------------------------------------
void SoftwareRasterizer::CopyTileToMainBuffer1x(const RasterData::Tile& iTile)
{
  for (int y = 0; y < iTile._Height; ++y)
  {
    const int globalY = iTile._Y + y;
    const int localRowStart = y * iTile._Width;
    const int globalRowStart = globalY * RenderWidth() + iTile._X;

    memcpy(
      &_ImageBuffer._ColorBuffer[globalRowStart],
      &iTile._LocalFB._ColorBuffer[localRowStart],
      iTile._Width * sizeof(RGBA8)
    );
  }
}

#ifdef SIMD_ARM_NEON
//-----------------------------------------------------------------------------
// CopyTileToMainBuffer4x
//-----------------------------------------------------------------------------
void SoftwareRasterizer::CopyTileToMainBuffer4x(const RasterData::Tile& iTile)
{
  for (int y = 0; y < iTile._Height; ++y)
  {
    const int globalY = iTile._Y + y;
    const int localRowStart = y * iTile._Width;
    const int globalRowStart = globalY * RenderWidth() + iTile._X;

    int x = 0;
    for (; x + 4 <= iTile._Width; x += 4)
    {
      uint32x4_t colors = vld1q_u32((uint32_t*)&iTile._LocalFB._ColorBuffer[localRowStart + x]);
      //float32x4_t depths = vld1q_dup_f32(&iTile._LocalFB._DepthBuffer[localRowStart + x]);

      vst1q_u32((uint32_t*)&_ImageBuffer._ColorBuffer[globalRowStart + x], colors);
      //vst1q_f32(&_ImageBuffer._DepthBuffer[globalRowStart + x], depths);
    }

    for (; x < iTile._Width; ++x)
    {
      _ImageBuffer._ColorBuffer[globalRowStart + x] = iTile._LocalFB._ColorBuffer[localRowStart + x];
      //_ImageBuffer._DepthBuffer[globalRowStart + x] = iTile._LocalFB._DepthBuffer[localRowStart + x];
    }
  }
}
#endif

#ifdef SIMD_AVX2
//-----------------------------------------------------------------------------
// CopyTileToMainBuffer8x
//-----------------------------------------------------------------------------
void SoftwareRasterizer::CopyTileToMainBuffer8x(const RasterData::Tile& iTile)
{
  for (int y = 0; y < iTile._Height; ++y)
  {
    const int globalY = iTile._Y + y;
    const int localRowStart = y * iTile._Width;
    const int globalRowStart = globalY * RenderWidth() + iTile._X;

    int x = 0;
    for (; x + 8 <= iTile._Width; x += 8)
    {
      __m256i colors = _mm256_load_si256((__m256i*) & iTile._LocalFB._ColorBuffer[localRowStart + x]);
      //__m256 depths = _mm256_load_ps(&iTile._LocalFB._DepthBuffer[localRowStart + x]);

      _mm256_storeu_si256((__m256i*) & _ImageBuffer._ColorBuffer[globalRowStart + x], colors);
      //_mm256_storeu_ps(&_ImageBuffer._DepthBuffer[globalRowStart + x], depths);
    }

    for (; x < iTile._Width; ++x)
    {
      _ImageBuffer._ColorBuffer[globalRowStart + x] = iTile._LocalFB._ColorBuffer[localRowStart + x];
      //_ImageBuffer._DepthBuffer[globalRowStart + x] = iTile._LocalFB._DepthBuffer[localRowStart + x];
    }
  }
}
#endif

// ----------------------------------------------------------------------------
// ReloadEnvMap
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ReloadEnvMap()
{
  GLUtil::DeleteTEX(_EnvMapTEX);

  if (_Scene.GetEnvMap().IsInitialized())
  {
    GLTextureDesc envDesc;
    envDesc._Target         = _EnvMapTEX._Target;
    envDesc._Slot           = _EnvMapTEX._Slot;
    envDesc._Width          = _Scene.GetEnvMap().GetWidth();
    envDesc._Height         = _Scene.GetEnvMap().GetHeight();
    envDesc._InternalFormat = _EnvMapTEX._InternalFormat;
    envDesc._DataFormat     = _EnvMapTEX._DataFormat;
    envDesc._DataType       = _EnvMapTEX._DataType;
    envDesc._Data           = _Scene.GetEnvMap().GetRawData();
    envDesc._MinFilter      = GL_LINEAR;
    envDesc._MagFilter      = GL_LINEAR;
    GLUtil::CreateTexture(envDesc, _EnvMapTEX);

    _Scene.GetEnvMap().SetHandle(_EnvMapTEX._Handle);
  }
  else
    _Settings._EnableSkybox = false;

  return 0;
}

// ----------------------------------------------------------------------------
// SampleEnvMap
// ----------------------------------------------------------------------------
Vec4 SoftwareRasterizer::SampleEnvMap(const Vec3& iDir)
{
  if ((_Scene.GetEnvMap().IsInitialized()))
  {
    float theta = std::asin(iDir.y);
    float phi = std::atan2(iDir.z, iDir.x);
    Vec2 uv = Vec2(.5f + phi * M_1_PI * .5f, .5f - theta * M_1_PI) + Vec2(_Settings._SkyBoxRotation, 0.0);

    if (_Settings._Sampling >= SamplingMode::Bilinear)
      return _Scene.GetEnvMap().BiLinearSample(uv);
    else
      return _Scene.GetEnvMap().Sample(uv);
  }

  return Vec4(0.f);
}

// ----------------------------------------------------------------------------
// RenderBackground
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderBackground(float iTop, float iRight)
{
  int width = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  if (_Settings._EnableBackGround)
  {
    Vec3 bottomLeft = _Scene.GetCamera().GetForward() * zNear - iRight * _Scene.GetCamera().GetRight() - iTop * _Scene.GetCamera().GetUp();
    Vec3 dX = _Scene.GetCamera().GetRight() * (2 * iRight / width);
    Vec3 dY = _Scene.GetCamera().GetUp() * (2 * iTop / height);

    if (TiledRendering())
    {
      for ( auto & tile : _Tiles )
        JobSystem::Get().Execute([this, bottomLeft, dX, dY, &tile]() { this->RenderBackground(bottomLeft, dX, dY, tile); });
      JobSystem::Get().Wait();
      _Stats._TileJobs += _Tiles.size();
    }
    else
    {
      for ( int y = 0; y < height; ++y )
        JobSystem::Get().Execute([this, y, bottomLeft, dX, dY]() { this->RenderBackgroundRows(y, y + 1, bottomLeft, dX, dY); });
      JobSystem::Get().Wait();
    }
  }
  else
  {
    const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
    std::fill(policy, _ImageBuffer._ColorBuffer.begin(), _ImageBuffer._ColorBuffer.end(), backgroundColor);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderUncoveredBackground
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderUncoveredBackground(float iTop, float iRight)
{
  const int width = _Settings._RenderResolution.x;
  const int height = _Settings._RenderResolution.y;
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);
  const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
  const Vec3 bottomLeft = _Scene.GetCamera().GetForward() * zNear - iRight * _Scene.GetCamera().GetRight() - iTop * _Scene.GetCamera().GetUp();
  const Vec3 dX = _Scene.GetCamera().GetRight() * (2 * iRight / width);
  const Vec3 dY = _Scene.GetCamera().GetUp() * (2 * iTop / height);

  const auto renderTiles = [this, bottomLeft, dX, dY, backgroundColor](unsigned int iBegin, unsigned int iEnd) {
    for ( unsigned int tileIndex = iBegin; tileIndex < iEnd; ++tileIndex )
    {
      const rd::Tile & tile = _Tiles[tileIndex];
      for ( int y = 0; y < tile._Height; ++y )
      {
        const int globalY = tile._Y + y;
        for ( int x = 0; x < tile._Width; ++x )
        {
          const unsigned int localPixelIndex = y * tile._Width + x;
          if ( tile._CompactHitGenerations[localPixelIndex] == tile._CompactGeneration )
            continue;
          const int globalX = tile._X + x;
          if ( _Settings._EnableBackGround )
          {
            const Vec3 worldP = glm::normalize(bottomLeft + dX * static_cast<float>(globalX) + dY * static_cast<float>(globalY));
            _ImageBuffer._ColorBuffer[globalX + RenderWidth() * globalY] = SampleEnvMap(worldP);
          }
          else
            _ImageBuffer._ColorBuffer[globalX + RenderWidth() * globalY] = backgroundColor;
        }
      }
    }
  };
  for ( unsigned int i = 0; i < _Tiles.size(); ++i )
    JobSystem::Get().Execute([&renderTiles, i]() { renderTiles(i, i + 1); });
  JobSystem::Get().Wait();
  _Stats._TileJobs += _Tiles.size();
  return 0;
}

// ----------------------------------------------------------------------------
// RenderBackgroundRows
// ----------------------------------------------------------------------------
void SoftwareRasterizer::RenderBackgroundRows(int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY)
{
  int width = _Settings._RenderResolution.x;

  for (int y = iStartY; y < iEndY; ++y)
  {
    for (int x = 0; x < width; ++x)
    {
      Vec3 worldP = glm::normalize(iBottomLeft + iDX * (float)x + iDY * (float)y);
      _ImageBuffer._ColorBuffer[x + width * y] = this->SampleEnvMap(worldP);
    }
  }
}

// ----------------------------------------------------------------------------
// RenderBackground
// ----------------------------------------------------------------------------
void SoftwareRasterizer::RenderBackground(Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY, RasterData::Tile& ioTile)
{
  // Precompute offsets to avoid repeated calculation
  const int tileX = ioTile._X;
  const int tileY = ioTile._Y;
  const int width = ioTile._Width;
  const int height = ioTile._Height;
  Vec3 base = iBottomLeft + iDX * (float)tileX + iDY * (float)tileY;

  for (int y = 0; y < height; ++y)
  {
    Vec3 rowStart = base + iDY * (float)y;
    for (int x = 0; x < width; ++x)
    {
      Vec3 worldP = glm::normalize(rowStart + iDX * (float)x);
      ioTile._LocalFB._ColorBuffer[x + width * y] = SampleEnvMap(worldP);
    }
  }
}

// ----------------------------------------------------------------------------
// RenderScene
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderScene()
{
  const double sceneStartTime = glfwGetTime();
  _PassEnabled[TimingRenderScene] = true;
  _PassEnabled[TimingProcessVertices] = true;
  _PassEnabled[TimingClipTriangles] = false;
  _PassEnabled[TimingRasterize] = false;
  _PassEnabled[TimingProcessFragments] = false;
  _PassTimes[TimingClipTriangles] = 0.;
  _PassTimes[TimingRasterize] = 0.;
  _PassTimes[TimingProcessFragments] = 0.;

  const double verticesStartTime = glfwGetTime();
  int ko = ProcessVertices();
  _PassTimes[TimingProcessVertices] = glfwGetTime() - verticesStartTime;

  if (!ko)
  {
    Mat4x4 RasterM;
    _Scene.GetCamera().ComputeRasterMatrix(RenderWidth(), RenderHeight(), RasterM);

    _PassEnabled[TimingClipTriangles] = true;
    const double clipStartTime = glfwGetTime();
    ko = ClipTriangles(RasterM);
    _PassTimes[TimingClipTriangles] = glfwGetTime() - clipStartTime;
  }

  if (!ko)
  {
    _PassEnabled[TimingRasterize] = true;
    const double rasterStartTime = glfwGetTime();
    ko = Rasterize();
    _PassTimes[TimingRasterize] = glfwGetTime() - rasterStartTime;
  }

  if (!ko)
  {
    _PassEnabled[TimingProcessFragments] = true;
    const double fragmentsStartTime = glfwGetTime();
    ko = ProcessFragments();
    _PassTimes[TimingProcessFragments] = glfwGetTime() - fragmentsStartTime;
  }

  _PassTimes[TimingRenderScene] = glfwGetTime() - sceneStartTime;

  return ko;
}

// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ProcessVertices()
{
  Mat4x4 M(1.f);

  Mat4x4 V;
  _Scene.GetCamera().ComputeLookAtMatrix(V);

  float ratio = RenderWidth() / float(RenderHeight());
  float top, right;
  Mat4x4 P;
  _Scene.GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  int nbVertices = static_cast<int>(_VertexBuffer.size());
  _ProjVerticesBuf.resize(nbVertices);
  _ProjVerticesBuf.reserve(nbVertices * 2);
  _VisibleInstanceRanges.clear();
  _TriangleVisible.assign(_Triangles.size(), 0);
  _Stats._VisibleInstances = 0;
  _Stats._RejectedInstances = 0;
  _Stats._AvoidedVertices = 0;
  _Stats._AvoidedTriangles = 0;
  _Stats._TransformedVertices = 0;
  const Mat4x4 viewProjection = P * V;
  for ( int instanceID = 0; instanceID < static_cast<int>(_InstanceRanges.size()); ++instanceID )
  {
    const CompiledInstanceRange & range = _InstanceRanges[instanceID];
    if ( !range._Visible )
      continue;
    if ( IsInstanceVisible(range, viewProjection) )
    {
      _VisibleInstanceRanges.push_back(instanceID);
      _Stats._VisibleInstances++;
      _Stats._TransformedVertices += range._VertexCount;
      for ( int triangle = range._TriangleStart; triangle < range._TriangleStart + range._TriangleCount; ++triangle )
        _TriangleVisible[triangle] = 1;
    }
    else
    {
      _Stats._RejectedInstances++;
      _Stats._AvoidedVertices += range._VertexCount;
      _Stats._AvoidedTriangles += range._TriangleCount;
    }
  }

  const auto processRanges = [this, M, V, P](unsigned int iBegin, unsigned int iEnd) {
    for ( unsigned int rangeIndex = iBegin; rangeIndex < iEnd; ++rangeIndex )
    {
      const CompiledInstanceRange & range = _InstanceRanges[_VisibleInstanceRanges[rangeIndex]];
      const int vertexBegin = range._VertexStart;
      const int vertexEnd = range._VertexStart + range._VertexCount;
      if (_EnableSIMD)
      {
#if defined(SIMD_AVX2)
        this->ProcessVerticesAVX2(M, V, P, vertexBegin, vertexEnd);
#elif defined(SIMD_ARM_NEON)
        this->ProcessVerticesARM(M, V, P, vertexBegin, vertexEnd);
#else
        this->ProcessVertices(M, V, P, vertexBegin, vertexEnd);
#endif
      }
      else
        this->ProcessVertices(M, V, P, vertexBegin, vertexEnd);
    }
  };

  if ( _EnableFrustumCulling )
  {
    const unsigned int rangeCount = static_cast<unsigned int>(_VisibleInstanceRanges.size());
    const unsigned int workerCount = std::max(1u, std::min(JobSystem::Get().GetThreadCount(), rangeCount));
    const unsigned int verticesPerJob = std::max(1u,
      static_cast<unsigned int>((_Stats._TransformedVertices + workerCount - 1) / workerCount));
    unsigned int rangeBegin = 0;
    unsigned int verticesInJob = 0;
    unsigned int jobsRemaining = workerCount;
    for ( unsigned int rangeIndex = 0; rangeIndex < rangeCount; ++rangeIndex )
    {
      verticesInJob += _InstanceRanges[_VisibleInstanceRanges[rangeIndex]]._VertexCount;
      if ( jobsRemaining > 1 && verticesInJob >= verticesPerJob )
      {
        const unsigned int rangeEnd = rangeIndex + 1;
        JobSystem::Get().Execute([&processRanges, rangeBegin, rangeEnd]() {
          processRanges(rangeBegin, rangeEnd);
        });
        rangeBegin = rangeEnd;
        verticesInJob = 0;
        jobsRemaining--;
      }
    }
    if ( rangeBegin < rangeCount )
      JobSystem::Get().Execute([&processRanges, rangeBegin, rangeCount]() {
        processRanges(rangeBegin, rangeCount);
      });
    JobSystem::Get().Wait();
  }
  else
  {
    for ( int vertexBegin = 0; vertexBegin < nbVertices; vertexBegin += 512 )
    {
      const int vertexEnd = std::min(nbVertices, vertexBegin + 512);
      JobSystem::Get().Execute([this, M, V, P, vertexBegin, vertexEnd]() {
        if ( _EnableSIMD )
        {
#if defined(SIMD_AVX2)
          this->ProcessVerticesAVX2(M, V, P, vertexBegin, vertexEnd);
#elif defined(SIMD_ARM_NEON)
          this->ProcessVerticesARM(M, V, P, vertexBegin, vertexEnd);
#else
          this->ProcessVertices(M, V, P, vertexBegin, vertexEnd);
#endif
        }
        else
          this->ProcessVertices(M, V, P, vertexBegin, vertexEnd);
      });
    }
    JobSystem::Get().Wait();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessVertices(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd)
{
  DefaultVertexShader vertexShader(iM, iV, iP);

  for (int i = iStartInd; i < iEndInd; ++i)
  {
    vertexShader.Process(_VertexBuffer[i], _ProjVerticesBuf[i]);
  }
}

#ifdef SIMD_AVX2
// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessVerticesAVX2(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd)
{
  DefaultVertexShaderAVX2 vertexShader(iM, iV, iP);

  for (int i = iStartInd; i < iEndInd; ++i)
  {
    vertexShader.Process(_VertexBuffer[i], _ProjVerticesBuf[i]);
  }
}
#endif

#ifdef SIMD_ARM_NEON
// ----------------------------------------------------------------------------
// ProcessVerticesARM
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessVerticesARM(const Mat4x4& iM, const Mat4x4& iV, const Mat4x4& iP, int iStartInd, int iEndInd)
{
  DefaultVertexShaderARM vertexShader(iM, iV, iP);

  for (int i = iStartInd; i < iEndInd; ++i)
  {
    vertexShader.Process(_VertexBuffer[i], _ProjVerticesBuf[i]);
  }
}
#endif

// ----------------------------------------------------------------------------
// ClipTriangles
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ClipTriangles(const Mat4x4& iRasterM)
{
  int nbTriangles = static_cast<int>(_Triangles.size());

  for ( unsigned int i = 0; i < _NbJobs; ++i )
    _RasterTrianglesBuf[i].clear();

  for (unsigned int i = 0; i < _NbJobs; ++i)
  {
    int startInd = (nbTriangles / _NbJobs) * i;
    int endInd = (i == _NbJobs - 1) ? (nbTriangles) : (startInd + (nbTriangles / _NbJobs));
    if (startInd >= endInd)
      continue;

    JobSystem::Get().Execute([this, iRasterM, i, startInd, endInd]() { this->ClipTriangles(iRasterM, i, startInd, endInd); });
  }

  JobSystem::Get().Wait();

  _Stats._ClippedTriangles = 0;
  for ( const auto & rasterTriangles : _RasterTrianglesBuf )
    _Stats._ClippedTriangles += rasterTriangles.size();

  return 0;
}

// ----------------------------------------------------------------------------
// ClipTriangles
// SutherlandHodgman algorithm
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ClipTriangles(const Mat4x4& iRasterM, int iThreadBin, int iStartInd, int iEndInd)
{
  if ( _RasterTrianglesBuf[iThreadBin].capacity() < static_cast<size_t>(iEndInd - iStartInd) )
    _RasterTrianglesBuf[iThreadBin].reserve(iEndInd - iStartInd);

  for (int i = iStartInd; i < iEndInd; ++i)
  {
    if ( i < static_cast<int>(_TriangleVisible.size()) && !_TriangleVisible[i] )
      continue;
    rd::Triangle& tri = _Triangles[i];

    uint32_t clipCode0 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[0]]._ProjPos);
    uint32_t clipCode1 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[1]]._ProjPos);
    uint32_t clipCode2 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[2]]._ProjPos);

    if (clipCode0 | clipCode1 | clipCode2)
    {
      // Check the clipping codes correctness
      if (!(clipCode0 & clipCode1 & clipCode2))
      {
        Polygon poly = SutherlandHodgman::ClipTriangle(
          _ProjVerticesBuf[tri._Indices[0]]._ProjPos,
          _ProjVerticesBuf[tri._Indices[1]]._ProjPos,
          _ProjVerticesBuf[tri._Indices[2]]._ProjPos,
          (clipCode0 ^ clipCode1) | (clipCode1 ^ clipCode2) | (clipCode2 ^ clipCode0));

        for (int j = 2; j < poly.Size(); ++j)
        {
          // Preserve winding
          Polygon::Point Points[3] = { poly[0], poly[j - 1], poly[j] };

          rd::RasterTriangle rasterTri;
          for (int k = 0; k < 3; ++k)
          {
            if (Points[k]._Distances.x == 1.f)
            {
              rasterTri._Indices[k] = tri._Indices[0]; // == V0
            }
            else if (Points[k]._Distances.y == 1.f)
            {
              rasterTri._Indices[k] = tri._Indices[1]; // == V1
            }
            else if (Points[k]._Distances.z == 1.f)
            {
              rasterTri._Indices[k] = tri._Indices[2]; // == V2
            }
            else
            {
              rd::ProjectedVertex newProjVert;
              newProjVert._ProjPos = Points[k]._Pos;
              newProjVert._Attrib = _ProjVerticesBuf[tri._Indices[0]]._Attrib * Points[k]._Distances.x +
                _ProjVerticesBuf[tri._Indices[1]]._Attrib * Points[k]._Distances.y +
                _ProjVerticesBuf[tri._Indices[2]]._Attrib * Points[k]._Distances.z;
              {
                std::unique_lock<std::mutex> lock(_ProjVerticesMutex);
                rasterTri._Indices[k] = static_cast<int>(_ProjVerticesBuf.size());
                _ProjVerticesBuf.emplace_back(newProjVert);
              }
            }

            Vec3 homogeneousProjPos; // NDC space
            rasterTri._InvW[k] = 1.f / Points[k]._Pos.w;
            homogeneousProjPos.x = Points[k]._Pos.x * rasterTri._InvW[k];
            homogeneousProjPos.y = Points[k]._Pos.y * rasterTri._InvW[k];
            homogeneousProjPos.z = Points[k]._Pos.z * rasterTri._InvW[k];

            rasterTri._V[k] = MathUtil::TransformPoint(homogeneousProjPos, iRasterM); // to screen space

            rasterTri._BBox.Insert(rasterTri._V[k]);
          }

          if (!MathUtil::EdgeFunctionCoefficients(rasterTri._V[0], rasterTri._V[1], rasterTri._V[2], rasterTri._EdgeA, rasterTri._EdgeB, rasterTri._EdgeC, rasterTri._InvArea))
            continue;

          if (rasterTri._InvArea < 0.f)
            continue;

          rasterTri._MatID = tri._MatID;
          rasterTri._Normal = tri._Normal;

          this -> ComputeLOD(rasterTri);

          _RasterTrianglesBuf[iThreadBin].emplace_back(std::move(rasterTri));
        }
      }
    }
    else
    {
      // No clipping needed
      rd::RasterTriangle rasterTri;
      for (int j = 0; j < 3; ++j)
      {
        rasterTri._Indices[j] = tri._Indices[j];

        rd::ProjectedVertex& projVert = _ProjVerticesBuf[tri._Indices[j]];

        Vec3 homogeneousProjPos; // NDC space
        rasterTri._InvW[j] = 1.f / projVert._ProjPos.w;
        homogeneousProjPos.x = projVert._ProjPos.x * rasterTri._InvW[j];
        homogeneousProjPos.y = projVert._ProjPos.y * rasterTri._InvW[j];
        homogeneousProjPos.z = projVert._ProjPos.z * rasterTri._InvW[j];

        rasterTri._V[j] = MathUtil::TransformPoint(homogeneousProjPos, iRasterM); // to screen space

        rasterTri._BBox.Insert(rasterTri._V[j]);
      }

      if (!MathUtil::EdgeFunctionCoefficients(rasterTri._V[0], rasterTri._V[1], rasterTri._V[2], rasterTri._EdgeA, rasterTri._EdgeB, rasterTri._EdgeC, rasterTri._InvArea))
        continue;

      if (rasterTri._InvArea < 0.f)
        continue;

      rasterTri._MatID = tri._MatID;
      rasterTri._Normal = tri._Normal;

      const rd::Varying & v0 = _ProjVerticesBuf[tri._Indices[0]]._Attrib;
      const rd::Varying & v1 = _ProjVerticesBuf[tri._Indices[1]]._Attrib;
      const rd::Varying & v2 = _ProjVerticesBuf[tri._Indices[2]]._Attrib;
      Vec3 dp1 = v1._WorldPos - v0._WorldPos;
      Vec3 dp2 = v2._WorldPos - v0._WorldPos;
      Vec2 duv1 = v1._UV - v0._UV;
      Vec2 duv2 = v2._UV - v0._UV;
      float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
      if ( abs(determinant) > EPSILON )
      {
        rasterTri._Tangent = glm::normalize((dp1 * duv2.y - dp2 * duv1.y) / determinant);
        rasterTri._Bitangent = glm::normalize(glm::cross(rasterTri._Normal, rasterTri._Tangent)) * glm::sign(determinant);
      }
      else
      {
        Vec3 up = ( abs(rasterTri._Normal.z) < 0.999f ) ? Vec3(0.f, 0.f, 1.f) : Vec3(1.f, 0.f, 0.f);
        rasterTri._Tangent = glm::normalize(glm::cross(up, rasterTri._Normal));
        rasterTri._Bitangent = glm::cross(rasterTri._Normal, rasterTri._Tangent);
      }

      this -> ComputeLOD(rasterTri);

      _RasterTrianglesBuf[iThreadBin].emplace_back(std::move(rasterTri));
    }
  }
}

// ----------------------------------------------------------------------------
// ComputeLOD
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ComputeLOD( RasterData::RasterTriangle & ioRasterTri )
{
  ioRasterTri._LOD = 0.f;

  if ( ( ioRasterTri._MatID >= 0 ) && ( ioRasterTri._MatID < (int)_Scene.GetMaterials().size() ) )
  {
    const auto & mat = _Scene.GetMaterials()[ioRasterTri._MatID];
    if ( mat._BaseColorTexId >= 0 )
    {
      const Texture * tex = _Scene.GetTextures()[(unsigned int)mat._BaseColorTexId];
      if ( tex )
      {
        // get UVs for the three vertices (works for both original or newly created proj verts)
        Vec2 uv0 = _ProjVerticesBuf[ioRasterTri._Indices[0]]._Attrib._UV;
        Vec2 uv1 = _ProjVerticesBuf[ioRasterTri._Indices[1]]._Attrib._UV;
        Vec2 uv2 = _ProjVerticesBuf[ioRasterTri._Indices[2]]._Attrib._UV;

        // screen-space positions
        Vec2 p0 = Vec2(ioRasterTri._V[0].x, ioRasterTri._V[0].y);
        Vec2 p1 = Vec2(ioRasterTri._V[1].x, ioRasterTri._V[1].y);
        Vec2 p2 = Vec2(ioRasterTri._V[2].x, ioRasterTri._V[2].y);

        float dUdx = 0.f, dUdy = 0.f, dVdx = 0.f, dVdy = 0.f;
        if ( !ComputeTriangleUVPartials(p0, p1, p2, uv0, uv1, uv2, dUdx, dUdy, dVdx, dVdy) )
        {
          float rhoX = sqrtf(dUdx * dUdx + dVdx * dVdx);
          float rhoY = sqrtf(dUdy * dUdy + dVdy * dVdy);
          float rho  = std::max(rhoX, rhoY);

          float maxDim = static_cast<float>(std::max(tex -> GetWidth(), tex -> GetHeight()));
          float lambda = rho * maxDim;
          if ( lambda > EPSILON )
            ioRasterTri._LOD = std::max(0.f, log2f(lambda));
        }
      }
    }
  }
}

// ----------------------------------------------------------------------------
// Rasterize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Rasterize()
{
  if (TiledRendering())
  {
    const auto binRanges = [this](unsigned int iBegin, unsigned int iEnd) {
      for ( unsigned int i = iBegin; i < iEnd; ++i )
        this->BinTrianglesToTiles(i);
    };
    for ( unsigned int i = 0; i < _NbJobs; ++i )
      JobSystem::Get().Execute([&binRanges, i]() { binRanges(i, i + 1); });
    JobSystem::Get().Wait();

    for ( const auto & tile : _Tiles )
    {
      for ( const auto & bin : tile._RasterTrisBins )
        _Stats._BinnedTriangles += bin.size();
    }

    const auto rasterizeTiles = [this](unsigned int iBegin, unsigned int iEnd) {
      for ( unsigned int tileIndex = iBegin; tileIndex < iEnd; ++tileIndex )
      {
        rd::Tile & tile = _Tiles[tileIndex];
        unsigned int totalTris = 0;
        for ( const auto & bin : tile._RasterTrisBins )
          totalTris += static_cast<unsigned int>(bin.size());
        if ( !totalTris )
          continue;
        if (_EnableSIMD)
        {
#if defined(SIMD_AVX2)
          this->RasterizeAVX2(tile);
#elif defined (SIMD_ARM_NEON)
          this->RasterizeARM(tile);
#else
          this->Rasterize(tile);
#endif
        }
        else
          this->Rasterize(tile);
      }
    };
    for ( unsigned int i = 0; i < _Tiles.size(); ++i )
      JobSystem::Get().Execute([&rasterizeTiles, i]() { rasterizeTiles(i, i + 1); });
    JobSystem::Get().Wait();
    _Stats._TileJobs += _Tiles.size();
    for ( const auto & tile : _Tiles )
    {
      _Stats._DepthWinningPixels += tile._DepthWins;
      _Stats._CoveredPixels += tile._CoveredCount;
      _Stats._MaskedFragmentsTested += tile._MaskedTested;
      _Stats._MaskedFragmentsRejected += tile._MaskedRejected;
    }
  }
  else
  {
    for (unsigned int i = 0; i < _NbJobs; ++i)
      _Fragments[i].clear();

    int height = _Settings._RenderResolution.y;

    for (unsigned int i = 0; i < _NbJobs; ++i)
    {
      int startY = (height / _NbJobs) * i;
      int endY = (i == _NbJobs - 1) ? (height) : (startY + (height / _NbJobs));

      JobSystem::Get().Execute([this, i, startY, endY]() { this->Rasterize(i, startY, endY); });
    }
    JobSystem::Get().Wait();
    for ( unsigned int i = 0; i < _NbJobs; ++i )
    {
      _Stats._MaskedFragmentsTested += _MaskedTestedBuf[i];
      _Stats._MaskedFragmentsRejected += _MaskedRejectedBuf[i];
      _MaskedTestedBuf[i] = 0;
      _MaskedRejectedBuf[i] = 0;
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Rasterize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Rasterize(int iThreadBin, int iStartY, int iEndY)
{
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);
  std::uint64_t maskedTested = 0;
  std::uint64_t maskedRejected = 0;

  for (unsigned int i = 0; i < _NbJobs; ++i)
  {
    for (int j = 0; j < _RasterTrianglesBuf[i].size(); ++j)
    {
      rd::RasterTriangle& tri = _RasterTrianglesBuf[i][j];
      const MaterialPass materialPass = TriangleMaterialPass(tri);
      if ( ( MaterialPass::Blend == materialPass ) || ( MaterialPass::Transmission == materialPass ) )
        continue;

      // Backface culling
      if (0)
      {
        Vec3 AB(tri._V[1] - tri._V[0]);
        Vec3 AC(tri._V[2] - tri._V[0]);
        Vec3 crossProd = glm::cross(AB, AC);
        if (crossProd.z < 0)
          continue;
      }

      int xMin = std::max(0, std::min(static_cast<int>(std::floorf(tri._BBox._Low.x)), RenderWidth() - 1));
      int yMin = std::max(iStartY, std::min(static_cast<int>(std::floorf(tri._BBox._Low.y)), iEndY - 1));
      int xMax = std::max(0, std::min(static_cast<int>(std::floorf(tri._BBox._High.x)), RenderWidth() - 1));
      int yMax = std::max(iStartY, std::min(static_cast<int>(std::floorf(tri._BBox._High.y)), iEndY - 1));

      for (int y = yMin; y <= yMax; ++y)
      {
        for (int x = xMin; x <= xMax; ++x)
        {
          // Frag coord
          Vec3 coord(x + .5f, y + .5f, 0.f);

          // Barycentric coordinates
          float W[3] = { 0.f };
          bool isIn = MathUtil::EvalBarycentricCoordinates(coord, tri._EdgeA, tri._EdgeB, tri._EdgeC, W);
          if (!isIn)
            continue;

          // Perspective correct Z
          W[0] *= tri._InvW[0];
          W[1] *= tri._InvW[1];
          W[2] *= tri._InvW[2];
          float Z = 1.f / (W[0] + W[1] + W[2]);

          // Interpolate depth in screen space
          W[0] *= Z;
          W[1] *= Z;
          W[2] *= Z;
          coord.z = W[0] * tri._V[0].z + W[1] * tri._V[1].z + W[2] * tri._V[2].z;

          if ( MaterialPass::Mask == materialPass )
          {
            maskedTested++;
            const Material & material = _Scene.GetMaterials()[tri._MatID];
            if ( ResolveFragmentOpacity(tri, W) < material._AlphaCutoff )
            {
              maskedRejected++;
              continue;
            }
          }

          // Depth test
          unsigned int pixelIndex = x + RenderWidth() * y;
          if (_Settings._WBuffer)
          {
            if (Z > _ImageBuffer._DepthBuffer[x + RenderWidth() * y] || (Z < zNear))
              continue;
            _ImageBuffer._DepthBuffer[pixelIndex] = Z;
          }
          else
          {
            if (coord.z > _ImageBuffer._DepthBuffer[x + RenderWidth() * y] || (coord.z < -1.f))
              continue;
            _ImageBuffer._DepthBuffer[pixelIndex] = coord.z;
          }

          // Setup fragment
          rd::Fragment frag;
          frag._FragCoords = coord;
          frag._PixelCoords.x = x;
          frag._PixelCoords.y = y;
          frag._RasterTriIdx.x = i;
          frag._RasterTriIdx.y = j,
          frag._Weights[0] = W[0];
          frag._Weights[1] = W[1];
          frag._Weights[2] = W[2];

          _Fragments[iThreadBin].push_back(frag);
        }
      }
    }
  }

  _MaskedTestedBuf[iThreadBin] = maskedTested;
  _MaskedRejectedBuf[iThreadBin] = maskedRejected;
  return 0;
}

// ----------------------------------------------------------------------------
// Rasterize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::Rasterize(rd::Tile& ioTile)
{
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  for (unsigned int i = 0; i < _NbJobs; ++i)
  {
    for (int j = 0; j < ioTile._RasterTrisBins[i].size(); ++j)
    {
      const rd::RasterTriangle * tri = ioTile._RasterTrisBins[i][j];
      if (!tri)
        continue;
      const MaterialPass materialPass = TriangleMaterialPass(*tri);
      if ( ( MaterialPass::Blend == materialPass ) || ( MaterialPass::Transmission == materialPass ) )
        continue;

      int startX = std::max(ioTile._X, static_cast<int>(std::floor(tri->_BBox._Low.x)));
      int endX = std::min(ioTile._X + ioTile._Width - 1, static_cast<int>(std::ceil(tri->_BBox._High.x)));
      int startY = std::max(ioTile._Y, static_cast<int>(std::floor(tri->_BBox._Low.y)));
      int endY = std::min(ioTile._Y + ioTile._Height - 1, static_cast<int>(std::ceil(tri->_BBox._High.y)));

      for (int y = startY; y <= endY; ++y)
      {
        for (int x = startX; x <= endX; ++x)
        {
          // Frag coord
          Vec3 coord(x + .5f, y + .5f, 0.f);

          // Barycentric coordinates
          float W[3] = { 0.f };
          bool isIn = MathUtil::EvalBarycentricCoordinates(coord, tri->_EdgeA, tri->_EdgeB, tri->_EdgeC, W);
          if (!isIn)
            continue;

          // Perspective correct Z
          W[0] *= tri->_InvW[0];
          W[1] *= tri->_InvW[1];
          W[2] *= tri->_InvW[2];
          float Z = 1.f / (W[0] + W[1] + W[2]);

          // Interpolate depth in screen space
          W[0] *= Z;
          W[1] *= Z;
          W[2] *= Z;
          coord.z = W[0] * tri->_V[0].z + W[1] * tri->_V[1].z + W[2] * tri->_V[2].z;

          if ( MaterialPass::Mask == materialPass )
          {
            ioTile._MaskedTested++;
            const Material & material = _Scene.GetMaterials()[tri->_MatID];
            if ( ResolveFragmentOpacity(*tri, W) < material._AlphaCutoff )
            {
              ioTile._MaskedRejected++;
              continue;
            }
          }

          // Depth test
          unsigned int localX = x - ioTile._X;
          unsigned int localY = y - ioTile._Y;
          unsigned int localPixelIndex = localY * ioTile._Width + localX;
          if (_Settings._WBuffer)
          {
            if ((Z > ioTile._LocalFB._DepthBuffer[localPixelIndex]) || (Z < zNear))
              continue;
            ioTile._LocalFB._DepthBuffer[localPixelIndex] = Z;
          }
          else
          {
            if ((coord.z > ioTile._LocalFB._DepthBuffer[localPixelIndex]) || (coord.z < -1.f))
              continue;
            ioTile._LocalFB._DepthBuffer[localPixelIndex] = coord.z;
          }        

          ioTile._DepthWins++;
          if ( _EnableCompactHits )
          {
            rd::CompactHit & hit = ioTile._CompactHits[localPixelIndex];
            if ( ioTile._CompactHitGenerations[localPixelIndex] != ioTile._CompactGeneration )
            {
              ioTile._CompactHitGenerations[localPixelIndex] = ioTile._CompactGeneration;
              ioTile._CoveredIndices.push_back(localPixelIndex);
              ioTile._CoveredCount++;
            }
            hit._Triangle = tri;
            hit._Depth = coord.z;
            hit._Weights[0] = W[0];
            hit._Weights[1] = W[1];
            hit._Weights[2] = W[2];
          }
          else
          {
            if ( !ioTile._CoveredPixels[localPixelIndex] )
              ioTile._CoveredCount++;
            ioTile._CoveredPixels[localPixelIndex] = true;
            rd::Fragment & frag = ioTile._Fragments[localPixelIndex];
            frag._FragCoords = coord;
            frag._RasterTriIdx.x = i;
            frag._RasterTriIdx.y = j;
            frag._Weights[0] = W[0];
            frag._Weights[1] = W[1];
            frag._Weights[2] = W[2];
          }
        }
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// TriangleMaterialPass
// ----------------------------------------------------------------------------
MaterialPass SoftwareRasterizer::TriangleMaterialPass( const rd::RasterTriangle & iTriangle ) const
{
  const std::vector<Material> & materials = _Scene.GetMaterials();
  if ( ( iTriangle._MatID < 0 ) || ( iTriangle._MatID >= static_cast<int>(materials.size()) ) )
    return MaterialPass::Opaque;
  return ClassifyMaterialPass(materials[iTriangle._MatID]);
}

// ----------------------------------------------------------------------------
// ResolveFragmentOpacity
// ----------------------------------------------------------------------------
float SoftwareRasterizer::ResolveFragmentOpacity( const rd::RasterTriangle & iTriangle, const float iWeights[3] ) const
{
  const std::vector<Material> & materials = _Scene.GetMaterials();
  if ( ( iTriangle._MatID < 0 ) || ( iTriangle._MatID >= static_cast<int>(materials.size()) ) )
    return 1.f;

  rd::Varying varying;
  rd::Varying::Interpolate(_ProjVerticesBuf[iTriangle._Indices[0]]._Attrib,
                           _ProjVerticesBuf[iTriangle._Indices[1]]._Attrib,
                           _ProjVerticesBuf[iTriangle._Indices[2]]._Attrib,
                           iWeights, varying);
  varying._LOD = iTriangle._LOD;
  return ResolveMaterialOpacity(materials[iTriangle._MatID], _Scene.GetTextures(),
                                _Settings._Sampling, varying._UV, varying._LOD);
}

// ----------------------------------------------------------------------------
// RasterizeTransparent
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RasterizeTransparent()
{
  if ( _DebugMode & ( (int)RasterDebugModes::DepthBuffer | (int)RasterDebugModes::Normals ) )
    return 0;

  if ( TiledRendering() )
  {
    for ( unsigned int i = 0; i < _Tiles.size(); ++i )
      JobSystem::Get().Execute([this, i]() { this->RasterizeTransparent(_Tiles[i]); });
    JobSystem::Get().Wait();
    _Stats._TileJobs += _Tiles.size();
    for ( const rd::Tile & tile : _Tiles )
    {
      _Stats._TransparentHitsGenerated += tile._TransparentHits.size();
      _Stats._TransparentHitBufferBytes += tile._TransparentHits.capacity() * sizeof(rd::TransparentHit);
      for ( const rd::TransparentHit & hit : tile._TransparentHits )
      {
        if ( MaterialPass::Transmission == hit._MaterialPass )
          _Stats._TransmissionHitsGenerated++;
        else
          _Stats._BlendHitsGenerated++;
      }
    }
  }
  else
  {
    const int height = RenderHeight();
    for ( unsigned int i = 0; i < _NbJobs; ++i )
    {
      _TransparentFragments[i].clear();
      const int startY = (height / _NbJobs) * i;
      const int endY = ( i == _NbJobs - 1 ) ? height : startY + height / _NbJobs;
      if ( startY < endY )
        JobSystem::Get().Execute([this, i, startY, endY]() { this->RasterizeTransparent(i, startY, endY); });
    }
    JobSystem::Get().Wait();
    for ( const auto & hits : _TransparentFragments )
    {
      _Stats._TransparentHitsGenerated += hits.size();
      _Stats._TransparentHitBufferBytes += hits.capacity() * sizeof(rd::TransparentHit);
      for ( const rd::TransparentHit & hit : hits )
      {
        if ( MaterialPass::Transmission == hit._MaterialPass )
          _Stats._TransmissionHitsGenerated++;
        else
          _Stats._BlendHitsGenerated++;
      }
    }
  }
  return 0;
}

// ----------------------------------------------------------------------------
// RasterizeTransparent
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RasterizeTransparent(int iThreadBin, int iStartY, int iEndY)
{
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);
  std::vector<rd::TransparentHit> & hits = _TransparentFragments[iThreadBin];

  for ( unsigned int bin = 0; bin < _NbJobs; ++bin )
  {
    for ( rd::RasterTriangle & tri : _RasterTrianglesBuf[bin] )
    {
      const MaterialPass materialPass = TriangleMaterialPass(tri);
      if ( ( MaterialPass::Blend != materialPass ) && ( MaterialPass::Transmission != materialPass ) )
        continue;
      const int xMin = std::max(0, std::min(static_cast<int>(std::floor(tri._BBox._Low.x)), RenderWidth() - 1));
      const int yMin = std::max(iStartY, std::min(static_cast<int>(std::floor(tri._BBox._Low.y)), iEndY - 1));
      const int xMax = std::max(0, std::min(static_cast<int>(std::ceil(tri._BBox._High.x)), RenderWidth() - 1));
      const int yMax = std::max(iStartY, std::min(static_cast<int>(std::ceil(tri._BBox._High.y)), iEndY - 1));

      for ( int y = yMin; y <= yMax; ++y )
      {
        for ( int x = xMin; x <= xMax; ++x )
        {
          Vec3 coord(x + .5f, y + .5f, 0.f);
          float weights[3] = { 0.f, 0.f, 0.f };
          if ( !MathUtil::EvalBarycentricCoordinates(coord, tri._EdgeA, tri._EdgeB, tri._EdgeC, weights) )
            continue;
          weights[0] *= tri._InvW[0];
          weights[1] *= tri._InvW[1];
          weights[2] *= tri._InvW[2];
          const float depth = 1.f / ( weights[0] + weights[1] + weights[2] );
          weights[0] *= depth;
          weights[1] *= depth;
          weights[2] *= depth;
          const float fragmentDepth = weights[0] * tri._V[0].z + weights[1] * tri._V[1].z + weights[2] * tri._V[2].z;
          const unsigned int pixelIndex = x + RenderWidth() * y;
          if ( _Settings._WBuffer )
          {
            if ( ( depth > _ImageBuffer._DepthBuffer[pixelIndex] ) || ( depth < zNear ) )
              continue;
          }
          else if ( ( fragmentDepth > _ImageBuffer._DepthBuffer[pixelIndex] ) || ( fragmentDepth < -1.f ) )
            continue;
          if ( ( MaterialPass::Blend == materialPass ) && ( ResolveFragmentOpacity(tri, weights) <= 0.f ) )
            continue;

          rd::TransparentHit hit;
          hit._Triangle = &tri;
          hit._PixelIndex = pixelIndex;
          hit._Depth = _Settings._WBuffer ? depth : fragmentDepth;
          hit._FragmentDepth = fragmentDepth;
          hit._MaterialPass = materialPass;
          std::copy(weights, weights + 3, hit._Weights);
          hits.push_back(hit);
        }
      }
    }
  }
  return 0;
}

// ----------------------------------------------------------------------------
// RasterizeTransparent
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RasterizeTransparent(rd::Tile& ioTile)
{
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);
  std::vector<rd::TransparentHit> & hits = ioTile._TransparentHits;

  for ( unsigned int bin = 0; bin < _NbJobs; ++bin )
  {
    for ( const rd::RasterTriangle * tri : ioTile._RasterTrisBins[bin] )
    {
      if ( !tri )
        continue;
      const MaterialPass materialPass = TriangleMaterialPass(*tri);
      if ( ( MaterialPass::Blend != materialPass ) && ( MaterialPass::Transmission != materialPass ) )
        continue;
      const int xMin = std::max(ioTile._X, static_cast<int>(std::floor(tri->_BBox._Low.x)));
      const int yMin = std::max(ioTile._Y, static_cast<int>(std::floor(tri->_BBox._Low.y)));
      const int xMax = std::min(ioTile._X + ioTile._Width - 1, static_cast<int>(std::ceil(tri->_BBox._High.x)));
      const int yMax = std::min(ioTile._Y + ioTile._Height - 1, static_cast<int>(std::ceil(tri->_BBox._High.y)));

      for ( int y = yMin; y <= yMax; ++y )
      {
        for ( int x = xMin; x <= xMax; ++x )
        {
          Vec3 coord(x + .5f, y + .5f, 0.f);
          float weights[3] = { 0.f, 0.f, 0.f };
          if ( !MathUtil::EvalBarycentricCoordinates(coord, tri->_EdgeA, tri->_EdgeB, tri->_EdgeC, weights) )
            continue;
          weights[0] *= tri->_InvW[0];
          weights[1] *= tri->_InvW[1];
          weights[2] *= tri->_InvW[2];
          const float depth = 1.f / ( weights[0] + weights[1] + weights[2] );
          weights[0] *= depth;
          weights[1] *= depth;
          weights[2] *= depth;
          const float fragmentDepth = weights[0] * tri->_V[0].z + weights[1] * tri->_V[1].z + weights[2] * tri->_V[2].z;
          const unsigned int localPixelIndex = ( y - ioTile._Y ) * ioTile._Width + x - ioTile._X;
          if ( _Settings._WBuffer )
          {
            if ( ( depth > ioTile._LocalFB._DepthBuffer[localPixelIndex] ) || ( depth < zNear ) )
              continue;
          }
          else if ( ( fragmentDepth > ioTile._LocalFB._DepthBuffer[localPixelIndex] ) || ( fragmentDepth < -1.f ) )
            continue;
          if ( ( MaterialPass::Blend == materialPass ) && ( ResolveFragmentOpacity(*tri, weights) <= 0.f ) )
            continue;

          rd::TransparentHit hit;
          hit._Triangle = tri;
          hit._PixelIndex = localPixelIndex;
          hit._Depth = _Settings._WBuffer ? depth : fragmentDepth;
          hit._FragmentDepth = fragmentDepth;
          hit._MaterialPass = materialPass;
          std::copy(weights, weights + 3, hit._Weights);
          hits.push_back(hit);
        }
      }
    }
  }
  return 0;
}

#ifdef SIMD_AVX2
// ----------------------------------------------------------------------------
// RasterizeAVX2
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RasterizeAVX2(rd::Tile& ioTile)
{
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  for (unsigned int i = 0; i < _NbJobs; ++i)
  {
    for (int j = 0; j < ioTile._RasterTrisBins[i].size(); ++j)
    {
      const rd::RasterTriangle * tri = ioTile._RasterTrisBins[i][j];
      if (!tri)
        continue;
      const MaterialPass materialPass = TriangleMaterialPass(*tri);
      if ( ( MaterialPass::Blend == materialPass ) || ( MaterialPass::Transmission == materialPass ) )
        continue;

      int startX = std::max(ioTile._X, static_cast<int>(std::floor(tri->_BBox._Low.x)));
      int endX = std::min(ioTile._X + ioTile._Width - 1, static_cast<int>(std::ceil(tri->_BBox._High.x)));
      int startY = std::max(ioTile._Y, static_cast<int>(std::floor(tri->_BBox._Low.y)));
      int endY = std::min(ioTile._Y + ioTile._Height - 1, static_cast<int>(std::ceil(tri->_BBox._High.y)));

      __m256 invZ0 = _mm256_set1_ps(tri->_InvW[0]);
      __m256 invZ1 = _mm256_set1_ps(tri->_InvW[1]);
      __m256 invZ2 = _mm256_set1_ps(tri->_InvW[2]);

      __m256 v0z = _mm256_set1_ps(tri->_V[0].z);
      __m256 v1z = _mm256_set1_ps(tri->_V[1].z);
      __m256 v2z = _mm256_set1_ps(tri->_V[2].z);

      // Precompute x_coords for all possible x offsets (0..7)
      alignas(32) float x_offsets[8] = {0.5f, 1.5f, 2.5f, 3.5f, 4.5f, 5.5f, 6.5f, 7.5f};

      for (int y = startY; y <= endY; ++y)
      {
        __m256 y_coord = _mm256_set1_ps(y + 0.5f);

        unsigned int localY = y - ioTile._Y;

        for (int x = startX; x <= endX; x += 8)
        {
          unsigned int localX = x - ioTile._X;
          unsigned int localPixelIndex = localY * ioTile._Width + localX;

          // Use aligned load for x_coords
          __m256 x_coords = _mm256_add_ps(_mm256_set1_ps((float)x), _mm256_load_ps(x_offsets));

          // Compute barycentric coordinates
          __m256 Weights[3];
          __m256 mask = SIMDUtils::EvalBarycentricCoordinatesAVX2(x_coords, y_coord, tri->_EdgeA, tri->_EdgeB, tri->_EdgeC, Weights);

          // Perspective correct Z
          Weights[0] = _mm256_mul_ps(Weights[0], invZ0);
          Weights[1] = _mm256_mul_ps(Weights[1], invZ1);
          Weights[2] = _mm256_mul_ps(Weights[2], invZ2);

          __m256 invDepths = _mm256_add_ps(_mm256_add_ps(Weights[0], Weights[1]), Weights[2]);
          __m256 depths = _mm256_div_ps(_mm256_set1_ps(1.0f), invDepths);

          // Interpolate depth in screen space
          Weights[0] = _mm256_mul_ps(Weights[0], depths);
          Weights[1] = _mm256_mul_ps(Weights[1], depths);
          Weights[2] = _mm256_mul_ps(Weights[2], depths);

          __m256 z_coord;
          SIMDUtils::InterpolateAVX2(v0z, v1z, v2z, Weights, z_coord);

          // Depth test
          __m256 depthBuf;
          if ( (endX - x + 1) >= 8 )
            depthBuf = _mm256_loadu_ps(&ioTile._LocalFB._DepthBuffer[localPixelIndex]); // Use aligned AVX load directly
          else
          {
            SIMD_ALIGN64 float DepthBuffer[8] = { 0. };
            memcpy(DepthBuffer, &ioTile._LocalFB._DepthBuffer[localPixelIndex], (endX - x + 1) * sizeof(float)); // Fallback for partial tiles
            depthBuf = _mm256_load_ps(DepthBuffer);
          }

          __m256 depthmask;
          if ( _Settings._WBuffer )
            depthmask = _mm256_cmp_ps(depths, depthBuf, _CMP_LE_OQ);
          else
            depthmask = _mm256_cmp_ps(z_coord, depthBuf, _CMP_LE_OQ);
          mask = _mm256_and_ps(mask, depthmask);

          int activeMask = _mm256_movemask_ps(mask);
          for (int k = 0; (k < 8) && ((x + k) <= endX); ++k)
          {
            if ( !(activeMask & (1 << k)) )
              continue;

            float depth = depths.m256_f32[k];
            float z = z_coord.m256_f32[k];

            float weights[3] = { Weights[0].m256_f32[k], Weights[1].m256_f32[k], Weights[2].m256_f32[k] };
            if ( MaterialPass::Mask == materialPass )
            {
              ioTile._MaskedTested++;
              const Material & material = _Scene.GetMaterials()[tri->_MatID];
              if ( ResolveFragmentOpacity(*tri, weights) < material._AlphaCutoff )
              {
                ioTile._MaskedRejected++;
                continue;
              }
            }

            ioTile._LocalFB._DepthBuffer[localPixelIndex + k] = ( _Settings._WBuffer ) ? ( depth ) : ( z );
            ioTile._DepthWins++;
            const unsigned int hitIndex = localPixelIndex + k;
            if ( _EnableCompactHits )
            {
              rd::CompactHit & hit = ioTile._CompactHits[hitIndex];
              if ( ioTile._CompactHitGenerations[hitIndex] != ioTile._CompactGeneration )
              {
                ioTile._CompactHitGenerations[hitIndex] = ioTile._CompactGeneration;
                ioTile._CoveredIndices.push_back(hitIndex);
                ioTile._CoveredCount++;
              }
              hit._Triangle = tri;
              hit._Depth = z;
              hit._Weights[0] = Weights[0].m256_f32[k];
              hit._Weights[1] = Weights[1].m256_f32[k];
              hit._Weights[2] = Weights[2].m256_f32[k];
            }
            else
            {
              if ( !ioTile._CoveredPixels[hitIndex] )
                ioTile._CoveredCount++;
              ioTile._CoveredPixels[hitIndex] = true;
              rd::Fragment & frag = ioTile._Fragments[hitIndex];
              frag._FragCoords = Vec3(x + k + .5f, y + .5f, z);
              frag._RasterTriIdx = Vec2i(i, j);
              frag._Weights[0] = Weights[0].m256_f32[k];
              frag._Weights[1] = Weights[1].m256_f32[k];
              frag._Weights[2] = Weights[2].m256_f32[k];
            }
          }
        }
      }
    }
  }

  return 0;
}
#endif // SIMD_AVX2

#ifdef SIMD_ARM_NEON
// ----------------------------------------------------------------------------
// RasterizeARM
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RasterizeARM(rd::Tile& ioTile)
{
  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  const float32x4_t ones = { 1.f, 1.f, 1.f, 1.f };

  for (unsigned int i = 0; i < _NbJobs; ++i)
  {
    for (int j = 0; j < ioTile._RasterTrisBins[i].size(); ++j)
    {
      const rd::RasterTriangle * tri = ioTile._RasterTrisBins[i][j];
      if (!tri)
        continue;
      const MaterialPass materialPass = TriangleMaterialPass(*tri);
      if ( ( MaterialPass::Blend == materialPass ) || ( MaterialPass::Transmission == materialPass ) )
        continue;

      int startX = std::max(ioTile._X, static_cast<int>(std::floor(tri->_BBox._Low.x)));
      int endX = std::min(ioTile._X + ioTile._Width - 1, static_cast<int>(std::ceil(tri->_BBox._High.x)));
      int startY = std::max(ioTile._Y, static_cast<int>(std::floor(tri->_BBox._Low.y)));
      int endY = std::min(ioTile._Y + ioTile._Height - 1, static_cast<int>(std::ceil(tri->_BBox._High.y)));

      float32x4_t invZ0 = vdupq_n_f32(tri->_InvW[0]);
      float32x4_t invZ1 = vdupq_n_f32(tri->_InvW[1]);
      float32x4_t invZ2 = vdupq_n_f32(tri->_InvW[2]);

      float32x4_t v0z = vdupq_n_f32(tri->_V[0].z);
      float32x4_t v1z = vdupq_n_f32(tri->_V[1].z);
      float32x4_t v2z = vdupq_n_f32(tri->_V[2].z);

      // Precompute x_coords for all possible x offsets (0..7)
      float32x4_t x_offsets = { 0.5f, 1.5f, 2.5f, 3.5f };

      for (int y = startY; y <= endY; ++y)
      {
        float32x4_t y_coord = vdupq_n_f32(y + 0.5f);

        unsigned int localY = y - ioTile._Y;

        for (int x = startX; x <= endX; x += 4)
        {
          unsigned int localX = x - ioTile._X;
          unsigned int localPixelIndex = localY * ioTile._Width + localX;

          // Use aligned load for x_coords
          float32x4_t x_coords = vaddq_f32(vdupq_n_f32((float)x), x_offsets);

          // Compute barycentric coordinates
          float32x4_t Weights[3];
          uint32x4_t mask = SIMDUtils::EvalBarycentricCoordinatesARM(x_coords, y_coord, tri->_EdgeA, tri->_EdgeB, tri->_EdgeC, Weights);

          // Perspective correct Z
          Weights[0] = vmulq_f32(Weights[0], invZ0);
          Weights[1] = vmulq_f32(Weights[1], invZ1);
          Weights[2] = vmulq_f32(Weights[2], invZ2);

          float32x4_t invDepths = vaddq_f32(vaddq_f32(Weights[0], Weights[1]), Weights[2]);
          float32x4_t depths = vdivq_f32(ones, invDepths);

          // Interpolate depth in screen space
          Weights[0] = vmulq_f32(Weights[0], depths);
          Weights[1] = vmulq_f32(Weights[1], depths);
          Weights[2] = vmulq_f32(Weights[2], depths);

          float32x4_t z_coord;
          SIMDUtils::InterpolateARM(v0z, v1z, v2z, Weights, z_coord);

          // Depth test
          float32x4_t depthBuf;
          if ( (endX - x + 1) >= 4 )
            depthBuf = vld1q_f32(&ioTile._LocalFB._DepthBuffer[localPixelIndex]); // Use aligned SIMD load directly
          else
          {
            SIMD_ALIGN64 float DepthBuffer[4] = { 0. };
            memcpy(DepthBuffer, &ioTile._LocalFB._DepthBuffer[localPixelIndex], (endX - x + 1) * sizeof(float)); // Fallback for partial tiles
            depthBuf = vld1q_f32(DepthBuffer);
          }

          uint32x4_t depthmask;
          if ( _Settings._WBuffer )
            depthmask = vcleq_f32(depths, depthBuf);
          else
            depthmask = vcleq_f32(z_coord, depthBuf);
          mask = vandq_u32(mask, depthmask);

          for (int k = 0; (k < 4) && ((x + k) <= endX); ++k)
          {
            if ( !SIMDUtils::GetVectorElement(mask, k) )
              continue;

            float depth = SIMDUtils::GetVectorElement(depths, k);
            float z = SIMDUtils::GetVectorElement(z_coord, k);

            float weights[3] = {
              SIMDUtils::GetVectorElement(Weights[0], k),
              SIMDUtils::GetVectorElement(Weights[1], k),
              SIMDUtils::GetVectorElement(Weights[2], k)
            };
            if ( MaterialPass::Mask == materialPass )
            {
              ioTile._MaskedTested++;
              const Material & material = _Scene.GetMaterials()[tri->_MatID];
              if ( ResolveFragmentOpacity(*tri, weights) < material._AlphaCutoff )
              {
                ioTile._MaskedRejected++;
                continue;
              }
            }

            ioTile._LocalFB._DepthBuffer[localPixelIndex + k] = ( _Settings._WBuffer ) ? ( depth ) : ( z );
            ioTile._DepthWins++;
            const unsigned int hitIndex = localPixelIndex + k;
            if ( _EnableCompactHits )
            {
              rd::CompactHit & hit = ioTile._CompactHits[hitIndex];
              if ( ioTile._CompactHitGenerations[hitIndex] != ioTile._CompactGeneration )
              {
                ioTile._CompactHitGenerations[hitIndex] = ioTile._CompactGeneration;
                ioTile._CoveredIndices.push_back(hitIndex);
                ioTile._CoveredCount++;
              }
              hit._Triangle = tri;
              hit._Depth = z;
              hit._Weights[0] = SIMDUtils::GetVectorElement(Weights[0], k);
              hit._Weights[1] = SIMDUtils::GetVectorElement(Weights[1], k);
              hit._Weights[2] = SIMDUtils::GetVectorElement(Weights[2], k);
            }
            else
            {
              if ( !ioTile._CoveredPixels[hitIndex] )
                ioTile._CoveredCount++;
              ioTile._CoveredPixels[hitIndex] = true;
              rd::Fragment & frag = ioTile._Fragments[hitIndex];
              frag._FragCoords = Vec3(x + k + .5f, y + .5f, z);
              frag._RasterTriIdx = Vec2i(i, j);
              frag._Weights[0] = SIMDUtils::GetVectorElement(Weights[0], k);
              frag._Weights[1] = SIMDUtils::GetVectorElement(Weights[1], k);
              frag._Weights[2] = SIMDUtils::GetVectorElement(Weights[2], k);
            }
          }
        }
      }
    }
  }

  return 0;
}
#endif // SIMD_ARM_NEON

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ProcessFragments()
{
  rd::DefaultUniform uniforms;
  uniforms._CameraPos = _Scene.GetCamera().GetPos();
  uniforms._Sampling = _Settings._Sampling;
  uniforms._Materials = &_Scene.GetMaterials();
  uniforms._Textures = &_Scene.GetTextures();
  for (int i = 0; i < _Scene.GetNbLights(); ++i)
    uniforms._Lights.push_back(*_Scene.GetLight(i));

  if (TiledRendering())
  {
    const auto processTiles = [this, &uniforms](unsigned int iBegin, unsigned int iEnd) {
      for ( unsigned int i = iBegin; i < iEnd; ++i )
      {
        if ( _Tiles[i]._Fragments.size() )
          this->ProcessFragments(_Tiles[i], uniforms);
        else
          this->CopyTileToMainBuffer(_Tiles[i]);
      }
    };
    for ( unsigned int i = 0; i < _Tiles.size(); ++i )
      JobSystem::Get().Execute([&processTiles, i]() { processTiles(i, i + 1); });
    JobSystem::Get().Wait();
    _Stats._TileJobs += _Tiles.size();
    for ( const auto & tile : _Tiles )
    {
      _Stats._ShadedPixels += tile._ShadedCount;
    }
  }
  else
  {
    for (unsigned int i = 0; i < _NbJobs; ++i)
    {
      if ( _Fragments[i].size() )
        JobSystem::Get().Execute([this, i, uniforms]() {
          this->ProcessFragments(i, uniforms);
        });
    }
    JobSystem::Get().Wait();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessTransparentFragments
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ProcessTransparentFragments()
{
  rd::DefaultUniform uniforms;
  uniforms._CameraPos = _Scene.GetCamera().GetPos();
  uniforms._Sampling = _Settings._Sampling;
  uniforms._Materials = &_Scene.GetMaterials();
  uniforms._Textures = &_Scene.GetTextures();
  uniforms._EnvMap = &_Scene.GetEnvMap();
  uniforms._EnvMapRotation = _Settings._SkyBoxRotation;
  uniforms._SpecularIBLIntensity = _Settings._SpecularIBLIntensity;
  uniforms._SpecularIBLMaxRoughness = _Settings._SpecularIBLMaxRoughness;
  uniforms._EnableEnvMap = _Settings._EnableSkybox && _Scene.GetEnvMap().IsInitialized();
  for ( int i = 0; i < _Scene.GetNbLights(); ++i )
    uniforms._Lights.push_back(*_Scene.GetLight(i));

  if ( TiledRendering() )
  {
    for ( unsigned int i = 0; i < _Tiles.size(); ++i )
    {
      if ( !_Tiles[i]._TransparentHits.empty() )
        JobSystem::Get().Execute([this, &uniforms, i]() {
          this->ProcessTransparentFragments(_Tiles[i]._TransparentHits, uniforms, &_Tiles[i]);
        });
    }
  }
  else
  {
    for ( unsigned int i = 0; i < _TransparentFragments.size(); ++i )
    {
      if ( !_TransparentFragments[i].empty() )
        JobSystem::Get().Execute([this, &uniforms, i]() {
          this->ProcessTransparentFragments(_TransparentFragments[i], uniforms, nullptr);
        });
    }
  }
  JobSystem::Get().Wait();

  std::uint64_t transparentPixels = 0;
  std::uint64_t maxLayers = 0;
  if ( TiledRendering() )
  {
    for ( const rd::Tile & tile : _Tiles )
    {
      _Stats._TransparentHitsShaded += tile._TransparentShaded;
      if ( !tile._TransparentHits.empty() )
      {
        unsigned int previousPixel = std::numeric_limits<unsigned int>::max();
        std::uint64_t layers = 0;
        for ( const rd::TransparentHit & hit : tile._TransparentHits )
        {
          if ( hit._PixelIndex != previousPixel )
          {
            if ( layers )
              maxLayers = std::max(maxLayers, layers);
            transparentPixels++;
            previousPixel = hit._PixelIndex;
            layers = 1;
          }
          else
            layers++;
        }
        maxLayers = std::max(maxLayers, layers);
      }
    }
  }
  else
  {
    for ( const auto & hits : _TransparentFragments )
    {
      _Stats._TransparentHitsShaded += hits.size();
      unsigned int previousPixel = std::numeric_limits<unsigned int>::max();
      std::uint64_t layers = 0;
      for ( const rd::TransparentHit & hit : hits )
      {
        if ( hit._PixelIndex != previousPixel )
        {
          if ( layers )
            maxLayers = std::max(maxLayers, layers);
          transparentPixels++;
          previousPixel = hit._PixelIndex;
          layers = 1;
        }
        else
          layers++;
      }
      maxLayers = std::max(maxLayers, layers);
    }
  }
  _Stats._TransparentPixels = transparentPixels;
  _Stats._MaxTransparentLayers = maxLayers;
  _Stats._AverageTransparentLayers = transparentPixels
    ? static_cast<double>(_Stats._TransparentHitsShaded) / transparentPixels : 0.;
  return 0;
}

// ----------------------------------------------------------------------------
// ProcessTransparentFragments
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessTransparentFragments(std::vector<rd::TransparentHit> & ioHits,
                                                      const rd::DefaultUniform & iUniforms,
                                                      rd::Tile * ioTile)
{
  std::sort(ioHits.begin(), ioHits.end(), [](const rd::TransparentHit & iLhs, const rd::TransparentHit & iRhs) {
    if ( iLhs._PixelIndex != iRhs._PixelIndex )
      return iLhs._PixelIndex < iRhs._PixelIndex;
    if ( iLhs._Depth != iRhs._Depth )
      return iLhs._Depth > iRhs._Depth;
    return std::less<const rd::RasterTriangle *>()(iLhs._Triangle, iRhs._Triangle);
  });

  std::unique_ptr<SoftwareFragmentShader> fragmentShader;
  if ( ShadingType::PBR == _Settings._ShadingType )
    fragmentShader = std::make_unique<PBRFragmentShader>(iUniforms);
  else
    fragmentShader = std::make_unique<BlinnPhongFragmentShader>(iUniforms);

  const bool renderWires = _DebugMode & (int)RasterDebugModes::Wires;
  std::unique_ptr<WireFrameFragmentShader> wireShader;
  if ( renderWires )
    wireShader = std::make_unique<WireFrameFragmentShader>(iUniforms);

  for ( const rd::TransparentHit & hit : ioHits )
  {
    if ( !hit._Triangle )
      continue;
    const unsigned int globalPixelIndex = ioTile
      ? ioTile->_X + hit._PixelIndex % ioTile->_Width + RenderWidth() * ( ioTile->_Y + hit._PixelIndex / ioTile->_Width )
      : hit._PixelIndex;
    rd::Fragment fragment;
    fragment._PixelCoords = Vec2i(globalPixelIndex % RenderWidth(), globalPixelIndex / RenderWidth());
    fragment._FragCoords = Vec3(fragment._PixelCoords.x + .5f, fragment._PixelCoords.y + .5f, hit._FragmentDepth);
    std::copy(hit._Weights, hit._Weights + 3, fragment._Weights);
    rd::Varying::Interpolate(_ProjVerticesBuf[hit._Triangle->_Indices[0]]._Attrib,
                             _ProjVerticesBuf[hit._Triangle->_Indices[1]]._Attrib,
                             _ProjVerticesBuf[hit._Triangle->_Indices[2]]._Attrib,
                             fragment._Weights, fragment._Attrib);
    if ( ShadingType::Flat == _Settings._ShadingType )
      fragment._Attrib._Normal = hit._Triangle->_Normal;
    fragment._Attrib._LOD = hit._Triangle->_LOD;

    TransparentShadingResult source = fragmentShader -> ProcessTransparent(fragment, *hit._Triangle, hit._MaterialPass);
    if ( renderWires )
    {
      const Vec4 wire = wireShader -> Process(fragment, *hit._Triangle);
      source._PremultipliedColor = glm::mix(source._PremultipliedColor, Vec3(wire) * source._Alpha, wire.w);
    }
    const float alpha = MathUtil::Clamp(source._Alpha, 0.f, 1.f);
    const float maxSource = std::max(source._PremultipliedColor.x,
                                     std::max(source._PremultipliedColor.y, source._PremultipliedColor.z));
    if ( ( alpha <= EPSILON ) && ( maxSource <= EPSILON ) )
      continue;
    RGBA8 & destination8 = _ImageBuffer._ColorBuffer[globalPixelIndex];
    const Vec3 destination(destination8._R / 255.f, destination8._G / 255.f, destination8._B / 255.f);
    destination8 = RGBA8(source._PremultipliedColor + destination * ( 1.f - alpha ), 1.f);
    if ( ioTile )
      ioTile -> _TransparentShaded++;
  }
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessFragments(int iThreadBin,
                                          const RasterData::DefaultUniform& iUniforms)
{
  std::unique_ptr<SoftwareFragmentShader> ownedFragmentShader;
  if (_DebugMode & (int)RasterDebugModes::DepthBuffer)
    ownedFragmentShader = std::make_unique<DepthFragmentShader>(iUniforms);
  else if (_DebugMode & (int)RasterDebugModes::Normals)
    ownedFragmentShader = std::make_unique<NormalFragmentShader>(iUniforms);
  else if (ShadingType::PBR == _Settings._ShadingType)
    ownedFragmentShader = std::make_unique<PBRFragmentShader>(iUniforms);
  else
    ownedFragmentShader = std::make_unique<BlinnPhongFragmentShader>(iUniforms);
  SoftwareFragmentShader * fragmentShader = ownedFragmentShader.get();

  const bool renderWires = _DebugMode & (int)RasterDebugModes::Wires;
  std::unique_ptr<SoftwareFragmentShader> ownedWireShader;
  SoftwareFragmentShader * wireShaderPtr = nullptr;
  if ( renderWires )
  {
    ownedWireShader = std::make_unique<WireFrameFragmentShader>(iUniforms);
    wireShaderPtr = ownedWireShader.get();
  }

  for ( auto & frag : _Fragments[iThreadBin] ) // From back to front
  {
    // Finalize fragment
    const rd::RasterTriangle & tri = _RasterTrianglesBuf[frag._RasterTriIdx.x][frag._RasterTriIdx.y];

    rd::Varying::Interpolate(_ProjVerticesBuf[tri._Indices[0]]._Attrib, _ProjVerticesBuf[tri._Indices[1]]._Attrib, _ProjVerticesBuf[tri._Indices[2]]._Attrib, frag._Weights, frag._Attrib);
    if (ShadingType::Flat == _Settings._ShadingType)
      frag._Attrib._Normal = tri._Normal;

    frag._Attrib._LOD = tri._LOD;

    // Shade fragment
    Vec4 fragColor = fragmentShader->Process(frag, tri);

    if (renderWires)
    {
      Vec4 wireColor = wireShaderPtr->Process(frag, tri);
      fragColor.x = glm::mix(fragColor.x, wireColor.x, wireColor.w);
      fragColor.y = glm::mix(fragColor.y, wireColor.y, wireColor.w);
      fragColor.z = glm::mix(fragColor.z, wireColor.z, wireColor.w);
    }

    unsigned int pixelIndex = frag._PixelCoords.x + RenderWidth() * frag._PixelCoords.y;
    _ImageBuffer._ColorBuffer[pixelIndex] = fragColor;
  }
}

// ----------------------------------------------------------------------------
// BinTrianglesToTiles
// ----------------------------------------------------------------------------
void SoftwareRasterizer::BinTrianglesToTiles(unsigned int iBufferIndex)
{
  if ((iBufferIndex < 0) || (iBufferIndex >= _NbJobs))
  {
    std::cerr << "Invalid buffer index: " << iBufferIndex << std::endl;
    return;
  }

  for (rd::RasterTriangle& tri : _RasterTrianglesBuf[iBufferIndex])
  {
    float xMin = std::max(0.f, std::min(tri._BBox._Low.x, static_cast<float>(RenderWidth() - 1.f)));
    float yMin = std::max(0.f, std::min(tri._BBox._Low.y, static_cast<float>(RenderHeight() - 1.f)));
    float xMax = std::max(0.f, std::min(tri._BBox._High.x, static_cast<float>(RenderWidth() - 1.f)));
    float yMax = std::max(0.f, std::min(tri._BBox._High.y, static_cast<float>(RenderHeight() - 1.f)));

    int tileXMin = std::max(0, static_cast<int>(xMin / _TileSize));
    int tileYMin = std::max(0, static_cast<int>(yMin / _TileSize));
    int tileXMax = std::min(_TileCountX - 1, static_cast<int>(xMax / _TileSize));
    int tileYMax = std::min(_TileCountY - 1, static_cast<int>(yMax / _TileSize));

    for (int ty = tileYMin; ty <= tileYMax; ++ty)
    {
      for (int tx = tileXMin; tx <= tileXMax; ++tx)
      {
        if ((tx < _TileCountX) && (ty < _TileCountY))
        {
          int tileIndex = ty * _TileCountX + tx;
          rd::Tile& curTile = _Tiles[tileIndex];
          curTile._RasterTrisBins[iBufferIndex].push_back(&tri);
        }
      }
    }
  }
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessFragments(RasterData::Tile& ioTile,
                                          const RasterData::DefaultUniform& iUniforms)
{
  std::unique_ptr<SoftwareFragmentShader> ownedFragmentShader;
  if (_DebugMode & (int)RasterDebugModes::DepthBuffer)
    ownedFragmentShader = std::make_unique<DepthFragmentShader>(iUniforms);
  else if (_DebugMode & (int)RasterDebugModes::Normals)
    ownedFragmentShader = std::make_unique<NormalFragmentShader>(iUniforms);
  else if (ShadingType::PBR == _Settings._ShadingType)
    ownedFragmentShader = std::make_unique<PBRFragmentShader>(iUniforms);
  else
    ownedFragmentShader = std::make_unique<BlinnPhongFragmentShader>(iUniforms);
  SoftwareFragmentShader * fragmentShader = ownedFragmentShader.get();

  const bool renderWires = _DebugMode & (int)RasterDebugModes::Wires;
  std::unique_ptr<SoftwareFragmentShader> ownedWireShader;
  SoftwareFragmentShader * wireShaderPtr = nullptr;
  if ( renderWires )
  {
    ownedWireShader = std::make_unique<WireFrameFragmentShader>(iUniforms);
    wireShaderPtr = ownedWireShader.get();
  }

  const auto shadeFragment = [&](rd::Fragment & ioFragment, const rd::RasterTriangle & iTriangle, unsigned int iPixelIndex) {
    if ( _EnableSIMD )
    {
#if defined(SIMD_AVX2)
      rd::Varying::InterpolateAVX2(_ProjVerticesBuf[iTriangle._Indices[0]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[1]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[2]]._Attrib, ioFragment._Weights, ioFragment._Attrib);
#elif defined (SIMD_ARM_NEON)
      rd::Varying::InterpolateARM(_ProjVerticesBuf[iTriangle._Indices[0]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[1]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[2]]._Attrib, ioFragment._Weights, ioFragment._Attrib);
#else
      rd::Varying::Interpolate(_ProjVerticesBuf[iTriangle._Indices[0]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[1]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[2]]._Attrib, ioFragment._Weights, ioFragment._Attrib);
#endif
    }
    else
      rd::Varying::Interpolate(_ProjVerticesBuf[iTriangle._Indices[0]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[1]]._Attrib, _ProjVerticesBuf[iTriangle._Indices[2]]._Attrib, ioFragment._Weights, ioFragment._Attrib);

    if (ShadingType::Flat == _Settings._ShadingType)
      ioFragment._Attrib._Normal = iTriangle._Normal;

    ioFragment._Attrib._LOD = iTriangle._LOD;

    Vec4 fragColor = fragmentShader->Process(ioFragment, iTriangle);

    if (renderWires)
    {
      Vec4 wireColor = wireShaderPtr->Process(ioFragment, iTriangle);
      fragColor.x = glm::mix(fragColor.x, wireColor.x, wireColor.w);
      fragColor.y = glm::mix(fragColor.y, wireColor.y, wireColor.w);
      fragColor.z = glm::mix(fragColor.z, wireColor.z, wireColor.w);
    }
    if ( _EnableDirectColorWrites && _EnableCompactHits )
    {
      const unsigned int localX = iPixelIndex % ioTile._Width;
      const unsigned int localY = iPixelIndex / ioTile._Width;
      _ImageBuffer._ColorBuffer[ioTile._X + localX + RenderWidth() * (ioTile._Y + localY)] = fragColor;
    }
    else
      ioTile._LocalFB._ColorBuffer[iPixelIndex] = fragColor;
    ioTile._ShadedCount++;
  };

  if ( _EnableCompactHits )
  {
    for ( unsigned int pixelIndex : ioTile._CoveredIndices )
    {
      const rd::CompactHit & hit = ioTile._CompactHits[pixelIndex];
      if ( !hit._Triangle )
        continue;
      const unsigned int localX = pixelIndex % ioTile._Width;
      const unsigned int localY = pixelIndex / ioTile._Width;
      rd::Fragment fragment;
      fragment._PixelCoords = Vec2i(ioTile._X + localX, ioTile._Y + localY);
      fragment._FragCoords = Vec3(fragment._PixelCoords.x + .5f, fragment._PixelCoords.y + .5f, hit._Depth);
      fragment._Weights[0] = hit._Weights[0];
      fragment._Weights[1] = hit._Weights[1];
      fragment._Weights[2] = hit._Weights[2];
      shadeFragment(fragment, *hit._Triangle, pixelIndex);
    }
  }
  else
  {
    for ( rd::Fragment & fragment : ioTile._Fragments )
    {
      const unsigned int pixelIndex = (fragment._PixelCoords.x - ioTile._X) + (fragment._PixelCoords.y - ioTile._Y) * ioTile._Width;
      if ( !ioTile._CoveredPixels[pixelIndex] )
        continue;
      const rd::RasterTriangle * triangle = ioTile._RasterTrisBins[fragment._RasterTriIdx.x][fragment._RasterTriIdx.y];
      if ( triangle )
        shadeFragment(fragment, *triangle, pixelIndex);
    }
  }

  if ( !_EnableDirectColorWrites || !_EnableCompactHits )
    this->CopyTileToMainBuffer(ioTile);
}

}
