#include "DeferredRenderer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "PathUtils.h"
#include "Mesh.h"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <cstdint>
#include <cstring>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "stb_image_write.h"

namespace RTRT
{

static Vec3 S_WireColor = Vec3(1.f, 0.f, 0.f);
static float S_WireWidth = 3.0f;

// ----------------------------------------------------------------------------
// HELPER TYPES
// ----------------------------------------------------------------------------
struct GPUMeshVertex
{
  Vec3 _Pos;
  Vec3 _Normal;
  Vec2 _UV;
};

struct IndexTriplet
{
  int v, n, u;

  bool operator==(IndexTriplet const& iRhs) const noexcept
  {
    return ( ( v == iRhs.v ) && ( n == iRhs.n ) && ( u == iRhs.u ) );
  }
};

struct IndexTripletHash
{
  std::size_t operator()(IndexTriplet const & k) const noexcept
  {
    // Combine three 32-bit ints into a 64-bit hash comfortably
    // simple mixing is fine for our use case
    std::size_t h = static_cast<std::size_t>(k.v) + 0x9e3779b97f4a7c15ull;
    h ^= static_cast<std::size_t>(k.n) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
    h ^= static_cast<std::size_t>(k.u) + 0x9e3779b97f4a7c15ull + (h<<6) + (h>>2);
    return h;
  }
};

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
DeferredRenderer::DeferredRenderer(Scene& iScene, RenderSettings& iSettings)
: Renderer(iScene, iSettings)
{
  // Nothing heavy in ctor; real setup in Initialize()
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
DeferredRenderer::~DeferredRenderer()
{
  // Delete framebuffers and textures
  GLUtil::DeleteFBO(_GBufferFBO);
  GLUtil::DeleteFBO(_LightingFBO);

  GLUtil::DeleteTEX(_GAlbedoTEX);
  GLUtil::DeleteTEX(_GNormalTEX);
  GLUtil::DeleteTEX(_GPositionTEX);
  GLUtil::DeleteTEX(_GDepthTEX);
  GLUtil::DeleteTEX(_LightingTEX);

  GLUtil::DeleteTBO(_TexIndTBO);
  GLUtil::DeleteTEX(_TexArrayTEX);
  GLUtil::DeleteTEX(_MaterialsTEX);
  GLUtil::DeleteTEX(_EnvMapTEX);

  UnloadScene();
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int DeferredRenderer::Initialize()
{
  if ( 0 != ReloadScene() )
  {
    std::cout << "DeferredRenderer : Failed to load scene !" << std::endl;
    return 1;
  }

  if ( 0 != RecompileShaders() )
  {
    std::cout << "DeferredRenderer : Shader compilation failed !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeFrameBuffers() )
  {
    std::cout << "DeferredRenderer : Failed to initialize G-buffer !" << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int DeferredRenderer::Update()
{
  if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    this -> ResizeRenderTarget();

  if ( _DirtyStates & (unsigned long)DirtyState::Textures )
  {
    if ( _GenerateMipMaps )
      GLUtil::SetMinFilter(_TexArrayTEX, GL_LINEAR_MIPMAP_LINEAR);
    else
      GLUtil::SetMinFilter(_TexArrayTEX, GL_LINEAR);

    if ( _GenerateMipMaps && _AnisotropicLevel )
      GLUtil::EnableAnisotropyIfAvailable(_TexArrayTEX, (float)_AnisotropicLevel);
  }

  if ( _DirtyStates & (unsigned long)DirtyState::SceneEnvMap )
    this -> ReloadEnvMap();

  UpdateUniforms();
 
  return 0;
}

// ----------------------------------------------------------------------------
// Done
// ----------------------------------------------------------------------------
int DeferredRenderer::Done()
{
  _FrameNum++;

  CleanStates();

  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int DeferredRenderer::UnloadScene()
{
  _FrameNum = 0;

  // Delete any GPU mesh buffers we created
  const size_t nb = _MeshVAOs.size();
  for (size_t i = 0; i < nb; ++i)
  {
    GLuint vao = _MeshVAOs[i];
    GLuint vbo = _MeshVBOs[i];
    GLuint ebo = _MeshEBOs[i];
    GLUtil::DeleteMeshBuffers(vao, vbo, ebo);
  }

  _MeshVAOs.clear();
  _MeshVBOs.clear();
  _MeshEBOs.clear();
  _MeshIndexCount.clear();

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadScene
// Build GPU buffers (VAO/VBO/EBO) for every mesh found in the Scene.
// ----------------------------------------------------------------------------
int DeferredRenderer::ReloadScene()
{
  UnloadScene();

  if ( ( _Settings._TextureSize.x > 0 ) && ( _Settings._TextureSize.y > 0 ) )
    _Scene.CompileMeshData( _Settings._TextureSize, true, false );

  const std::vector<Mesh*> & meshes = _Scene.GetMeshes();
  const size_t meshCount = meshes.size();

  _MeshVAOs.assign(meshCount, 0u);
  _MeshVBOs.assign(meshCount, 0u);
  _MeshEBOs.assign(meshCount, 0u);
  _MeshIndexCount.assign(meshCount, 0);

  for ( size_t mi = 0; mi < meshCount; ++mi )
  {
    Mesh * mesh = meshes[mi];
    if ( !mesh )
      continue;

    const std::vector<Vec3>  & srcPos  = mesh -> GetVertices();
    const std::vector<Vec3>  & srcNorm = mesh -> GetNormals();
    const std::vector<Vec2>  & srcUV   = mesh -> GetUVs();
    const std::vector<Vec3i> & srcIdx  = mesh -> GetIndices();

    // Map each (posIdx, normIdx, uvIdx) triplet to a unique GPU vertex
    std::unordered_map<IndexTriplet, uint32_t, IndexTripletHash> indexMap;
    std::vector<GPUMeshVertex> outVertices;
    std::vector<uint32_t> outIndices;
    outVertices.reserve(srcPos.size());
    outIndices.reserve(srcIdx.size());

    for ( size_t k = 0; k < srcIdx.size(); ++k )
    {
      const Vec3i & triIdx = srcIdx[k];

      IndexTriplet key{ triIdx.x, triIdx.y, triIdx.z };
      auto it = indexMap.find(key);
      if ( it != indexMap.end() )
      {
        outIndices.push_back(static_cast<uint32_t>(it -> second));
      }
      else
      {
        GPUMeshVertex v{};
        // position
        if ( ( key.v >= 0 ) && ( static_cast<size_t>(key.v) < srcPos.size() ) )
          v._Pos = srcPos[key.v];
        else
          v._Pos = Vec3(0.f);

        // normal (fallback to zero if missing)
        if ( ( key.n >= 0 ) && ( static_cast<size_t>(key.n) < srcNorm.size() ) )
          v._Normal = srcNorm[key.n];
        else
          v._Normal = Vec3(0.f);

        // uv
        if ( ( key.u >= 0 ) && ( static_cast<size_t>(key.u) < srcUV.size() ) )
          v._UV = srcUV[key.u];
        else
          v._UV = Vec2(0.f);

        uint32_t newIndex = static_cast<uint32_t>(outVertices.size());
        outVertices.push_back(v);
        indexMap.emplace(key, newIndex);
        outIndices.push_back(newIndex);
      }
    }

    if ( outIndices.empty() || outVertices.empty() )
      continue;

    // Create GPU buffers and VAO
    GLuint vao = 0, vbo = 0, ebo = 0;

    // Attributes: location 0 = position (vec3), 1 = normal (vec3), 2 = uv (vec2)
    const GLsizei stride = static_cast<GLsizei>(sizeof(GPUMeshVertex));
    std::vector<std::tuple<GLuint, GLint, GLenum, GLboolean, GLsizei, std::size_t>> attrs;
    attrs.emplace_back(0u, 3, GL_FLOAT, GL_FALSE, stride, offsetof(GPUMeshVertex, _Pos));
    attrs.emplace_back(1u, 3, GL_FLOAT, GL_FALSE, stride, offsetof(GPUMeshVertex, _Normal));
    attrs.emplace_back(2u, 2, GL_FLOAT, GL_FALSE, stride, offsetof(GPUMeshVertex, _UV));

    GLUtil::CreateMeshBuffers( static_cast<GLsizeiptr>(outVertices.size() * sizeof(GPUMeshVertex)), outVertices.data(),
                               static_cast<GLsizeiptr>(outIndices.size() * sizeof(uint32_t)), outIndices.data(),
                               attrs,
                               vao, vbo, ebo);

    _MeshVAOs[mi] = vao;
    _MeshVBOs[mi] = vbo;
    _MeshEBOs[mi] = ebo;
    _MeshIndexCount[mi] = static_cast<int>(outIndices.size());
  }

  // Materials
  if ( _Scene.GetTextureArrayIDs().size() )
  {
    GLUtil::InitializeTBO(_TexIndTBO, sizeof(int) * _Scene.GetTextureArrayIDs().size(), &_Scene.GetTextureArrayIDs()[0], GL_R32I);

    glGenTextures(1, &_TexArrayTEX._Handle);
    glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTEX._Handle);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, _Settings._TextureSize.x, _Settings._TextureSize.y, _Scene.GetNbCompiledTex(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_Scene.GetTextureArray()[0]);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if ( _GenerateMipMaps )
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    else
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    if ( _GenerateMipMaps && _AnisotropicLevel )
      GLUtil::EnableAnisotropyIfAvailable(_TexArrayTEX, (float)_AnisotropicLevel);
  }

  glGenTextures(1, &_MaterialsTEX._Handle);
  glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>((sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size()), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadEnvMap
// ----------------------------------------------------------------------------
void DeferredRenderer::SetGenerateMipMaps(bool iGenerate)
{
  _GenerateMipMaps = iGenerate;
  _DirtyStates |= (unsigned long)DirtyState::Textures;
}

// ----------------------------------------------------------------------------
// ReloadEnvMap
// ----------------------------------------------------------------------------
void DeferredRenderer::SetAnisotropicLevel(int iLevel)
{
  _AnisotropicLevel = iLevel;
  _DirtyStates |= (unsigned long)DirtyState::Textures;
}

// ----------------------------------------------------------------------------
// ReloadEnvMap
// ----------------------------------------------------------------------------
int DeferredRenderer::ReloadEnvMap()
{
  GLUtil::DeleteTEX(_EnvMapTEX);

  if ( _Scene.GetEnvMap().IsInitialized() )
  {
    glGenTextures(1, &_EnvMapTEX._Handle);
    glBindTexture(GL_TEXTURE_2D, _EnvMapTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, _Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight(), 0, GL_RGB, GL_FLOAT, _Scene.GetEnvMap().GetRawData());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    _Scene.GetEnvMap().SetHandle(_EnvMapTEX._Handle);
  }
  else
    _Settings._EnableSkybox = false;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeFrameBuffers()
{
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * RenderScale());
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * RenderScale());

  // Albedo (8-bit RGBA)
  GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA8, RenderWidth(), RenderHeight(), GL_RGBA, GL_UNSIGNED_BYTE, nullptr, _GAlbedoTEX);

  // Normals (high precision)
  GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA16F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _GNormalTEX);

  // World positions (high precision)
  GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA16F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _GPositionTEX);

  // Depth
  GLUtil::GenTexture(GL_TEXTURE_2D, GL_DEPTH_COMPONENT24, RenderWidth(), RenderHeight(), GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr, _GDepthTEX, GL_NEAREST, GL_NEAREST);

  // Create / configure G-buffer FBO
  glGenFramebuffers(1, &_GBufferFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _GBufferFBO._Handle);

  _GBufferFBO._Tex.clear();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _GAlbedoTEX._Handle, 0);
  _GBufferFBO._Tex.push_back(_GAlbedoTEX);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _GNormalTEX._Handle, 0);
  _GBufferFBO._Tex.push_back(_GNormalTEX);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _GPositionTEX._Handle, 0);
  _GBufferFBO._Tex.push_back(_GPositionTEX);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_TEXTURE_2D, _GDepthTEX._Handle, 0);
  _GBufferFBO._Tex.push_back(_GDepthTEX);

  GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
  glDrawBuffers(3, DrawBuffers);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "DeferredRenderer : G-buffer framebuffer not complete !" << std::endl;
    return 1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Lighting target (single HDR target)
  GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _LightingTEX);

  glGenFramebuffers(1, &_LightingFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _LightingFBO._Handle);

  _LightingFBO._Tex.clear();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _LightingTEX._Handle, 0);
  _LightingFBO._Tex.push_back(_LightingTEX);

  GLenum LightDrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(1, LightDrawBuffers);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "DeferredRenderer : Lighting framebuffer not complete !" << std::endl;
    return 1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeRenderTarget
// ----------------------------------------------------------------------------
int DeferredRenderer::ResizeRenderTarget()
{
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * RenderScale());
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * RenderScale());

  GLUtil::ResizeFBO(_GBufferFBO, RenderWidth(), RenderHeight());
  GLUtil::ResizeFBO(_LightingFBO, RenderWidth(), RenderHeight());

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int DeferredRenderer::RecompileShaders()
{
  // Geometry pass shader (standard vertex + fragment that writes G-buffer)
  ShaderSource geomVert = Shader::LoadShader(PathUtils::GetShaderPath("vertex_DeferredGeometry.glsl"));
  ShaderSource geomFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_DeferredGeometry.glsl"));
  ShaderProgram* geomProg = ShaderProgram::LoadShaders(geomVert, geomFrag);
  if (!geomProg)
    return 1;
  _GeometryShader.reset(geomProg);

  // Lighting/composite pass: fullscreen quad sampling G-buffer
  ShaderSource defaultVert = Shader::LoadShader(PathUtils::GetShaderPath("vertex_Default.glsl"));
  ShaderSource lightFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_DeferredLighting.glsl"));
  ShaderProgram* lightProg = ShaderProgram::LoadShaders(defaultVert, lightFrag);
  if (!lightProg)
    return 1;
  _LightingShader.reset(lightProg);

  // Optional post-process/composite (reuse existing postprocess if desired)
  ShaderSource postFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_Postprocess.glsl"));
  ShaderProgram* postProg = ShaderProgram::LoadShaders(defaultVert, postFrag);
  if (!postProg)
    return 1;
  _CompositeShader.reset(postProg);

  // DEBUG : Wireframe shader
  ShaderSource wireFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_Wireframe.glsl"));
  ShaderProgram* wireProg = ShaderProgram::LoadShaders(geomVert, wireFrag); // reuse geometry vertex shader
  if (!wireProg)
    return 1;
  _WireframeShader.reset(wireProg);
 
  return 0;
}

// ----------------------------------------------------------------------------
// BindGBufferTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindGBufferTextures()
{
  GLUtil::ActivateTextures(_GBufferFBO); 

  return 0;
}

// ----------------------------------------------------------------------------
// BindLightingTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindLightingTextures()
{
  GLUtil::ActivateTextures(_GBufferFBO); 

  GLUtil::ActivateTexture(_TexIndTBO._Tex);
  GLUtil::ActivateTexture(_TexArrayTEX);
  GLUtil::ActivateTexture(_MaterialsTEX);

  GLUtil::ActivateTexture(_EnvMapTEX);

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindRenderToScreenTextures()
{
  GLUtil::ActivateTextures(_LightingFBO);
  GLUtil::ActivateTexture(_EnvMapTEX);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateUniforms
// ----------------------------------------------------------------------------
int DeferredRenderer::UpdateUniforms()
{
  // Build camera matrices and common camera params
  Mat4x4 V;
  _Scene.GetCamera().ComputeLookAtMatrix(V);

  float ratio = RenderWidth() / float(RenderHeight());
  float top, right;
  Mat4x4 P;
  _Scene.GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  float zNear = 0.0f, zFar = 0.0f;
  _Scene.GetCamera().GetZNearFar(zNear, zFar);

  Vec3 camPos = _Scene.GetCamera().GetPos();
  Vec3 camUp = _Scene.GetCamera().GetUp();
  Vec3 camRight = _Scene.GetCamera().GetRight();
  Vec3 camForward = _Scene.GetCamera().GetForward();
  float camFov = _Scene.GetCamera().GetFOV();

  // Update Scene data
  if ( _DirtyStates & (unsigned long)DirtyState::SceneMaterials )
  {
    glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>((sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size()), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  // Geometry shader
  if ( _GeometryShader )
  {
    _GeometryShader -> Use();
    _GeometryShader -> SetUniform("u_CameraPos", camPos);
    _GeometryShader -> SetUniform("u_View", V);
    _GeometryShader -> SetUniform("u_Proj", P);

    // Scene data
    _GeometryShader -> SetUniform("u_TexIndTexture",    (int)DeferredTexSlot::_TexInd);
    _GeometryShader -> SetUniform("u_TexArrayTexture",  (int)DeferredTexSlot::_TexArray);
    _GeometryShader -> SetUniform("u_MaterialsTexture", (int)DeferredTexSlot::_Materials);

    _GeometryShader -> StopUsing();
  }

  // Lighting shader
  if ( _LightingShader )
  {
    _LightingShader -> Use();
    _LightingShader -> SetUniform("u_Camera._Pos", camPos);
    _LightingShader -> SetUniform("u_Camera._Up", camUp);
    _LightingShader -> SetUniform("u_Camera._Right", camRight);
    _LightingShader -> SetUniform("u_Camera._Forward", camForward);
    _LightingShader -> SetUniform("u_Camera._FOV", camFov);

    // The lighting shader expects samplers named u_GAlbedo, u_GNormal, u_GPosition, u_GDepth
    _LightingShader -> SetUniform("u_GAlbedo",   (int)DeferredTexSlot::_GAlbedo);
    _LightingShader -> SetUniform("u_GNormal",   (int)DeferredTexSlot::_GNormal);
    _LightingShader -> SetUniform("u_GPosition", (int)DeferredTexSlot::_GPosition);
    _LightingShader -> SetUniform("u_GDepth",    (int)DeferredTexSlot::_GDepth);
    _LightingShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));

    if ( _DirtyStates & (unsigned long)DirtyState::SceneLights )
    {
      int nbLights = 0;

      for ( int i = 0; i < _Scene.GetNbLights(); ++i )
      {
        Light * curLight = _Scene.GetLight(i);
        if ( !curLight )
          continue;

        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Pos"     ), curLight -> _Pos);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Emission"), curLight -> _Emission * curLight -> _Intensity);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_DirU"    ), curLight -> _DirU);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_DirV"    ), curLight -> _DirV);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Radius"  ), curLight -> _Radius);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Area"    ), curLight -> _Area);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Type"    ), curLight -> _Type);

        nbLights++;
        if ( nbLights >= 32 )
          break;
      }

      _LightingShader -> SetUniform("u_NbLights", nbLights);
      _LightingShader -> SetUniform("u_ShowLights", (int)_Settings._ShowLights);
    }

    // Scene data
    _LightingShader -> SetUniform("u_BackgroundColor", _Settings._BackgroundColor);
    _LightingShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
    _LightingShader -> SetUniform("u_EnableBackground" , (int)_Settings._EnableBackGround);
    _LightingShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
    _LightingShader -> SetUniform("u_EnvMapRes", (float)_Scene.GetEnvMap().GetWidth(), (float)_Scene.GetEnvMap().GetHeight());
    _LightingShader -> SetUniform("u_EnvMap", (int)DeferredTexSlot::_EnvMap);

    // Debug
    _LightingShader -> SetUniform("u_DebugMode" , _DebugMode);

    _LightingShader -> StopUsing();
  }

  // DEBUG : Wireframe shader
  if ( _WireframeShader )
  {
    _WireframeShader -> Use();
    _WireframeShader -> SetUniform("u_CameraPos", camPos);
    _WireframeShader -> SetUniform("u_View", V);
    _WireframeShader -> SetUniform("u_Proj", P);
    _WireframeShader -> SetUniform("u_WireColor", S_WireColor);
    _WireframeShader -> StopUsing();
  }

  // Composite shader
  if ( _CompositeShader )
  {
    _CompositeShader -> Use();

    _CompositeShader -> SetUniform("u_ScreenTexture", (int)DeferredTexSlot::_Lighting);
    _CompositeShader -> SetUniform("u_RenderRes", static_cast<float>(_Settings._RenderResolution.x), static_cast<float>(_Settings._RenderResolution.y));
    _CompositeShader -> SetUniform("u_Gamma", _Settings._Gamma);
    _CompositeShader -> SetUniform("u_Exposure", _Settings._Exposure);
    _CompositeShader -> SetUniform("u_ToneMapping", ( _Settings._ToneMapping ? 1 : 0 ));
    _CompositeShader -> SetUniform("u_FXAA", (_Settings._FXAA ?  1 : 0 ));

    _CompositeShader -> StopUsing();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderToTexture()
{
  if (_GeometryShader)
  {
    // Geometry pass: render scene into G-buffer
    glBindFramebuffer(GL_FRAMEBUFFER, _GBufferFBO._Handle);
    glViewport(0, 0, RenderWidth(), RenderHeight());

    // Ensure a well-defined GL state: enable depth writes before clearing so the depth
    // attachment is actually cleared each frame (previous code could leave depth mask false).
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // now actually clears depth

    _GeometryShader -> Use();

    this -> BindGBufferTextures();

    const auto & instances = _Scene.GetMeshInstances();
    for ( const MeshInstance& inst : instances )
    {
      int meshID = inst._MeshID;
      if ( ( meshID < 0 ) || ( static_cast<size_t>(meshID) >= _MeshVAOs.size() ) )
        continue;

      // Per-instance uniforms expected by geometry shader
      _GeometryShader -> SetUniform("u_Model", inst._Transform);
      _GeometryShader -> SetUniform("u_MaterialID", inst._MaterialID);

      GLuint vao = _MeshVAOs[meshID];
      int idxCount = _MeshIndexCount[meshID];
      if ( vao && ( idxCount > 0 ) )
      {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
      }
    }

    glBindVertexArray(0);
    _GeometryShader -> StopUsing();

    // Disable depth writes and depth test for subsequent fullscreen passes.
    glDepthMask(GL_FALSE);   // stop writing depth for fullscreen passes
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  if (_LightingShader)
  {
    // Lighting pass: sample G-buffer and compute shading into lighting FBO
    glBindFramebuffer(GL_FRAMEBUFFER, _LightingFBO._Handle);
    glViewport(0, 0, RenderWidth(), RenderHeight());

    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Bind G-buffer textures to shader
    _LightingShader -> Use();

    this -> BindLightingTextures();

    // Render fullscreen quad to produce lit image
    _Quad.Render(*_LightingShader);
    _LightingShader -> StopUsing();

    // At this point _LightingTEX contains the shaded image
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  // DEBUG : Wireframe overlay. Render lines into lighting target on top of shaded image
  if ( ( _DebugMode & (int)DeferredDebugModes::Wires ) && _WireframeShader )
  { 
    glBindFramebuffer(GL_FRAMEBUFFER, _LightingFBO._Handle);
    glViewport(0, 0, RenderWidth(), RenderHeight());

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _GDepthTEX._Handle, 0);

    // Use depth test so lines occlude correctly, but do not write depth
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    // Render only lines
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(S_WireWidth);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    _WireframeShader -> Use();

    this -> BindLightingTextures();

    const auto & instances2 = _Scene.GetMeshInstances();
    for ( const MeshInstance& inst : instances2 )
    {
      int meshID = inst._MeshID;
      if ( ( meshID < 0 ) || ( static_cast<size_t>(meshID) >= _MeshVAOs.size() ) )
        continue;

      _WireframeShader -> SetUniform("u_Model", inst._Transform);

      GLuint vao = _MeshVAOs[meshID];
      int idxCount = _MeshIndexCount[meshID];
      if ( vao && ( idxCount > 0 ) )
      {
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
      }
    }

    _WireframeShader -> StopUsing();

    // Restore state
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToScreen
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderToScreen()
{
  // Composite / postprocess and render to default framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  if ( _CompositeShader )
  {
    _CompositeShader -> Use();

    this -> BindRenderToScreenTextures();

    _Quad.Render(*_CompositeShader);

    _CompositeShader -> StopUsing();
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderToFile(const std::filesystem::path& iFilePath)
{
  // Render current lighting texture to an intermediary FBO bound to a temporary texture,
  // then read back pixels similar to PathTracer::RenderToFile
  GLFrameBuffer temporaryFBO;
  temporaryFBO._Tex.push_back({0, GL_TEXTURE_2D, 5});

  int w = _Settings._WindowResolution.x;
  int h = _Settings._WindowResolution.y;

  // Create temp texture and FBO
  glGenTextures(1, &temporaryFBO._Tex[0]._Handle);
  glActiveTexture(GL_TEX_UNIT(temporaryFBO._Tex[0]));
  glBindTexture(GL_TEXTURE_2D, temporaryFBO._Tex[0]._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &temporaryFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, temporaryFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temporaryFBO._Tex[0]._Handle, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    GLUtil::DeleteTEX(temporaryFBO._Tex[0]);
    return 1;
  }

  // Render lighting texture into temporary FBO
  glBindFramebuffer(GL_FRAMEBUFFER, temporaryFBO._Handle);
  glViewport(0, 0, w, h);

  if (_CompositeShader)
  {
    _CompositeShader -> Use();

    BindRenderToScreenTextures();

    _Quad.Render(*_CompositeShader);

    _CompositeShader -> StopUsing();
  }

  // Readback and save
  unsigned char* frameData = new unsigned char[w * h * 4];
  glBindTexture(GL_TEXTURE_2D, temporaryFBO._Tex[0]._Handle);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData);

  stbi_flip_vertically_on_write(true);
  int saved = stbi_write_png(iFilePath.string().c_str(), w, h, 4, frameData, w * 4);
  delete[] frameData;

  GLUtil::DeleteFBO(temporaryFBO);

  if ( saved && std::filesystem::exists(iFilePath) )
    std::cout << "Frame saved in " << std::filesystem::absolute(iFilePath) << std::endl;
  else
    std::cout << "ERROR : Failed to save screen capture in " << std::filesystem::absolute(iFilePath) << std::endl;

  return 0;
}

} // namespace RTRT
