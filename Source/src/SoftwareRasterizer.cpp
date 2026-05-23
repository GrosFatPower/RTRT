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

  if (_DirtyStates & (unsigned long)DirtyState::RenderSettings)
  {
    this->ResizeRenderTarget();
    this->UpdateNumberOfWorkers();
  }

  if (_DirtyStates & (unsigned long)DirtyState::SceneEnvMap)
    this->ReloadEnvMap();

  if (_DirtyStates & (unsigned long)DirtyState::SceneInstances)
  {
    if ( CanRefreshSceneInstanceTransforms() )
    {
      if ( 0 != this->RefreshSceneInstanceTransforms() )
        return 1;
    }
    else if ( 0 != this->ReloadScene() )
      return 1;
  }

  this->UpdateImageBuffer();

  this->UpdateTextures();

  this->UpdateRenderToTextureUniforms();
  this->UpdateRenderToScreenUniforms();

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
  oTimings.push_back({ "Process vertices", _PassTimes[TimingProcessVertices], false, _PassEnabled[TimingProcessVertices] });
  oTimings.push_back({ "Clip triangles", _PassTimes[TimingClipTriangles], false, _PassEnabled[TimingClipTriangles] });
  oTimings.push_back({ "Rasterize", _PassTimes[TimingRasterize], false, _PassEnabled[TimingRasterize] });
  oTimings.push_back({ "Process fragments", _PassTimes[TimingProcessFragments], false, _PassEnabled[TimingProcessFragments] });
  oTimings.push_back({ "Render scene", _PassTimes[TimingRenderScene], false, _PassEnabled[TimingRenderScene] });
  oTimings.push_back({ "Copy to render target", _PassTimes[TimingCopyToRenderTarget], true, _PassEnabled[TimingCopyToRenderTarget] });
  oTimings.push_back({ "Composite / screen", _PassTimes[TimingCompositeScreen], true, _PassEnabled[TimingCompositeScreen] });
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateTextures
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateTextures()
{
  glBindTexture(GL_TEXTURE_2D, _ColorBufferTEX._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_ImageBuffer._ColorBuffer[0]);
  glBindTexture(GL_TEXTURE_2D, 0);

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
  std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), zFar);

  float top, right;
  Mat4x4 P;
  _Scene.GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  ResetTiles();

  RenderBackground(top, right);

  RenderScene();

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
  colorBufferDesc._Data           = &_ImageBuffer._ColorBuffer[0];
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
  _ProjVerticesBuf.clear();
  for ( auto & rasterTriangles : _RasterTrianglesBuf )
    rasterTriangles.clear();
  for ( auto & fragments : _Fragments )
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

  for ( int instID = 0; instID < static_cast<int>(meshInstances.size()); ++instID )
  {
    const MeshInstance & meshInst = meshInstances[instID];
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
  }

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

  for ( const RasterSourceVertex & sourceVertex : _VertexSources )
  {
    if ( ( sourceVertex._MeshInstanceID < 0 ) || ( sourceVertex._MeshInstanceID >= static_cast<int>(meshInstances.size()) ) )
      return false;

    const MeshInstance & meshInst = meshInstances[sourceVertex._MeshInstanceID];
    if ( !meshInst._Visible )
      return false;

    if ( meshInst._MeshID != sourceVertex._MeshID )
      return false;

    if ( ( sourceVertex._MeshID < 0 ) || ( sourceVertex._MeshID >= static_cast<int>(meshes.size()) ) )
      return false;

    Mesh * curMesh = meshes[sourceVertex._MeshID];
    if ( !curMesh )
      return false;

    if ( ( sourceVertex._VertexID < 0 ) || ( sourceVertex._VertexID >= static_cast<int>(curMesh -> GetVertices().size()) ) )
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

  for ( int i = 0; i < static_cast<int>(_VertexBuffer.size()); ++i )
  {
    const RasterSourceVertex & sourceVertex = _VertexSources[i];
    const MeshInstance & meshInst = meshInstances[sourceVertex._MeshInstanceID];
    Mesh * curMesh = meshes[sourceVertex._MeshID];

    const std::vector<Vec3> & vertices = curMesh -> GetVertices();
    const std::vector<Vec3> & normals = curMesh -> GetNormals();

    Vec4 transformedVtx = meshInst._Transform * Vec4(vertices[sourceVertex._VertexID], 1.f);
    _VertexBuffer[i]._WorldPos = Vec3(transformedVtx);

    if ( ( sourceVertex._NormalID >= 0 ) && ( sourceVertex._NormalID < static_cast<int>(normals.size()) ) )
    {
      Mat4x4 trInvTransfo = glm::transpose(glm::inverse(meshInst._Transform));
      Vec4 transformedNormal = trInvTransfo * Vec4(normals[sourceVertex._NormalID], 0.f);
      _VertexBuffer[i]._Normal = glm::normalize(Vec3(transformedNormal));
    }
  }

  for ( RasterData::Triangle & tri : _Triangles )
  {
    const Vec3 & p0 = _VertexBuffer[tri._Indices[0]]._WorldPos;
    const Vec3 & p1 = _VertexBuffer[tri._Indices[1]]._WorldPos;
    const Vec3 & p2 = _VertexBuffer[tri._Indices[2]]._WorldPos;
    tri._Normal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
  }

  _FrameNum = 0;

  return 0;
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
      if ( _GenerateMipMaps )
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
  if (_GenerateMipMaps == iGenerate)
    return;

  _GenerateMipMaps = iGenerate;

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
    for (auto& bin : tile._RasterTrisBins)
    {
      bin.clear();
      //bin.reserve(100);
    }

    //tile._Fragments.clear();
    //tile._Fragments.reserve(tile._Width * tile._Height);

    std::fill(policy, tile._CoveredPixels.begin(), tile._CoveredPixels.end(), false);

    std::fill(policy, tile._LocalFB._ColorBuffer.begin(), tile._LocalFB._ColorBuffer.end(), S_DefaultColor);
    std::fill(policy, tile._LocalFB._DepthBuffer.begin(), tile._LocalFB._DepthBuffer.end(), MAX_FLOAT);
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
      for (auto& tile : _Tiles)
        JobSystem::Get().Execute([this, bottomLeft, dX, dY, &tile]() { this->RenderBackground(bottomLeft, dX, dY, tile); });
    }
    else
    {
      for (int i = 0; i < height; ++i)
        JobSystem::Get().Execute([this, i, bottomLeft, dX, dY]() { this->RenderBackgroundRows(i, i + 1, bottomLeft, dX, dY); });
    }

    JobSystem::Get().Wait();
  }
  else
  {
    const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
    std::fill(policy, _ImageBuffer._ColorBuffer.begin(), _ImageBuffer._ColorBuffer.end(), backgroundColor);
  }

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

  const int chunkSize = 512;
  int curInd = 0;
  do
  {
    int nextInd = std::min(curInd + chunkSize, nbVertices);

    if (_EnableSIMD)
    {
#if defined(SIMD_AVX2)
      JobSystem::Get().Execute([this, M, V, P, curInd, nextInd]() { this->ProcessVerticesAVX2(M, V, P, curInd, nextInd); });
#elif defined(SIMD_ARM_NEON)
      JobSystem::Get().Execute([this, M, V, P, curInd, nextInd]() { this->ProcessVerticesARM(M, V, P, curInd, nextInd); });
#else
      JobSystem::Get().Execute([this, M, V, P, curInd, nextInd]() { this->ProcessVertices(M, V, P, curInd, nextInd); });
#endif
    }
    else
      JobSystem::Get().Execute([this, M, V, P, curInd, nextInd]() { this->ProcessVertices(M, V, P, curInd, nextInd); });

    curInd = nextInd;

  } while (curInd < nbVertices);

  JobSystem::Get().Wait();

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
    for (unsigned int i = 0; i < _NbJobs; ++i)
    {
      JobSystem::Get().Execute([this, i]() { this->BinTrianglesToTiles(i); });
    }
    JobSystem::Get().Wait();

    for (auto& tile : _Tiles)
    {
      unsigned int totalTris = 0;
      for (auto& bin : tile._RasterTrisBins)
        totalTris += static_cast<int>(bin.size());

      if (totalTris)
      {
        if (_EnableSIMD)
        {
#if defined(SIMD_AVX2)
          JobSystem::Get().Execute([this, &tile]() { this->RasterizeAVX2(tile); });
#elif defined (SIMD_ARM_NEON)
          JobSystem::Get().Execute([this, &tile]() { this->RasterizeARM(tile); });
#else
          JobSystem::Get().Execute([this, &tile]() { this->Rasterize(tile); });
#endif
        }
        else
          JobSystem::Get().Execute([this, &tile]() { this->Rasterize(tile); });
      }
    }
    JobSystem::Get().Wait();
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

  for (unsigned int i = 0; i < _NbJobs; ++i)
  {
    for (int j = 0; j < _RasterTrianglesBuf[i].size(); ++j)
    {
      rd::RasterTriangle& tri = _RasterTrianglesBuf[i][j];

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

          ioTile._CoveredPixels[localPixelIndex] = true;

          // Setup fragment
          rd::Fragment & frag = ioTile._Fragments[localPixelIndex];
          frag._FragCoords = coord;
          frag._RasterTriIdx.x = i;
          frag._RasterTriIdx.y = j,
          frag._Weights[0] = W[0];
          frag._Weights[1] = W[1];
          frag._Weights[2] = W[2];
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

            ioTile._LocalFB._DepthBuffer[localPixelIndex + k] = ( _Settings._WBuffer ) ? ( depth ) : ( z );
            ioTile._CoveredPixels[localPixelIndex + k] = true;

            rd::Fragment & frag = ioTile._Fragments[localPixelIndex + k];
            frag._FragCoords.x = x + k + .5f;
            frag._FragCoords.y = y + .5f;
            frag._FragCoords.z = z;
            frag._RasterTriIdx.x = i;
            frag._RasterTriIdx.y = j,
            frag._Weights[0] = Weights[0].m256_f32[k];
            frag._Weights[1] = Weights[1].m256_f32[k];
            frag._Weights[2] = Weights[2].m256_f32[k];
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

            ioTile._LocalFB._DepthBuffer[localPixelIndex + k] = ( _Settings._WBuffer ) ? ( depth ) : ( z );
            ioTile._CoveredPixels[localPixelIndex + k] = true;

            rd::Fragment & frag = ioTile._Fragments[localPixelIndex + k];
            frag._FragCoords.x = x + k + .5f;
            frag._FragCoords.y = y + .5f;
            frag._FragCoords.z = z;
            frag._RasterTriIdx.x = i;
            frag._RasterTriIdx.y = j,
            frag._Weights[0] = SIMDUtils::GetVectorElement(Weights[0], k);
            frag._Weights[1] = SIMDUtils::GetVectorElement(Weights[1], k);
            frag._Weights[2] = SIMDUtils::GetVectorElement(Weights[2], k);
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
    for (auto& tile : _Tiles)
    {
      if ( tile._Fragments.size() )
        JobSystem::Get().Execute([this, &tile, uniforms]() { this->ProcessFragments(tile, uniforms); });
      else
        JobSystem::Get().Execute([this, &tile, uniforms]() { this->CopyTileToMainBuffer(tile); });
    }
    JobSystem::Get().Wait();
  }
  else
  {
    for (unsigned int i = 0; i < _NbJobs; ++i)
    {
      if ( _Fragments[i].size() )
        JobSystem::Get().Execute([this, i, uniforms]() { this->ProcessFragments(i, uniforms); });
    }
    JobSystem::Get().Wait();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessFragments(int iThreadBin, const RasterData::DefaultUniform& iUniforms)
{
  std::unique_ptr<SoftwareFragmentShader> fragmentShader = nullptr;
  if (_DebugMode & (int)RasterDebugModes::DepthBuffer)
    fragmentShader = std::make_unique<DepthFragmentShader>(iUniforms);
  else if (_DebugMode & (int)RasterDebugModes::Normals)
    fragmentShader = std::make_unique<NormalFragmentShader>(iUniforms);
  else if (ShadingType::PBR == _Settings._ShadingType)
    fragmentShader = std::make_unique<PBRFragmentShader>(iUniforms);
  else
    fragmentShader = std::make_unique<BlinnPhongFragmentShader>(iUniforms);
  if (!fragmentShader)
    return;

  std::unique_ptr<SoftwareFragmentShader> wireShader = nullptr;
  if (_DebugMode & (int)RasterDebugModes::Wires)
    wireShader = std::make_unique<WireFrameFragmentShader>(iUniforms);

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

    if (wireShader)
    {
      Vec4 wireColor = wireShader->Process(frag, tri);
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
void SoftwareRasterizer::ProcessFragments(RasterData::Tile& ioTile, const RasterData::DefaultUniform& iUniforms)
{
  std::unique_ptr<SoftwareFragmentShader> fragmentShader = nullptr;
  if (_DebugMode & (int)RasterDebugModes::DepthBuffer)
    fragmentShader = std::make_unique<DepthFragmentShader>(iUniforms);
  else if (_DebugMode & (int)RasterDebugModes::Normals)
    fragmentShader = std::make_unique<NormalFragmentShader>(iUniforms);
  else if (ShadingType::PBR == _Settings._ShadingType)
    fragmentShader = std::make_unique<PBRFragmentShader>(iUniforms);
  else
    fragmentShader = std::make_unique<BlinnPhongFragmentShader>(iUniforms);
  if (!fragmentShader)
    return;

  std::unique_ptr<SoftwareFragmentShader> wireShader = nullptr;
  if (_DebugMode & (int)RasterDebugModes::Wires)
    wireShader = std::make_unique<WireFrameFragmentShader>(iUniforms);

  for (auto it = ioTile._Fragments.rbegin(); it != ioTile._Fragments.rend(); ++it)
  {
    rd::Fragment & frag = *it;

    unsigned int pixelIndex = (frag._PixelCoords.x - ioTile._X) + (frag._PixelCoords.y - ioTile._Y) * ioTile._Width;
    if ( !ioTile._CoveredPixels[pixelIndex] )
      continue;

    // Finalize fragment
    const rd::RasterTriangle * tri = ioTile._RasterTrisBins[frag._RasterTriIdx.x][frag._RasterTriIdx.y];
    if ( !tri )
      continue;

    if ( _EnableSIMD )
    {
#if defined(SIMD_AVX2)
      rd::Varying::InterpolateAVX2(_ProjVerticesBuf[tri -> _Indices[0]]._Attrib, _ProjVerticesBuf[tri -> _Indices[1]]._Attrib, _ProjVerticesBuf[tri -> _Indices[2]]._Attrib, frag._Weights, frag._Attrib);
#elif defined (SIMD_ARM_NEON)
      rd::Varying::InterpolateARM(_ProjVerticesBuf[tri -> _Indices[0]]._Attrib, _ProjVerticesBuf[tri -> _Indices[1]]._Attrib, _ProjVerticesBuf[tri -> _Indices[2]]._Attrib, frag._Weights, frag._Attrib);
#else
      rd::Varying::Interpolate(_ProjVerticesBuf[tri -> _Indices[0]]._Attrib, _ProjVerticesBuf[tri -> _Indices[1]]._Attrib, _ProjVerticesBuf[tri -> _Indices[2]]._Attrib, frag._Weights, frag._Attrib);
#endif
    }
    else
      rd::Varying::Interpolate(_ProjVerticesBuf[tri -> _Indices[0]]._Attrib, _ProjVerticesBuf[tri -> _Indices[1]]._Attrib, _ProjVerticesBuf[tri -> _Indices[2]]._Attrib, frag._Weights, frag._Attrib);

    if (ShadingType::Flat == _Settings._ShadingType)
      frag._Attrib._Normal = tri -> _Normal;

    frag._Attrib._LOD = tri -> _LOD;

    // Shade fragment
    Vec4 fragColor = fragmentShader->Process(frag, *tri);

    if (wireShader)
    {
      Vec4 wireColor = wireShader->Process(frag, *tri);
      fragColor.x = glm::mix(fragColor.x, wireColor.x, wireColor.w);
      fragColor.y = glm::mix(fragColor.y, wireColor.y, wireColor.w);
      fragColor.z = glm::mix(fragColor.z, wireColor.z, wireColor.w);
    }

    unsigned int localX = frag._PixelCoords.x - ioTile._X;
    unsigned int localY = frag._PixelCoords.y - ioTile._Y;
    unsigned int localPixelIndex = localY * ioTile._Width + localX;
    ioTile._LocalFB._ColorBuffer[localPixelIndex] = fragColor;
  }

  this->CopyTileToMainBuffer(ioTile);
}

}
