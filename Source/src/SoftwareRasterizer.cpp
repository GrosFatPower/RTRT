#include "SoftwareRasterizer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "ShaderProgram.h"
#include "SutherlandHodgman.h"
#include "JobSystem.h"

#include <string>
#include <iostream>
#include <execution>
#include <unordered_map>
//#include <omp.h>
#include <thread>

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "stb_image_write.h"


#define PARALLEL
#ifdef PARALLEL
constexpr std::execution::parallel_policy policy = std::execution::par;
#else
constexpr std::execution::sequenced_policy policy = std::execution::seq;
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
  struct hash<RTRT::SoftwareRasterizer::Vertex>
  {
    size_t operator()(RTRT::SoftwareRasterizer::Vertex const & iV) const
    {
      return
        ( (hash<Vec3>()(iV._WorldPos))
        ^ (hash<Vec3>()(iV._Normal))
        ^ (hash<Vec2>()(iV._UV)) );
    }
  };
}


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
  UpdateNumberOfWorkers(true);
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
// Initialize
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateNumberOfWorkers( bool iForce )
{
  if ( ( _NbJobs != _Settings._NbThreads ) || iForce )
  {
    _NbJobs = std::min(_Settings._NbThreads, std::thread::hardware_concurrency());

    JobSystem::Get().Initialize(_NbJobs);

    _RasterTrianglesBuf.resize(_NbJobs);

    for ( int i = 0; i < _NbJobs; ++i )
      _RasterTrianglesBuf[i].reserve(std::max(_Triangles.size()/_NbJobs, (size_t)1));
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
  {
    this -> ResizeRenderTarget();
    this -> UpdateNumberOfWorkers();
  }

  if ( _DirtyStates & (unsigned long)DirtyState::SceneEnvMap )
    this -> ReloadEnvMap();

  this -> UpdateImageBuffer();

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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_ImageBuffer._ColorBuffer[0]);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateImageBuffer
// ----------------------------------------------------------------------------
int SoftwareRasterizer::UpdateImageBuffer()
{
  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;
  float ratio = width / float(height);

  Mat4x4 MV;
  _Scene.GetCamera().ComputeLookAtMatrix(MV);

  float top, right;
  Mat4x4 P;
  _Scene.GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  Mat4x4 RasterM;
  _Scene.GetCamera().ComputeRasterMatrix(width, height, RasterM);

  Mat4x4 MVP = P * MV;

  RenderBackground(top, right);

  RenderScene(MV, P, RasterM);

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
  _RenderToScreenShader -> SetUniform("u_ToneMapping", _Settings._ToneMapping);
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_ImageBuffer._ColorBuffer[0]);
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
  GLUtil::DeleteTEX(_EnvMapTEX);

  _FrameNum = 0;

  _Vertices.clear();
  _Triangles.clear();
  _ProjVerticesBuf.clear();

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

  // Load _Triangles
  const std::vector<Vec3i>    & Indices   = _Scene.GetIndices();
  const std::vector<Vec3>     & Vertices  = _Scene.GetVertices();
  const std::vector<Vec3>     & Normals   = _Scene.GetNormals();
  const std::vector<Vec3>     & UVMatIDs  = _Scene.GetUVMatID();
  const std::vector<Material> & Materials = _Scene.GetMaterials();
  const std::vector<Texture*> & Textures  = _Scene.GetTextures();
  const int nbTris = Indices.size() / 3;

  std::unordered_map<Vertex, int> VertexIDs;
  VertexIDs.reserve(Vertices.size());

  _Triangles.resize(nbTris);
  for ( int i = 0; i < nbTris; ++i )
  {
    Triangle & tri = _Triangles[i];

    Vec3i Index[3];
    Index[0] = Indices[i*3];
    Index[1] = Indices[i*3+1];
    Index[2] = Indices[i*3+2];

    Vertex Vert[3];
    for ( int j = 0; j < 3; ++j )
    {
      Vert[j]._WorldPos = Vertices[Index[j].x];
      Vert[j]._UV       = Vec2(0.f);
      Vert[j]._Normal   = Vec3(0.f);
    }

    Vec3 vec1(Vert[1]._WorldPos.x-Vert[0]._WorldPos.x, Vert[1]._WorldPos.y-Vert[0]._WorldPos.y, Vert[1]._WorldPos.z-Vert[0]._WorldPos.z);
    Vec3 vec2(Vert[2]._WorldPos.x-Vert[0]._WorldPos.x, Vert[2]._WorldPos.y-Vert[0]._WorldPos.y, Vert[2]._WorldPos.z-Vert[0]._WorldPos.z);
    tri._Normal = glm::normalize(glm::cross(vec1, vec2));

    for ( int j = 0; j < 3; ++j )
    {
      if ( Index[j].y >= 0 )
        Vert[j]._Normal = Normals[Index[j].y];
      else
        Vert[j]._Normal = tri._Normal;

      if ( Index[j].z >= 0 )
         Vert[j]._UV = Vec2(UVMatIDs[Index[j].z].x, UVMatIDs[Index[j].z].y);
    }

    tri._MatID = (int)UVMatIDs[Index[0].z].z;

    for ( int j = 0; j < 3; ++j )
    {
      int idx = 0;
      if ( 0 == VertexIDs.count(Vert[j]) )
      {
        idx = (int)_Vertices.size();
        VertexIDs[Vert[j]] = idx;
        _Vertices.push_back(Vert[j]);
      }
      else
        idx = VertexIDs[Vert[j]];

      tri._Indices[j] = idx;
    }
  }

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

    _Scene.GetEnvMap().SetGLTexID(_EnvMapTEX._ID);
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

// ----------------------------------------------------------------------------
// SampleEnvMap
// ----------------------------------------------------------------------------
Vec4 SoftwareRasterizer::SampleEnvMap( const Vec3 & iDir )
{
  std::vector<Texture*> & textures = _Scene.GetTextures();

  if ( ( _Scene.GetEnvMap().IsInitialized() ) )
  {
    float theta = std::asin(iDir.y);
    float phi   = std::atan2(iDir.z, iDir.x);
    Vec2 uv = Vec2(.5f + phi * M_1_PI * .5f, .5f - theta * M_1_PI) + Vec2(_Settings._SkyBoxRotation, 0.0);

    if ( _Settings._BilinearSampling )
      return _Scene.GetEnvMap().BiLinearSample(uv);
    else
      return _Scene.GetEnvMap().Sample(uv);
  }

  return Vec4(0.f);
}

// ----------------------------------------------------------------------------
// VertexShader
// ----------------------------------------------------------------------------
void SoftwareRasterizer::VertexShader( const Vec4 & iVertexPos, const Vec2 & iUV, const Vec3 iNormal, const Mat4x4 iMVP, ProjectedVertex & oProjectedVertex )
{
  oProjectedVertex._ProjPos          = iMVP * iVertexPos;
  oProjectedVertex._Attrib._WorldPos = iVertexPos;
  oProjectedVertex._Attrib._UV       = iUV;
  oProjectedVertex._Attrib._Normal   = iNormal;
}

// ----------------------------------------------------------------------------
// FragmentShader_Color
// ----------------------------------------------------------------------------
void SoftwareRasterizer::FragmentShader_Color( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor )
{
  Vec4 albedo;
  if ( iFrag._MatID >= 0 )
  {
    const Material & mat = (*iUniforms._Materials)[iFrag._MatID];
    if ( mat._BaseColorTexId >= 0 )
    {
      const Texture * tex = (*iUniforms._Textures)[mat._BaseColorTexId];
      if ( iUniforms._BilinearSampling )
        albedo = tex -> BiLinearSample(iFrag._Attrib._UV);
      else
        albedo = tex -> Sample(iFrag._Attrib._UV);
    }
    else
      albedo = Vec4(mat._Albedo, 1.f);
  }

  // Shading
  Vec4 alpha(0.f, 0.f, 0.f, 0.f);
  for ( const auto & light : iUniforms._Lights )
  {
    float ambientStrength = .1f;
    float diffuse = 0.f;
    float specular = 0.f;

    Vec3 dirToLight = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);
    diffuse = std::max(0.f, glm::dot(iFrag._Attrib._Normal, dirToLight));

    Vec3 viewDir =  glm::normalize(iUniforms._CameraPos - iFrag._Attrib._WorldPos);
    Vec3 reflectDir = glm::reflect(-dirToLight, iFrag._Attrib._Normal);

    static float specularStrength = 0.5f;
    specular = pow(std::max(glm::dot(viewDir, reflectDir), 0.f), 32) * specularStrength;

    alpha += std::min(diffuse+ambientStrength+specular, 1.f) * Vec4(glm::normalize(light._Emission), 1.f);
  }

  oColor = MathUtil::Min(albedo * alpha, Vec4(1.f));
}

// ----------------------------------------------------------------------------
// FragmentShader_Depth
// ----------------------------------------------------------------------------
void SoftwareRasterizer::FragmentShader_Depth( const Fragment  & iFrag, Uniform & iUniforms, Vec4 & oColor )
{
  oColor = Vec4(Vec3(iFrag._FragCoords.z + 1.f) * .5f, 1.f);
  return;
}

// ----------------------------------------------------------------------------
// FragmentShader_Normal
// ----------------------------------------------------------------------------
void SoftwareRasterizer::FragmentShader_Normal( const Fragment  & iFrag, Uniform & iUniforms, Vec4 & oColor )
{
  oColor = Vec4(glm::abs(iFrag._Attrib._Normal),1.f);
  return;
}

// ----------------------------------------------------------------------------
// FragmentShader_Wires
// ----------------------------------------------------------------------------
void SoftwareRasterizer::FragmentShader_Wires( const Fragment & iFrag, const Vec3 iVertCoord[3], Uniform & iUniforms, Vec4 & oColor )
{
  Vec2 P(iFrag._FragCoords);
  if ( ( MathUtil::DistanceToSegment(iVertCoord[0], iVertCoord[1], P) <= 1.f )
    || ( MathUtil::DistanceToSegment(iVertCoord[1], iVertCoord[2], P) <= 1.f )
    || ( MathUtil::DistanceToSegment(iVertCoord[2], iVertCoord[0], P) <= 1.f ) )
  {
    oColor = Vec4(1.f, 0.f, 0.f, 1.f);
  }
  else
    oColor = Vec4(0.f, 0.f, 0.f, 0.f);
}

// ----------------------------------------------------------------------------
// RenderBackground
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderBackground( float iTop, float iRight )
{
  double startTime = glfwGetTime();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  if ( _Settings._EnableBackGround )
  {
    Vec3 bottomLeft = _Scene.GetCamera().GetForward() * zNear - iRight * _Scene.GetCamera().GetRight() - iTop * _Scene.GetCamera().GetUp();
    Vec3 dX = _Scene.GetCamera().GetRight() * ( 2 * iRight / width );
    Vec3 dY = _Scene.GetCamera().GetUp() * ( 2 * iTop / height );

    for ( int i = 0; i < height; ++i )
      JobSystem::Get().Execute([this, i, bottomLeft, dX, dY](){ this -> RenderBackgroundRows(i, i+1, bottomLeft, dX, dY); });

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
void SoftwareRasterizer::RenderBackgroundRows( int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY )
{
  int width  = _Settings._RenderResolution.x;

  for ( int y = iStartY; y < iEndY; ++y )
  {
    for ( int x = 0; x < width; ++x  )
    {
      Vec3 worldP = glm::normalize(iBottomLeft + iDX * (float)x + iDY * (float)y);
      _ImageBuffer._ColorBuffer[x + width * y] = this -> SampleEnvMap(worldP);
    }
  }
}

// ----------------------------------------------------------------------------
// RenderScene
// ----------------------------------------------------------------------------
int SoftwareRasterizer::RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP, const Mat4x4 & iRasterM )
{
  if ( _Settings._WBuffer )
  {
    float zNear, zFar;
    _Scene.GetCamera().GetZNearFar(zNear, zFar);
    std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), zFar);
  }
  else
    std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), 1.f);

  int ko = ProcessVertices(iMV, iP);

  if ( !ko )
    ko = ClipTriangles(iRasterM);

  if ( !ko )
    ko = ProcessFragments();

  return ko;
}


// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ProcessVertices( const Mat4x4 & iMV, const Mat4x4 & iP )
{
  Mat4x4 MVP = iP * iMV;

  int nbVertices = _Vertices.size();
  _ProjVerticesBuf.resize(nbVertices);
  _ProjVerticesBuf.reserve(nbVertices*2);

  //for ( int i = 0; i < _NbJobs; ++i )
  //{
  //  int startInd = ( _Vertices.size() / _NbJobs ) * i;
  //  int endInd = ( i == _NbJobs-1 ) ? ( _Vertices.size() ) : ( startInd + ( _Vertices.size() / _NbJobs ) );
  //
  //  JobSystem::Get().Execute([this, MVP, startInd, endInd](){ this -> ProcessVertices(MVP, startInd, endInd); });
  //}

  int curInd = 0;
  do
  {
    int nextInd = std::min(curInd + 512, nbVertices);

    JobSystem::Get().Execute([this, MVP, curInd, nextInd](){ this -> ProcessVertices(MVP, curInd, nextInd); });

    curInd = nextInd;

  } while ( curInd < nbVertices );

  JobSystem::Get().Wait();

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessVertices( const Mat4x4 & iMVP, int iStartInd, int iEndInd )
{
  for ( int i = iStartInd; i < iEndInd; ++i )
  {
    Vertex & vert = _Vertices[i];

    ProjectedVertex & projVtx = _ProjVerticesBuf[i];
    VertexShader(Vec4(vert._WorldPos ,1.f), vert._UV, vert._Normal, iMVP, projVtx);
  }
}

// ----------------------------------------------------------------------------
// ClipTriangles
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ClipTriangles( const Mat4x4 & iRasterM )
{
  int nbTriangles = _Triangles.size();

  for ( int i = 0; i < _NbJobs; ++i )
  {
    int startInd = ( nbTriangles / _NbJobs ) * i;
    int endInd = ( i == _NbJobs-1 ) ? ( nbTriangles ) : ( startInd + ( nbTriangles / _NbJobs ) );
    if ( startInd >= endInd )
      continue;
  
    JobSystem::Get().Execute([this, iRasterM, i, startInd, endInd](){ this -> ClipTriangles(iRasterM, i, startInd, endInd); });
  }

  JobSystem::Get().Wait();

  return 0;
}

// ----------------------------------------------------------------------------
// ClipTriangles
// SutherlandHodgman algorithm
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ClipTriangles( const Mat4x4 & iRasterM, int iThreadBin, int iStartInd, int iEndInd )
{
  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  _RasterTrianglesBuf[iThreadBin].clear();
  for ( int i = iStartInd; i < iEndInd; ++i )
  {
    Triangle & tri = _Triangles[i];

    uint32_t clipCode0 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[0]]._ProjPos);
    uint32_t clipCode1 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[1]]._ProjPos);
    uint32_t clipCode2 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[2]]._ProjPos);

    if (clipCode0 | clipCode1 | clipCode2)
    {
      // Check the clipping codes correctness
      if ( !(clipCode0 & clipCode1 & clipCode2) )
      {
        Polygon poly = SutherlandHodgman::ClipTriangle(
          _ProjVerticesBuf[tri._Indices[0]]._ProjPos,
          _ProjVerticesBuf[tri._Indices[1]]._ProjPos,
          _ProjVerticesBuf[tri._Indices[2]]._ProjPos,
          (clipCode0 ^ clipCode1) | (clipCode1 ^ clipCode2) | (clipCode2 ^ clipCode0));

        for ( int j = 2; j < poly.Size(); ++j )
        {
          // Preserve winding
          Polygon::Point Points[3] = { poly[0], poly[j - 1], poly[j] };

          RasterTriangle rasterTri;
          for ( int k = 0; k < 3; ++k )
          {
            if ( Points[k]._Distances.x == 1.f )
            {
              rasterTri._Indices[k] = tri._Indices[0]; // == V0
            }
            else if ( Points[k]._Distances.y == 1.f )
            {
              rasterTri._Indices[k] = tri._Indices[1]; // == V1
            }
            else if ( Points[k]._Distances.z == 1.f )
            {
              rasterTri._Indices[k] = tri._Indices[2]; // == V2
            }
            else
            {
              ProjectedVertex newProjVert;
              newProjVert._ProjPos = Points[k]._Pos;
              newProjVert._Attrib  = _ProjVerticesBuf[tri._Indices[0]]._Attrib * Points[k]._Distances.x + 
                                     _ProjVerticesBuf[tri._Indices[1]]._Attrib * Points[k]._Distances.y +
                                     _ProjVerticesBuf[tri._Indices[2]]._Attrib * Points[k]._Distances.z;
              {
                std::unique_lock<std::mutex> lock(_ProjVerticesMutex);
                rasterTri._Indices[k] = _ProjVerticesBuf.size();
                _ProjVerticesBuf.emplace_back(newProjVert);
              }
            }

            Vec3 homogeneousProjPos;
            rasterTri._InvW[k] = 1.f / Points[k]._Pos.w;
            homogeneousProjPos.x = Points[k]._Pos.x * rasterTri._InvW[k];
            homogeneousProjPos.y = Points[k]._Pos.y * rasterTri._InvW[k];
            homogeneousProjPos.z = Points[k]._Pos.z * rasterTri._InvW[k];

            rasterTri._V[k] = MathUtil::TransformPoint(homogeneousProjPos, iRasterM);

            rasterTri._BBox.Insert(rasterTri._V[k]);
          }

          float area = MathUtil::EdgeFunction(rasterTri._V[0], rasterTri._V[1], rasterTri._V[2]);
          if ( area > 0.f )
            rasterTri._InvArea = 1.f / area;
          else
            continue;

          rasterTri._MatID  = tri._MatID;
          rasterTri._Normal = tri._Normal;

          _RasterTrianglesBuf[iThreadBin].push_back(rasterTri);
        }
      }
    }
    else
    {
      // No clipping needed
      RasterTriangle rasterTri;
      for ( int j = 0; j < 3; ++j )
      {
        rasterTri._Indices[j] = tri._Indices[j];

        ProjectedVertex & projVert = _ProjVerticesBuf[tri._Indices[j]];

        Vec3 homogeneousProjPos;
        rasterTri._InvW[j] = 1.f / projVert._ProjPos.w;
        homogeneousProjPos.x = projVert._ProjPos.x * rasterTri._InvW[j];
        homogeneousProjPos.y = projVert._ProjPos.y * rasterTri._InvW[j];
        homogeneousProjPos.z = projVert._ProjPos.z * rasterTri._InvW[j];

        rasterTri._V[j] = MathUtil::TransformPoint(homogeneousProjPos, iRasterM);

        rasterTri._BBox.Insert(rasterTri._V[j]);
      }

      float area = MathUtil::EdgeFunction(rasterTri._V[0], rasterTri._V[1], rasterTri._V[2]);
      if ( area > 0.f )
        rasterTri._InvArea = 1.f / area;
      else
        continue;

      rasterTri._MatID  = tri._MatID;
      rasterTri._Normal = tri._Normal;

      _RasterTrianglesBuf[iThreadBin].push_back(rasterTri);
    }
  }
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
int SoftwareRasterizer::ProcessFragments()
{
  int height = _Settings._RenderResolution.y;

  for ( int i = 0; i < _NbJobs; ++i )
  {
    int startY = ( height / _NbJobs ) * i;
    int endY = ( i == _NbJobs-1 ) ? ( height ) : ( startY + ( height / _NbJobs ) );

    JobSystem::Get().Execute([this, startY, endY](){ this -> ProcessFragments(startY, endY); });
  }

  JobSystem::Get().Wait();

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
void SoftwareRasterizer::ProcessFragments( int iStartY, int iEndY )
{
  const std::vector<Material> & Materials = _Scene.GetMaterials();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  float zNear, zFar;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  Uniform uniforms;
  uniforms._CameraPos        = _Scene.GetCamera().GetPos();
  uniforms._BilinearSampling = _Settings._BilinearSampling;
  uniforms._Materials        = &_Scene.GetMaterials();
  uniforms._Textures         = &_Scene.GetTextures();
  for ( int i = 0; i < _Scene.GetNbLights(); ++i )
    uniforms._Lights.push_back(*_Scene.GetLight(i));

  for ( int i = 0; i < _NbJobs; ++i )
  {
    for ( int j = 0; j < _RasterTrianglesBuf[i].size(); ++j )
    {
      RasterTriangle & tri = _RasterTrianglesBuf[i][j];

      // Backface culling
      if ( 0 )
      {
        Vec3 AB(tri._V[1] - tri._V[0]);
        Vec3 AC(tri._V[2] - tri._V[0]);
        Vec3 crossProd = glm::cross(AB,AC);
        if ( crossProd.z < 0 )
          continue;
      }

      int xMin = std::max(0,       std::min((int)std::floorf(tri._BBox._Low.x),  width - 1));
      int yMin = std::max(iStartY, std::min((int)std::floorf(tri._BBox._Low.y),  iEndY - 1 ));
      int xMax = std::max(0,       std::min((int)std::floorf(tri._BBox._High.x), width - 1));
      int yMax = std::max(iStartY, std::min((int)std::floorf(tri._BBox._High.y), iEndY - 1 ));

      for ( int y = yMin; y <= yMax; ++y )
      {
        for ( int x = xMin; x <= xMax; ++x )
        {
          Vec3 coord(x + .5f, y + .5f, 0.f);

          Vec3 W;
          W.x = MathUtil::EdgeFunction(tri._V[1], tri._V[2], coord);
          W.y = MathUtil::EdgeFunction(tri._V[2], tri._V[0], coord);
          W.z = MathUtil::EdgeFunction(tri._V[0], tri._V[1], coord);
          if ( ( W.x < 0.f )
            || ( W.y < 0.f )
            || ( W.z < 0.f ) )
            continue;

          // Perspective correction
          W *= tri._InvArea;

          W.x *= tri._InvW[0]; // W0 / -z0
          W.y *= tri._InvW[1]; // W1 / -z1
          W.z *= tri._InvW[2]; // W2 / -z2

          float Z = 1.f / (W.x + W.y + W.z);
          W *= Z;

          coord.z = W.x * tri._V[0].z + W.y * tri._V[1].z + W.z * tri._V[2].z;
          if ( _Settings._WBuffer )
          {
            if ( Z > _ImageBuffer._DepthBuffer[x + width * y] || ( Z < zNear ) )
              continue;
          }
          else
          {
            if ( coord.z > _ImageBuffer._DepthBuffer[x + width * y] || ( coord.z < -1.f ) )
              continue;
          }

          Varying Attrib[3];
          Attrib[0] = _ProjVerticesBuf[tri._Indices[0]]._Attrib;
          Attrib[1] = _ProjVerticesBuf[tri._Indices[1]]._Attrib;
          Attrib[2] = _ProjVerticesBuf[tri._Indices[2]]._Attrib;

          Fragment frag;
          frag._FragCoords = coord;
          frag._MatID      = tri._MatID;
          frag._Attrib     = Attrib[0] * W.x + Attrib[1] * W.y + Attrib[2] * W.z;

         if ( ShadingType::Phong == _Settings._ShadingType )
            frag._Attrib._Normal = glm::normalize(frag._Attrib._Normal);
          else
            frag._Attrib._Normal = tri._Normal;

          Vec4 fragColor(1.f);

          if ( _DebugMode & (int)RasterDebugModes::DepthBuffer )
            FragmentShader_Depth(frag, uniforms, fragColor);
          else if ( _DebugMode & (int)RasterDebugModes::Normals )
            FragmentShader_Normal(frag, uniforms, fragColor);
          else
            FragmentShader_Color(frag, uniforms, fragColor);

          if ( _DebugMode & (int)RasterDebugModes::Wires )
          {
            Vec4 wireColor(1.f);
            FragmentShader_Wires(frag, tri._V, uniforms, wireColor);
            fragColor.x = glm::mix(fragColor.x, wireColor.x, wireColor.w);
            fragColor.y = glm::mix(fragColor.y, wireColor.y, wireColor.w);
            fragColor.z = glm::mix(fragColor.z, wireColor.z, wireColor.w);
          }

          _ImageBuffer._ColorBuffer[x + width * y] = fragColor;
          if ( _Settings._WBuffer )
            _ImageBuffer._DepthBuffer[x + width * y] = Z;
          else
            _ImageBuffer._DepthBuffer[x + width * y] = coord.z;

        }
      }
    }
  }
}

}
