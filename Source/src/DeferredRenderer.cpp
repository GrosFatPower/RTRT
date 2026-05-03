#include "DeferredRenderer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "PathUtils.h"
#include "Mesh.h"
#include "Material.h"
#include "MathUtil.h"
#include "Light.h"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <algorithm>
#include <numeric>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <random>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "stb_image_write.h"

namespace RTRT
{

static Vec3 S_WireColor = Vec3(1.f, 0.f, 0.f);
static float S_WireWidth = 3.0f;
static const float S_TransparentSpecTransThreshold = 0.001f;

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
  GLUtil::DeleteFBO(_GBufferFBO);
  GLUtil::DeleteFBO(_LightingFBO);
  GLUtil::DeleteFBO(_BRDFFBO);
  GLUtil::DeleteFBO(_ShadowFBO);
  GLUtil::DeleteFBO(_SSAOFBO);
  GLUtil::DeleteFBO(_SSAOBlurFBO);

  GLUtil::DeleteTEX(_GAlbedoTEX);
  GLUtil::DeleteTEX(_GNormalTEX);
  GLUtil::DeleteTEX(_GPositionTEX);
  GLUtil::DeleteTEX(_GMaterialTEX);
  GLUtil::DeleteTEX(_GDepthTEX);
  GLUtil::DeleteTEX(_SSAOTEX);
  GLUtil::DeleteTEX(_SSAOBlurTEX);
  GLUtil::DeleteTEX(_SSAONoiseTEX);
  GLUtil::DeleteTEX(_ShadowCubeMapTEX);
  GLUtil::DeleteTEX(_Shadow2DMapTEX);
  GLUtil::DeleteTEX(_BRDFLUTTEX);

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

  if ( 0 != InitializeBRDFLUT() )
  {
    std::cout << "DeferredRenderer : Failed to initialize BRDF LUT !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeSSAO() )
  {
    std::cout << "DeferredRenderer : Failed to initialize SSAO !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeShadowMap() )
  {
    std::cout << "DeferredRenderer : Failed to initialize shadow map !" << std::endl;
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
  {
    this -> ResizeRenderTarget();

    if ( _ShadowMapSize != _Settings._ShadowMapResolution )
      this -> InitializeShadowMap();
  }

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

  if ( _DirtyStates & ( (unsigned long)DirtyState::SceneMaterials | (unsigned long)DirtyState::SceneInstances ) )
    BuildDeferredDrawLists();

  UpdateShadowState();
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

  const size_t nb = _MeshVAOs.size();
  for ( size_t i = 0; i < nb; ++i )
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
  _TransparentMeshBaseIndices.clear();
  _TransparentMeshLocalTriCenters.clear();
  _TransparentMeshSortedIndices.clear();
  _TransparentMeshSortedTriOrder.clear();
  _TransparentMeshTriDepths.clear();
  _OpaqueMeshInstanceIDs.clear();
  _TransparentMeshInstanceIDs.clear();

  _ShadowLightIndex = -1;
  _ShadowLightType = LightType::SphereLight;
  _HasShadowLight = false;

  return 0;
}

// ----------------------------------------------------------------------------
// ComputeSceneBounds
// ----------------------------------------------------------------------------
void DeferredRenderer::ComputeSceneBounds()
{
  bool initialized = false;
  Vec3 low(-10.f), high(10.f);

  const auto & instances = _Scene.GetMeshInstances();
  const auto & meshes = _Scene.GetMeshes();
  for ( const MeshInstance & inst : instances )
  {
    if ( ( inst._MeshID < 0 ) || ( inst._MeshID >= (int)meshes.size() ) )
      continue;

    Mesh * mesh = meshes[inst._MeshID];
    if ( !mesh )
      continue;

    const AABB<Vec3> & bbox = mesh -> GetBoundingBox();
    Vec3 corners[8];
    bbox.Corners(corners);

    for ( const Vec3 & corner : corners )
    {
      Vec3 worldCorner = MathUtil::TransformPoint(corner, inst._Transform);
      if ( !initialized )
      {
        low = high = worldCorner;
        initialized = true;
      }
      else
      {
        MathUtil::Minimize(low, worldCorner);
        MathUtil::Maximize(high, worldCorner);
      }
    }
  }

  _SceneBounds._Low = low;
  _SceneBounds._High = high;
  _SceneBoundsRadius = std::max( glm::length( high - low ) * 0.5f, 1.f );
}

// ----------------------------------------------------------------------------
// ComputeAutoShadowFar
// ----------------------------------------------------------------------------
float DeferredRenderer::ComputeAutoShadowFar( const Vec3 & iLightPos ) const
{
  Vec3 corners[8];
  _SceneBounds.Corners(corners);

  float maxDistance = 1.f;
  for ( const Vec3 & corner : corners )
    maxDistance = std::max( maxDistance, glm::length( corner - iLightPos ) );

  return maxDistance * 1.05f;
}

// ----------------------------------------------------------------------------
// IsTransparentMaterial
// ----------------------------------------------------------------------------
bool DeferredRenderer::IsTransparentMaterial( int iMaterialID )
{
  const std::vector<Material> & materials = _Scene.GetMaterials();
  if ( ( iMaterialID < 0 ) || ( static_cast<size_t>(iMaterialID) >= materials.size() ) )
    return false;

  const Material & mat = materials[iMaterialID];
  AlphaMode alphaMode = MaterialAlphaMode(mat);

  if ( AlphaMode::Blend == alphaMode )
    return true;

  if ( mat._SpecTrans > S_TransparentSpecTransThreshold )
    return true;

  return false;
}

// ----------------------------------------------------------------------------
// BuildDeferredDrawLists
// ----------------------------------------------------------------------------
void DeferredRenderer::BuildDeferredDrawLists()
{
  _OpaqueMeshInstanceIDs.clear();
  _TransparentMeshInstanceIDs.clear();

  const std::vector<MeshInstance> & instances = _Scene.GetMeshInstances();
  _OpaqueMeshInstanceIDs.reserve(instances.size());
  _TransparentMeshInstanceIDs.reserve(instances.size());

  for ( int i = 0; i < static_cast<int>(instances.size()); ++i )
  {
    const MeshInstance & inst = instances[i];
    if ( IsTransparentMaterial( inst._MaterialID ) )
      _TransparentMeshInstanceIDs.push_back(i);
    else
      _OpaqueMeshInstanceIDs.push_back(i);
  }
}

// ----------------------------------------------------------------------------
// SortTransparentInstances
// ----------------------------------------------------------------------------
void DeferredRenderer::SortTransparentInstances()
{
  if ( _TransparentMeshInstanceIDs.size() < 2 )
    return;

  Mat4x4 view;
  _Scene.GetCamera().ComputeLookAtMatrix(view);

  const std::vector<MeshInstance> & instances = _Scene.GetMeshInstances();
  const std::vector<Mesh*> & meshes = _Scene.GetMeshes();

  std::sort( _TransparentMeshInstanceIDs.begin(), _TransparentMeshInstanceIDs.end(),
    [&]( int iLhsIdx, int iRhsIdx )
    {
      if ( ( iLhsIdx < 0 ) || ( static_cast<size_t>(iLhsIdx) >= instances.size() )
        || ( iRhsIdx < 0 ) || ( static_cast<size_t>(iRhsIdx) >= instances.size() ) )
        return iLhsIdx > iRhsIdx;

      const MeshInstance & lhsInst = instances[iLhsIdx];
      const MeshInstance & rhsInst = instances[iRhsIdx];

      Vec3 lhsWorldCenter(0.f);
      Vec3 rhsWorldCenter(0.f);

      if ( ( lhsInst._MeshID >= 0 ) && ( static_cast<size_t>(lhsInst._MeshID) < meshes.size() ) && meshes[lhsInst._MeshID] )
        lhsWorldCenter = MathUtil::TransformPoint( meshes[lhsInst._MeshID] -> GetBoundingBox().Center(), lhsInst._Transform );

      if ( ( rhsInst._MeshID >= 0 ) && ( static_cast<size_t>(rhsInst._MeshID) < meshes.size() ) && meshes[rhsInst._MeshID] )
        rhsWorldCenter = MathUtil::TransformPoint( meshes[rhsInst._MeshID] -> GetBoundingBox().Center(), rhsInst._Transform );

      float lhsViewZ = ( view * Vec4(lhsWorldCenter, 1.f) ).z;
      float rhsViewZ = ( view * Vec4(rhsWorldCenter, 1.f) ).z;

      return lhsViewZ < rhsViewZ;
    } );
}

// ----------------------------------------------------------------------------
// BuildTransparentMeshTriangleData
// ----------------------------------------------------------------------------
void DeferredRenderer::BuildTransparentMeshTriangleData( size_t iMeshID, const std::vector<Vec3> & iPositions, const std::vector<uint32_t> & iIndices )
{
  if ( iMeshID >= _TransparentMeshBaseIndices.size() )
    return;

  std::vector<uint32_t> & baseIndices = _TransparentMeshBaseIndices[iMeshID];
  std::vector<Vec3> & localCenters = _TransparentMeshLocalTriCenters[iMeshID];
  std::vector<uint32_t> & sortedIndices = _TransparentMeshSortedIndices[iMeshID];
  std::vector<int> & sortedTriOrder = _TransparentMeshSortedTriOrder[iMeshID];
  std::vector<float> & triDepths = _TransparentMeshTriDepths[iMeshID];

  baseIndices.clear();
  localCenters.clear();
  sortedIndices.clear();
  sortedTriOrder.clear();
  triDepths.clear();

  if ( iIndices.size() < 3 )
    return;

  const size_t triCount = iIndices.size() / 3;
  baseIndices.reserve(triCount * 3);
  localCenters.reserve(triCount);

  for ( size_t ti = 0; ti < triCount; ++ti )
  {
    size_t base = ti * 3;
    uint32_t i0 = iIndices[base + 0];
    uint32_t i1 = iIndices[base + 1];
    uint32_t i2 = iIndices[base + 2];

    if ( ( static_cast<size_t>(i0) >= iPositions.size() )
      || ( static_cast<size_t>(i1) >= iPositions.size() )
      || ( static_cast<size_t>(i2) >= iPositions.size() ) )
      continue;

    baseIndices.push_back(i0);
    baseIndices.push_back(i1);
    baseIndices.push_back(i2);

    Vec3 center = ( iPositions[i0] + iPositions[i1] + iPositions[i2] ) * (1.f / 3.f);
    localCenters.push_back(center);
  }

  sortedIndices = baseIndices;
  sortedTriOrder.resize(localCenters.size(), 0);
  triDepths.resize(localCenters.size(), 0.f);
  std::iota(sortedTriOrder.begin(), sortedTriOrder.end(), 0);
}

// ----------------------------------------------------------------------------
// UpdateSortedTransparentMeshIndices
// ----------------------------------------------------------------------------
bool DeferredRenderer::UpdateSortedTransparentMeshIndices( int iMeshID, const Mat4x4 & iModel, const Mat4x4 & iView )
{
  if ( ( iMeshID < 0 ) || ( static_cast<size_t>(iMeshID) >= _MeshEBOs.size() ) )
    return false;
  if ( static_cast<size_t>(iMeshID) >= _MeshVAOs.size() )
    return false;
  if ( ( static_cast<size_t>(iMeshID) >= _TransparentMeshBaseIndices.size() )
    || ( static_cast<size_t>(iMeshID) >= _TransparentMeshLocalTriCenters.size() )
    || ( static_cast<size_t>(iMeshID) >= _TransparentMeshSortedIndices.size() )
    || ( static_cast<size_t>(iMeshID) >= _TransparentMeshSortedTriOrder.size() )
    || ( static_cast<size_t>(iMeshID) >= _TransparentMeshTriDepths.size() ) )
    return false;

  const std::vector<uint32_t> & baseIndices = _TransparentMeshBaseIndices[iMeshID];
  const std::vector<Vec3> & localCenters = _TransparentMeshLocalTriCenters[iMeshID];
  std::vector<uint32_t> & sortedIndices = _TransparentMeshSortedIndices[iMeshID];
  std::vector<int> & sortedTriOrder = _TransparentMeshSortedTriOrder[iMeshID];
  std::vector<float> & triDepths = _TransparentMeshTriDepths[iMeshID];

  if ( localCenters.empty() || baseIndices.empty() )
    return false;

  if ( ( baseIndices.size() % 3 ) != 0 )
    return false;

  const size_t triCount = localCenters.size();
  if ( triCount != ( baseIndices.size() / 3 ) )
    return false;

  if ( sortedTriOrder.size() != triCount )
  {
    sortedTriOrder.resize(triCount, 0);
    std::iota(sortedTriOrder.begin(), sortedTriOrder.end(), 0);
  }
  if ( triDepths.size() != triCount )
    triDepths.resize(triCount, 0.f);
  if ( sortedIndices.size() != baseIndices.size() )
    sortedIndices.resize(baseIndices.size(), 0u);

  for ( size_t ti = 0; ti < triCount; ++ti )
  {
    Vec3 worldCenter = MathUtil::TransformPoint(localCenters[ti], iModel);
    triDepths[ti] = ( iView * Vec4(worldCenter, 1.f) ).z;
  }

  std::sort( sortedTriOrder.begin(), sortedTriOrder.end(),
    [&]( int iLhs, int iRhs )
    {
      return triDepths[iLhs] < triDepths[iRhs];
    } );

  for ( size_t sortedIdx = 0; sortedIdx < triCount; ++sortedIdx )
  {
    size_t srcTri = static_cast<size_t>(sortedTriOrder[sortedIdx]);
    size_t srcBase = srcTri * 3;
    size_t dstBase = sortedIdx * 3;
    sortedIndices[dstBase + 0] = baseIndices[srcBase + 0];
    sortedIndices[dstBase + 1] = baseIndices[srcBase + 1];
    sortedIndices[dstBase + 2] = baseIndices[srcBase + 2];
  }

  return true;
}

// ----------------------------------------------------------------------------
// UpdateShadowState
// ----------------------------------------------------------------------------
int DeferredRenderer::UpdateShadowState()
{
  _ShadowLightIndex = -1;
  _ShadowLightType = LightType::SphereLight;
  _HasShadowLight = false;

  if ( !_Settings._ShadowMapping )
    return 0;

  int distantLightIndex = -1;
  Vec3 distantLightDir(0.f, 1.f, 0.f);
  int sphereLightIndex = -1;
  Vec3 sphereLightPos(0.f);
  int rectLightIndex = -1;
  Vec3 rectLightPos(0.f);

  for ( int i = 0; i < _Scene.GetNbLights(); ++i )
  {
    Light * curLight = _Scene.GetLight(i);
    if ( !curLight )
      continue;

    LightType lightType = (LightType) curLight -> _Type;
    if ( ( distantLightIndex < 0 ) && ( LightType::DistantLight == lightType ) )
    {
      distantLightIndex = i;
      if ( glm::length(curLight -> _Pos) > 0.f )
        distantLightDir = glm::normalize(curLight -> _Pos);
      continue;
    }
    if ( ( sphereLightIndex < 0 ) && ( LightType::SphereLight == lightType ) )
    {
      sphereLightIndex = i;
      sphereLightPos = curLight -> _Pos;
      continue;
    }
    if ( ( rectLightIndex < 0 ) && ( LightType::RectLight == lightType ) )
    {
      rectLightIndex = i;
      rectLightPos = curLight -> _Pos;
    }
  }

  if ( distantLightIndex >= 0 )
  {
    _ShadowLightIndex = distantLightIndex;
    _ShadowLightType = LightType::DistantLight;
    _ShadowLightDir = distantLightDir;
    _HasShadowLight = true;

    float lightDistance = std::max( _SceneBoundsRadius * 2.f, 10.f );
    Vec3 lightPos = _SceneBounds.Center() + _ShadowLightDir * lightDistance;
    Vec3 up = ( std::abs(glm::dot(_ShadowLightDir, Vec3(0.f, 1.f, 0.f))) > 0.99f ) ? ( Vec3(0.f, 0.f, 1.f) ) : ( Vec3(0.f, 1.f, 0.f) );
    Mat4x4 lightView = glm::lookAt(lightPos, _SceneBounds.Center(), up);

    Vec3 corners[8];
    _SceneBounds.Corners(corners);

    Vec3 lightSpaceLow( std::numeric_limits<float>::infinity() );
    Vec3 lightSpaceHigh( -std::numeric_limits<float>::infinity() );
    for ( const Vec3 & corner : corners )
    {
      Vec4 lightSpaceCorner = lightView * Vec4(corner, 1.f);
      MathUtil::Minimize(lightSpaceLow, Vec3(lightSpaceCorner));
      MathUtil::Maximize(lightSpaceHigh, Vec3(lightSpaceCorner));
    }

    float padXY = std::max( _SceneBoundsRadius * 0.1f, 1.f );
    float nearPlane = 0.1f;
    float farPlane = lightDistance + _SceneBoundsRadius * 4.f;
    Mat4x4 shadowProj = glm::ortho(lightSpaceLow.x - padXY, lightSpaceHigh.x + padXY,
                                   lightSpaceLow.y - padXY, lightSpaceHigh.y + padXY,
                                   nearPlane, farPlane);
    _ShadowDirectionalViewProj = shadowProj * lightView;

    return 0;
  }

  if ( sphereLightIndex >= 0 )
  {
    _ShadowLightIndex = sphereLightIndex;
    _ShadowLightType = LightType::SphereLight;
    _ShadowLightPos = sphereLightPos;
    _HasShadowLight = true;
  }
  else if ( rectLightIndex >= 0 )
  {
    _ShadowLightIndex = rectLightIndex;
    _ShadowLightType = LightType::RectLight;
    _ShadowLightPos = rectLightPos;
    _HasShadowLight = true;
  }

  if ( !_HasShadowLight )
    return 0;

  _ShadowFar = ( _Settings._ShadowFar > 0.f ) ? ( _Settings._ShadowFar ) : ( ComputeAutoShadowFar( _ShadowLightPos ) );

  Mat4x4 shadowProj = glm::perspective(MathUtil::ToRadians(90.f), 1.0f, _ShadowNear, _ShadowFar);

  const Vec3 lookDirs[6] = {
    Vec3( 1.f,  0.f,  0.f),
    Vec3(-1.f,  0.f,  0.f),
    Vec3( 0.f,  1.f,  0.f),
    Vec3( 0.f, -1.f,  0.f),
    Vec3( 0.f,  0.f,  1.f),
    Vec3( 0.f,  0.f, -1.f)
  };
  const Vec3 upDirs[6] = {
    Vec3(0.f, -1.f,  0.f),
    Vec3(0.f, -1.f,  0.f),
    Vec3(0.f,  0.f,  1.f),
    Vec3(0.f,  0.f, -1.f),
    Vec3(0.f, -1.f,  0.f),
    Vec3(0.f, -1.f,  0.f)
  };

  for ( int i = 0; i < 6; ++i )
  {
    Mat4x4 view = glm::lookAt(_ShadowLightPos, _ShadowLightPos + lookDirs[i], upDirs[i]);
    _ShadowViewProj[i] = shadowProj * view;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeShadowMap
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeShadowMap()
{
  GLUtil::DeleteFBO(_ShadowFBO);
  GLUtil::DeleteTEX(_ShadowCubeMapTEX);
  GLUtil::DeleteTEX(_Shadow2DMapTEX);

  int shadowMapSize = std::clamp(_Settings._ShadowMapResolution, 256, 4096);
  _ShadowMapSize = shadowMapSize;

  glGenTextures(1, &_ShadowCubeMapTEX._Handle);
  glBindTexture(GL_TEXTURE_CUBE_MAP, _ShadowCubeMapTEX._Handle);
  for ( int i = 0; i < 6; ++i )
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, _ShadowCubeMapTEX._InternalFormat, shadowMapSize, shadowMapSize, 0, _ShadowCubeMapTEX._DataFormat, _ShadowCubeMapTEX._DataType, nullptr);

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

  glGenTextures(1, &_Shadow2DMapTEX._Handle);
  glBindTexture(GL_TEXTURE_2D, _Shadow2DMapTEX._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, _Shadow2DMapTEX._InternalFormat, shadowMapSize, shadowMapSize, 0, _Shadow2DMapTEX._DataFormat, _Shadow2DMapTEX._DataType, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenFramebuffers(1, &_ShadowFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _ShadowFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _Shadow2DMapTEX._Handle, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "DeferredRenderer : Shadow framebuffer not complete !" << std::endl;
    return 1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}
// ----------------------------------------------------------------------------
// InitializeSSAO
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeSSAO()
{
  GLUtil::DeleteFBO(_SSAOFBO);
  GLUtil::DeleteFBO(_SSAOBlurFBO);
  GLUtil::DeleteTEX(_SSAOTEX);
  GLUtil::DeleteTEX(_SSAOBlurTEX);
  GLUtil::DeleteTEX(_SSAONoiseTEX);

  GLUtil::GenTexture(GL_TEXTURE_2D, _SSAOTEX._InternalFormat, RenderWidth(), RenderHeight(), _SSAOTEX._DataFormat, _SSAOTEX._DataType, nullptr, _SSAOTEX, GL_LINEAR, GL_LINEAR);
  glGenFramebuffers(1, &_SSAOFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _SSAOFBO._Handle);
  _SSAOFBO._Tex.clear();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _SSAOTEX._Handle, 0);
  _SSAOFBO._Tex.push_back(_SSAOTEX);
  GLenum ssaoDrawBuffers[] = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(1, ssaoDrawBuffers);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "DeferredRenderer : SSAO framebuffer not complete !" << std::endl;
    return 1;
  }

  GLUtil::GenTexture(GL_TEXTURE_2D, _SSAOBlurTEX._InternalFormat, RenderWidth(), RenderHeight(), _SSAOBlurTEX._DataFormat, _SSAOBlurTEX._DataType, nullptr, _SSAOBlurTEX, GL_LINEAR, GL_LINEAR);
  glGenFramebuffers(1, &_SSAOBlurFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _SSAOBlurFBO._Handle);
  _SSAOBlurFBO._Tex.clear();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _SSAOBlurTEX._Handle, 0);
  _SSAOBlurFBO._Tex.push_back(_SSAOBlurTEX);
  glDrawBuffers(1, ssaoDrawBuffers);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "DeferredRenderer : SSAO blur framebuffer not complete !" << std::endl;
    return 1;
  }

  std::mt19937 rng(1337u);
  std::uniform_real_distribution<float> dist01(0.f, 1.f);
  std::uniform_real_distribution<float> dist11(-1.f, 1.f);

  for ( int i = 0; i < (int)_SSAOKernel.size(); ++i )
  {
    Vec3 sample(dist11(rng), dist11(rng), dist01(rng));
    if ( glm::length(sample) > 0.f )
      sample = glm::normalize(sample);

    sample *= dist01(rng);
    float scale = float(i) / float(_SSAOKernel.size());
    scale = MathUtil::Lerp(0.1f, 1.0f, scale * scale);
    _SSAOKernel[i] = sample * scale;
  }

  std::vector<Vec4> noiseData;
  noiseData.reserve(16);
  for ( int i = 0; i < 16; ++i )
  {
    Vec3 noise(dist11(rng), dist11(rng), 0.f);
    if ( glm::length(noise) > 0.f )
      noise = glm::normalize(noise);
    noiseData.push_back(Vec4(noise, 1.f));
  }

  glGenTextures(1, &_SSAONoiseTEX._Handle);
  glBindTexture(GL_TEXTURE_2D, _SSAONoiseTEX._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, _SSAONoiseTEX._InternalFormat, 4, 4, 0, _SSAONoiseTEX._DataFormat, _SSAONoiseTEX._DataType, noiseData.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeBRDFLUT
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeBRDFLUT()
{
  GLUtil::DeleteFBO(_BRDFFBO);
  GLUtil::DeleteTEX(_BRDFLUTTEX);

  const int lutSize = 256;
  GLUtil::GenTexture(GL_TEXTURE_2D, _BRDFLUTTEX._InternalFormat, lutSize, lutSize, _BRDFLUTTEX._DataFormat, _BRDFLUTTEX._DataType, nullptr, _BRDFLUTTEX, GL_LINEAR, GL_LINEAR);

  glGenFramebuffers(1, &_BRDFFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _BRDFFBO._Handle);
  _BRDFFBO._Tex.clear();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _BRDFLUTTEX._Handle, 0);
  _BRDFFBO._Tex.push_back(_BRDFLUTTEX);

  GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(1, drawBuffers);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "DeferredRenderer : BRDF LUT framebuffer not complete !" << std::endl;
    return 1;
  }

  if ( _BRDFLUTShader )
  {
    glViewport(0, 0, lutSize, lutSize);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    _Quad.Render(*_BRDFLUTShader);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
  _TransparentMeshBaseIndices.assign(meshCount, {});
  _TransparentMeshLocalTriCenters.assign(meshCount, {});
  _TransparentMeshSortedIndices.assign(meshCount, {});
  _TransparentMeshSortedTriOrder.assign(meshCount, {});
  _TransparentMeshTriDepths.assign(meshCount, {});

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

    std::vector<Vec3> gpuPositions;
    gpuPositions.reserve(outVertices.size());
    for ( const GPUMeshVertex & vertex : outVertices )
      gpuPositions.push_back(vertex._Pos);
    BuildTransparentMeshTriangleData(mi, gpuPositions, outIndices);
  }

  ComputeSceneBounds();
  if ( _Settings._ShadowFar <= 0.f )
    _Settings._ShadowFar = std::max( _SceneBoundsRadius * 2.f, 25.f );

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

  BuildDeferredDrawLists();

  return 0;
}

// ----------------------------------------------------------------------------
// SetGenerateMipMaps
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
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

  // Material params (roughness, metallic, reflectance)
  GLUtil::GenTexture(GL_TEXTURE_2D, GL_RGBA16F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _GMaterialTEX);

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
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, _GMaterialTEX._Handle, 0);
  _GBufferFBO._Tex.push_back(_GMaterialTEX);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_TEXTURE_2D, _GDepthTEX._Handle, 0);
  _GBufferFBO._Tex.push_back(_GDepthTEX);

  GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
  glDrawBuffers(4, DrawBuffers);
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
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _GDepthTEX._Handle, 0);

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
  GLUtil::ResizeFBO(_SSAOFBO, RenderWidth(), RenderHeight());
  GLUtil::ResizeFBO(_SSAOBlurFBO, RenderWidth(), RenderHeight());

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

  ShaderSource ssaoFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_SSAO.glsl"));
  ShaderProgram* ssaoProg = ShaderProgram::LoadShaders(defaultVert, ssaoFrag);
  if (!ssaoProg)
    return 1;
  _SSAOShader.reset(ssaoProg);

  ShaderSource ssaoBlurFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_SSAOBlur.glsl"));
  ShaderProgram* ssaoBlurProg = ShaderProgram::LoadShaders(defaultVert, ssaoBlurFrag);
  if (!ssaoBlurProg)
    return 1;
  _SSAOBlurShader.reset(ssaoBlurProg);

  ShaderSource brdfLutFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_BRDFLUT.glsl"));
  ShaderProgram* brdfLutProg = ShaderProgram::LoadShaders(defaultVert, brdfLutFrag);
  if (!brdfLutProg)
    return 1;
  _BRDFLUTShader.reset(brdfLutProg);

  // Optional post-process/composite (reuse existing postprocess if desired)
  ShaderSource postFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_Postprocess.glsl"));
  ShaderProgram* postProg = ShaderProgram::LoadShaders(defaultVert, postFrag);
  if (!postProg)
    return 1;
  _CompositeShader.reset(postProg);

  // DEBUG : Wireframe shader
  ShaderSource wireFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_Wireframe.glsl"));
  ShaderProgram* wireProg = ShaderProgram::LoadShaders(geomVert, wireFrag);
  if (!wireProg)
    return 1;
  _WireframeShader.reset(wireProg);

  ShaderSource shadowCubeVert = Shader::LoadShader(PathUtils::GetShaderPath("vertex_ShadowCubeDepth.glsl"));
  ShaderSource shadowCubeFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_ShadowCubeDepth.glsl"));
  ShaderProgram* shadowCubeProg = ShaderProgram::LoadShaders(shadowCubeVert, shadowCubeFrag);
  if (!shadowCubeProg)
    return 1;
  _ShadowCubeShader.reset(shadowCubeProg);

  ShaderSource shadowDirVert = Shader::LoadShader(PathUtils::GetShaderPath("vertex_ShadowDirectionalDepth.glsl"));
  ShaderSource shadowDirFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_ShadowDirectionalDepth.glsl"));
  ShaderProgram* shadowDirProg = ShaderProgram::LoadShaders(shadowDirVert, shadowDirFrag);
  if (!shadowDirProg)
    return 1;
  _ShadowDirectionalShader.reset(shadowDirProg);

  ShaderSource transparentFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_DeferredTransparent.glsl"));
  ShaderProgram* transparentProg = ShaderProgram::LoadShaders(geomVert, transparentFrag);
  if ( !transparentProg )
    return 1;
  _TransparentShader.reset(transparentProg);
 
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
// BindSSAOPassTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindSSAOPassTextures()
{
  GLUtil::ActivateTextures(_GBufferFBO);
  GLUtil::ActivateTexture(_SSAONoiseTEX);

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
  GLUtil::ActivateTexture(_BRDFLUTTEX);
  GLUtil::ActivateTexture(_ShadowCubeMapTEX);
  GLUtil::ActivateTexture(_Shadow2DMapTEX);
  GLUtil::ActivateTexture(_SSAOBlurTEX);

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
  Mat4x4 V;
  _Scene.GetCamera().ComputeLookAtMatrix(V);

  float ratio = RenderWidth() / float(RenderHeight());
  float top, right;
  Mat4x4 P;
  _Scene.GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  Vec3 camPos = _Scene.GetCamera().GetPos();
  Vec3 camUp = _Scene.GetCamera().GetUp();
  Vec3 camRight = _Scene.GetCamera().GetRight();
  Vec3 camForward = _Scene.GetCamera().GetForward();
  float camFov = _Scene.GetCamera().GetFOV();

  if ( _DirtyStates & (unsigned long)DirtyState::SceneMaterials )
  {
    glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>((sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size()), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  if ( _GeometryShader )
  {
    _GeometryShader -> Use();
    _GeometryShader -> SetUniform("u_CameraPos", camPos);
    _GeometryShader -> SetUniform("u_View", V);
    _GeometryShader -> SetUniform("u_Proj", P);
    _GeometryShader -> SetUniform("u_TexIndTexture",    (int)DeferredTexSlot::_TexInd);
    _GeometryShader -> SetUniform("u_TexArrayTexture",  (int)DeferredTexSlot::_TexArray);
    _GeometryShader -> SetUniform("u_MaterialsTexture", (int)DeferredTexSlot::_Materials);
    _GeometryShader -> StopUsing();
  }

  if ( _SSAOShader )
  {
    _SSAOShader -> Use();
    _SSAOShader -> SetUniform("u_GNormal", (int)DeferredTexSlot::_GNormal);
    _SSAOShader -> SetUniform("u_GPosition", (int)DeferredTexSlot::_GPosition);
    _SSAOShader -> SetUniform("u_GDepth", (int)DeferredTexSlot::_GDepth);
    _SSAOShader -> SetUniform("u_SSAONoise", (int)DeferredTexSlot::_SSAONoise);
    _SSAOShader -> SetUniform("u_View", V);
    _SSAOShader -> SetUniform("u_Proj", P);
    if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    {
      _SSAOShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));
      _SSAOShader -> SetUniform("u_EnableSSAO", _Settings._SSAO ? 1 : 0);
      _SSAOShader -> SetUniform("u_SSAORadius", _Settings._SSAORadius);
      _SSAOShader -> SetUniform("u_SSAOBias", _Settings._SSAOBias);
      _SSAOShader -> SetUniform("u_KernelSize", std::min(std::max(_Settings._SSAOKernelSize, 1), 32));
      for ( int i = 0; i < (int)_SSAOKernel.size(); ++i )
        _SSAOShader -> SetUniform("u_KernelSamples[" + std::to_string(i) + "]", _SSAOKernel[i]);
    }
    _SSAOShader -> StopUsing();
  }

  if ( _SSAOBlurShader && ( _DirtyStates & (unsigned long)DirtyState::RenderSettings ) ) 
  {
    _SSAOBlurShader -> Use();
    _SSAOBlurShader -> SetUniform("u_SSAOInput", (int)DeferredTexSlot::_SSAO);
    _SSAOBlurShader -> SetUniform("u_GDepth", (int)DeferredTexSlot::_GDepth);
    _SSAOBlurShader -> SetUniform("u_GNormal", (int)DeferredTexSlot::_GNormal);
    _SSAOBlurShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));
    _SSAOBlurShader -> SetUniform("u_EnableBlur", _Settings._SSAOBlur ? 1 : 0);
    _SSAOBlurShader -> StopUsing();
  }

  if ( _LightingShader )
  {
    _LightingShader -> Use();
    _LightingShader -> SetUniform("u_Camera._Pos", camPos);
    _LightingShader -> SetUniform("u_Camera._Up", camUp);
    _LightingShader -> SetUniform("u_Camera._Right", camRight);
    _LightingShader -> SetUniform("u_Camera._Forward", camForward);
    _LightingShader -> SetUniform("u_Camera._FOV", camFov);

    if ( ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
      || ( _DirtyStates & (unsigned long)DirtyState::SceneLights )
      || ( _DirtyStates & (unsigned long)DirtyState::Textures )
      || ( _DirtyStates & (unsigned long)DirtyState::SceneInstances ) )
    {
      _LightingShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));

      _LightingShader -> SetUniform("u_GAlbedo",   (int)DeferredTexSlot::_GAlbedo);
      _LightingShader -> SetUniform("u_GNormal",   (int)DeferredTexSlot::_GNormal);
      _LightingShader -> SetUniform("u_GPosition", (int)DeferredTexSlot::_GPosition);
      _LightingShader -> SetUniform("u_GMaterial", (int)DeferredTexSlot::_GMaterial);
      _LightingShader -> SetUniform("u_GDepth",    (int)DeferredTexSlot::_GDepth);

      _LightingShader -> SetUniform("u_SSAOMap", (int)DeferredTexSlot::_SSAOBlur);
      _LightingShader -> SetUniform("u_EnableSSAO", _Settings._SSAO ? 1 : 0);
      _LightingShader -> SetUniform("u_SSAOIntensity", _Settings._SSAOIntensity);

      _LightingShader -> SetUniform("u_ShadowCubeMap", (int)DeferredTexSlot::_ShadowCubeMap);
      _LightingShader -> SetUniform("u_Shadow2DMap", (int)DeferredTexSlot::_Shadow2DMap);
      _LightingShader -> SetUniform("u_EnableShadowMapping", ( _Settings._ShadowMapping && _HasShadowLight ) ? ( 1 ) : ( 0 ));
      _LightingShader -> SetUniform("u_ShadowLightIndex", _ShadowLightIndex);
      _LightingShader -> SetUniform("u_ShadowLightType", (int)_ShadowLightType);
      _LightingShader -> SetUniform("u_ShadowLightPos", _ShadowLightPos);
      _LightingShader -> SetUniform("u_ShadowLightDir", _ShadowLightDir);
      _LightingShader -> SetUniform("u_ShadowLightViewProj", _ShadowDirectionalViewProj);
      _LightingShader -> SetUniform("u_ShadowBias", _Settings._ShadowBias);
      _LightingShader -> SetUniform("u_ShadowFar", _ShadowFar);

      _LightingShader -> SetUniform("u_BRDFLUT", (int)DeferredTexSlot::_BRDFLUT);
      _LightingShader -> SetUniform("u_EnableSpecularIBL", _Settings._SpecularIBL ? 1 : 0);
      _LightingShader -> SetUniform("u_SpecularIBLIntensity", _Settings._SpecularIBLIntensity);
    }

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

    if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    {
      _LightingShader -> SetUniform("u_BackgroundColor", _Settings._BackgroundColor);
      _LightingShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
      _LightingShader -> SetUniform("u_EnableBackground" , (int)_Settings._EnableBackGround);
      _LightingShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
      _LightingShader -> SetUniform("u_EnvMapRes", (float)_Scene.GetEnvMap().GetWidth(), (float)_Scene.GetEnvMap().GetHeight());
      _LightingShader -> SetUniform("u_EnvMap", (int)DeferredTexSlot::_EnvMap);
      float envMipCount = 1.f;
      if ( _Scene.GetEnvMap().GetWidth() > 0 && _Scene.GetEnvMap().GetHeight() > 0 )
      {
        int maxDim = std::max(_Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight());
        envMipCount = std::floor(std::log2((float)maxDim)) + 1.0f;
      }
      _LightingShader -> SetUniform("u_EnvMapMipCount", envMipCount);
    }

    _LightingShader -> SetUniform("u_DebugMode" , _DebugMode);

    _LightingShader -> StopUsing();
  }

  if ( _ShadowCubeShader )
  {
    _ShadowCubeShader -> Use();
    _ShadowCubeShader -> SetUniform("u_LightPos", _ShadowLightPos);
    _ShadowCubeShader -> SetUniform("u_FarPlane", _ShadowFar);
    _ShadowCubeShader -> StopUsing();
  }

  if ( _ShadowDirectionalShader )
  {
    _ShadowDirectionalShader -> Use();
    _ShadowDirectionalShader -> SetUniform("u_LightViewProj", _ShadowDirectionalViewProj);
    _ShadowDirectionalShader -> StopUsing();
  }

  if ( _WireframeShader )
  {
    _WireframeShader -> Use();
    _WireframeShader -> SetUniform("u_CameraPos", camPos);
    _WireframeShader -> SetUniform("u_View", V);
    _WireframeShader -> SetUniform("u_Proj", P);
    _WireframeShader -> SetUniform("u_WireColor", S_WireColor);
    _WireframeShader -> StopUsing();
  }

  if ( _TransparentShader )
  {
    _TransparentShader -> Use();
    _TransparentShader -> SetUniform("u_Camera._Pos", camPos);
    _TransparentShader -> SetUniform("u_Camera._Up", camUp);
    _TransparentShader -> SetUniform("u_Camera._Right", camRight);
    _TransparentShader -> SetUniform("u_Camera._Forward", camForward);
    _TransparentShader -> SetUniform("u_Camera._FOV", camFov);

    _TransparentShader -> SetUniform("u_View", V);
    _TransparentShader -> SetUniform("u_Proj", P);
    _TransparentShader -> SetUniform("u_TexIndTexture",    (int)DeferredTexSlot::_TexInd);
    _TransparentShader -> SetUniform("u_TexArrayTexture",  (int)DeferredTexSlot::_TexArray);
    _TransparentShader -> SetUniform("u_MaterialsTexture", (int)DeferredTexSlot::_Materials);
    _TransparentShader -> SetUniform("u_GDepth", (int)DeferredTexSlot::_GDepth);

    if ( _DirtyStates & (unsigned long)DirtyState::SceneLights )
    {
      int transparentNbLights = 0;
      for ( int i = 0; i < _Scene.GetNbLights(); ++i )
      {
        Light * curLight = _Scene.GetLight(i);
        if ( !curLight )
          continue;

        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_Pos"),      curLight -> _Pos);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_Emission"), curLight -> _Emission * curLight -> _Intensity);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_DirU"),     curLight -> _DirU);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_DirV"),     curLight -> _DirV);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_Radius"),   curLight -> _Radius);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_Area"),     curLight -> _Area);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights", i, "_Type"),     curLight -> _Type);

        transparentNbLights++;
        if ( transparentNbLights >= 32 )
          break;
      }
      _TransparentShader -> SetUniform("u_NbLights", transparentNbLights);
    }

    if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    {
      _TransparentShader -> SetUniform("u_EnvMapRes", (float)_Scene.GetEnvMap().GetWidth(), (float)_Scene.GetEnvMap().GetHeight());
      _TransparentShader -> SetUniform("u_EnvMap", (int)DeferredTexSlot::_EnvMap);
      _TransparentShader -> SetUniform("u_BRDFLUT", (int)DeferredTexSlot::_BRDFLUT);
      _TransparentShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
      _TransparentShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
      float transparentEnvMipCount = 1.f;
      if ( _Scene.GetEnvMap().GetWidth() > 0 && _Scene.GetEnvMap().GetHeight() > 0 )
      {
        int maxDim = std::max(_Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight());
        transparentEnvMipCount = std::floor(std::log2((float)maxDim)) + 1.0f;
      }
      _TransparentShader -> SetUniform("u_EnvMapMipCount", transparentEnvMipCount);
    }

    if ( ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
      || ( _DirtyStates & (unsigned long)DirtyState::SceneLights )
      || ( _DirtyStates & (unsigned long)DirtyState::Textures )
      || ( _DirtyStates & (unsigned long)DirtyState::SceneInstances ) )
    {
      _TransparentShader -> SetUniform("u_ShadowCubeMap", (int)DeferredTexSlot::_ShadowCubeMap);
      _TransparentShader -> SetUniform("u_Shadow2DMap", (int)DeferredTexSlot::_Shadow2DMap);
      _TransparentShader -> SetUniform("u_EnableShadowMapping", ( _Settings._ShadowMapping && _HasShadowLight ) ? ( 1 ) : ( 0 ));
      _TransparentShader -> SetUniform("u_ShadowLightIndex", _ShadowLightIndex);
      _TransparentShader -> SetUniform("u_ShadowLightType", (int)_ShadowLightType);
      _TransparentShader -> SetUniform("u_ShadowLightPos", _ShadowLightPos);
      _TransparentShader -> SetUniform("u_ShadowLightDir", _ShadowLightDir);
      _TransparentShader -> SetUniform("u_ShadowLightViewProj", _ShadowDirectionalViewProj);
      _TransparentShader -> SetUniform("u_ShadowBias", _Settings._ShadowBias);
      _TransparentShader -> SetUniform("u_ShadowFar", _ShadowFar);
    }

    _TransparentShader -> StopUsing();
  }

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
// RenderShadowMap
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderShadowMap()
{
  if ( !_Settings._ShadowMapping || !_HasShadowLight || !_ShadowFBO._Handle )
    return 0;

  glViewport(0, 0, _ShadowMapSize, _ShadowMapSize);
  glBindFramebuffer(GL_FRAMEBUFFER, _ShadowFBO._Handle);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  const std::vector<MeshInstance> & instances = _Scene.GetMeshInstances();

  if ( LightType::DistantLight == _ShadowLightType )
  {
    if ( !_ShadowDirectionalShader || !_Shadow2DMapTEX._Handle )
      return 0;

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _Shadow2DMapTEX._Handle, 0);
    glClear(GL_DEPTH_BUFFER_BIT);

    _ShadowDirectionalShader -> Use();
    _ShadowDirectionalShader -> SetUniform("u_LightViewProj", _ShadowDirectionalViewProj);

    for ( int instID : _OpaqueMeshInstanceIDs )
    {
      if ( ( instID < 0 ) || ( static_cast<size_t>(instID) >= instances.size() ) )
        continue;

      const MeshInstance & inst = instances[instID];
      int meshID = inst._MeshID;
      if ( ( meshID < 0 ) || ( static_cast<size_t>(meshID) >= _MeshVAOs.size() ) )
        continue;

      GLuint vao = _MeshVAOs[meshID];
      int idxCount = _MeshIndexCount[meshID];
      if ( !vao || idxCount <= 0 )
        continue;

      _ShadowDirectionalShader -> SetUniform("u_Model", inst._Transform);
      glBindVertexArray(vao);
      glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
    }

    _ShadowDirectionalShader -> StopUsing();
    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return 0;
  }

  if ( !_ShadowCubeShader || !_ShadowCubeMapTEX._Handle )
    return 0;

  _ShadowCubeShader -> Use();
  _ShadowCubeShader -> SetUniform("u_LightPos", _ShadowLightPos);
  _ShadowCubeShader -> SetUniform("u_FarPlane", _ShadowFar);

  for ( int face = 0; face < 6; ++face )
  {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, _ShadowCubeMapTEX._Handle, 0);
    glClear(GL_DEPTH_BUFFER_BIT);

    _ShadowCubeShader -> SetUniform("u_LightViewProj", _ShadowViewProj[face]);

    for ( int instID : _OpaqueMeshInstanceIDs )
    {
      if ( ( instID < 0 ) || ( static_cast<size_t>(instID) >= instances.size() ) )
        continue;

      const MeshInstance & inst = instances[instID];
      int meshID = inst._MeshID;
      if ( ( meshID < 0 ) || ( static_cast<size_t>(meshID) >= _MeshVAOs.size() ) )
        continue;

      GLuint vao = _MeshVAOs[meshID];
      int idxCount = _MeshIndexCount[meshID];
      if ( !vao || idxCount <= 0 )
        continue;

      _ShadowCubeShader -> SetUniform("u_Model", inst._Transform);
      glBindVertexArray(vao);
      glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
    }
  }

  glBindVertexArray(0);
  _ShadowCubeShader -> StopUsing();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderSSAO
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderSSAO()
{
  if ( !_SSAOFBO._Handle || !_SSAOBlurFBO._Handle || !_SSAOShader || !_SSAOBlurShader )
    return 0;

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);

  glBindFramebuffer(GL_FRAMEBUFFER, _SSAOFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());
  glClearColor(1.f, 1.f, 1.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  _SSAOShader -> Use();
  BindSSAOPassTextures();
  _Quad.Render(*_SSAOShader);
  _SSAOShader -> StopUsing();

  glBindFramebuffer(GL_FRAMEBUFFER, _SSAOBlurFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());
  glClearColor(1.f, 1.f, 1.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  _SSAOBlurShader -> Use();
  GLUtil::ActivateTexture(_SSAOTEX);
  GLUtil::ActivateTexture(_GDepthTEX);
  GLUtil::ActivateTexture(_GNormalTEX);
  _Quad.Render(*_SSAOBlurShader);
  _SSAOBlurShader -> StopUsing();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderTransparent
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderTransparent()
{
  if ( !_Settings._Transparency || !_TransparentShader || !_LightingFBO._Handle || !_GDepthTEX._Handle )
    return 0;

  const std::vector<MeshInstance> & instances = _Scene.GetMeshInstances();
  if ( _TransparentMeshInstanceIDs.empty() || instances.empty() )
    return 0;

  SortTransparentInstances();
  
  Mat4x4 view;
  _Scene.GetCamera().ComputeLookAtMatrix(view);

  glBindFramebuffer(GL_FRAMEBUFFER, _LightingFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _GDepthTEX._Handle, 0);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  _TransparentShader -> Use();
  BindLightingTextures();

  // Per-instance sorting is not enough for transparent shell meshes. Re-sorting
  // triangles back-to-front per draw stabilizes intra-mesh blending order.
  for ( int instID : _TransparentMeshInstanceIDs )
  {
    if ( ( instID < 0 ) || ( static_cast<size_t>(instID) >= instances.size() ) )
      continue;

    const MeshInstance & inst = instances[instID];
    int meshID = inst._MeshID;
    if ( ( meshID < 0 ) || ( static_cast<size_t>(meshID) >= _MeshVAOs.size() ) )
      continue;

    GLuint vao = _MeshVAOs[meshID];
    GLuint ebo = _MeshEBOs[meshID];
    int idxCount = _MeshIndexCount[meshID];
    if ( !vao || !ebo || ( idxCount <= 0 ) )
      continue;

    this -> UpdateSortedTransparentMeshIndices(meshID, inst._Transform, view);

    std::vector<uint32_t> & sortedIndices = _TransparentMeshSortedIndices[meshID];

    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sortedIndices.size() * sizeof(uint32_t)), sortedIndices.data());

    _TransparentShader -> SetUniform("u_Model", inst._Transform);
    _TransparentShader -> SetUniform("u_MaterialID", inst._MaterialID);
    glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  }

  glBindVertexArray(0);
  _TransparentShader -> StopUsing();

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderToTexture()
{
  if ( _Settings._ShadowMapping && _HasShadowLight )
    RenderShadowMap();

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
    glDisable(GL_CULL_FACE);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // now actually clears depth

    _GeometryShader -> Use();

    this -> BindGBufferTextures();

    const std::vector<MeshInstance> & instances = _Scene.GetMeshInstances();
    for ( int instID : _OpaqueMeshInstanceIDs )
    {
      if ( ( instID < 0 ) || ( static_cast<size_t>(instID) >= instances.size() ) )
        continue;

      const MeshInstance & inst = instances[instID];
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

  RenderSSAO();

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

  RenderTransparent();

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

    const std::vector<MeshInstance> & instances2 = _Scene.GetMeshInstances();
    for ( int instID : _OpaqueMeshInstanceIDs )
    {
      if ( ( instID < 0 ) || ( static_cast<size_t>(instID) >= instances2.size() ) )
        continue;

      const MeshInstance& inst = instances2[instID];
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
