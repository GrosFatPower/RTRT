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

static constexpr GLint S_DeferredNonTextureArraySamplers = 7;

// ----------------------------------------------------------------------------
// Texture arrays : ConfigureTextureBucketShader
// ----------------------------------------------------------------------------
static void ConfigureTextureBucketShader( ShaderSource & ioShaderSource, bool iUseTextureBuckets )
{
  if ( iUseTextureBuckets )
    return;

  const size_t versionEnd = ioShaderSource._Src.find('\n');
  if ( versionEnd != std::string::npos )
    ioShaderSource._Src.insert(versionEnd + 1, "#define USE_TEXTURE_BUCKETS 0\n");
}

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
  GLint maxFragmentSamplers = 0;
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxFragmentSamplers);
  _UseTextureBuckets = maxFragmentSamplers >= ( S_DeferredNonTextureArraySamplers + S_TextureBucketCount );
  if ( !_UseTextureBuckets )
    std::cout << "DeferredRenderer : " << maxFragmentSamplers << " fragment samplers available; using a single texture array compatibility path." << std::endl;

  for ( int i = 0; i < S_TextureBucketCount; ++i )
    _TexArrayTEX[i] = { 0, GL_TEXTURE_2D_ARRAY, DeferredTexSlot::_TexArray0 + static_cast<TextureSlot>(i), GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
DeferredRenderer::~DeferredRenderer()
{
  for ( auto & timerIDs : _TimerIDs )
  {
    if ( timerIDs[0] )
      glDeleteQueries(1, &timerIDs[0]);
    if ( timerIDs[1] )
      glDeleteQueries(1, &timerIDs[1]);
  }

  GLUtil::DeleteFBO(_GBufferFBO);
  GLUtil::DeleteFBO(_LightingFBO);
  GLUtil::DeleteFBO(_BRDFFBO);
  GLUtil::DeleteFBO(_ShadowFBO);
  GLUtil::DeleteFBO(_SSAOFBO);
  GLUtil::DeleteFBO(_SSAOBlurFBO);
  GLUtil::DeleteFBO(_SSRFBO);
  GLUtil::DeleteFBO(_SSRSourceFBO);

  GLUtil::DeleteTEX(_GAlbedoTEX);
  GLUtil::DeleteTEX(_GNormalTEX);
  GLUtil::DeleteTEX(_GPositionTEX);
  GLUtil::DeleteTEX(_GMaterialTEX);
  GLUtil::DeleteTEX(_GEmissionTEX);
  GLUtil::DeleteTEX(_GDepthTEX);
  GLUtil::DeleteTEX(_SSAOTEX);
  GLUtil::DeleteTEX(_SSAOBlurTEX);
  GLUtil::DeleteTEX(_SSAONoiseTEX);
  GLUtil::DeleteTEX(_SSRTEX);
  GLUtil::DeleteTEX(_SSRSourceTEX);
  GLUtil::DeleteTEX(_ShadowCubeMapTEX);
  GLUtil::DeleteTEX(_Shadow2DMapTEX);
  GLUtil::DeleteTEX(_BRDFLUTTEX);

  GLUtil::DeleteTBO(_TexIndTBO);
  for ( GLTexture & texture : _TexArrayTEX )
    GLUtil::DeleteTEX(texture);
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

  if ( 0 != InitializeSSR() )
  {
    std::cout << "DeferredRenderer : Failed to initialize SSR !" << std::endl;
    return 1;
  }

  UpdateShadowState();
  if ( 0 != InitializeShadowMap() )
  {
    std::cout << "DeferredRenderer : Failed to initialize shadow map !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeStats() )
  {
    std::cout << "DeferredRenderer : Failed to initialize frame statistics !" << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int DeferredRenderer::Update()
{
  UpdateStats();

  if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
  {
    this -> ResizeRenderTarget();
  }

  if ( _DirtyStates & (unsigned long)DirtyState::Textures )
  {
    if ( 0 != ReloadScene() )
      return 1;
  }

  if ( _DirtyStates & (unsigned long)DirtyState::SceneEnvMap )
    this -> ReloadEnvMap();

  if ( _DirtyStates & ( (unsigned long)DirtyState::SceneMaterials | (unsigned long)DirtyState::SceneInstances ) )
    BuildDeferredDrawLists();

  if ( _DirtyStates & (unsigned long)DirtyState::SceneInstances )
    ComputeSceneBounds(false);

  UpdateShadowState();
  int shadowMapSize = std::clamp(_Settings._ShadowMapResolution, 256, 4096);
  if ( ( _ShadowMapSize != shadowMapSize )
    || ( _ShadowLocalCapacity != _LocalShadowCasterCount )
    || ( _ShadowDirectionalCapacity != _DirectionalShadowCasterCount ) )
  {
    if ( 0 != InitializeShadowMap() )
      return 1;
  }

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
// InitializeStats
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeStats()
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
int DeferredRenderer::UpdateStats()
{
  for ( int i = 0; i < TimingCount; ++i )
  {
    if ( _TimerWritten[i] )
      _PassTimes[i] = ReadTimer(i);
    else
      _PassTimes[i] = 0.;
  }

  _PassEnabled.fill(false);

  return 0;
}

// ----------------------------------------------------------------------------
// BeginTimer
// ----------------------------------------------------------------------------
void DeferredRenderer::BeginTimer( int iTimerID )
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
void DeferredRenderer::EndTimer( int iTimerID )
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
double DeferredRenderer::ReadTimer( int iTimerID )
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
// SetTimingEnabled
// ----------------------------------------------------------------------------
void DeferredRenderer::SetTimingEnabled( int iTimerID, bool iEnabled )
{
  if ( ( iTimerID < 0 ) || ( iTimerID >= TimingCount ) )
    return;

  _PassEnabled[iTimerID] = iEnabled;
  if ( !iEnabled )
    _PassTimes[iTimerID] = 0.;
}

// ----------------------------------------------------------------------------
// GetRenderPassTimings
// ----------------------------------------------------------------------------
int DeferredRenderer::GetRenderPassTimings( std::vector<RenderPassTiming> & oTimings ) const
{
  oTimings.clear();
  oTimings.push_back({ "Shadow map", _PassTimes[TimingShadowMap], true, _PassEnabled[TimingShadowMap] });
  oTimings.push_back({ "G-buffer", _PassTimes[TimingGBuffer], true, _PassEnabled[TimingGBuffer] });
  oTimings.push_back({ "SSAO", _PassTimes[TimingSSAO], true, _PassEnabled[TimingSSAO] });
  oTimings.push_back({ "SSR", _PassTimes[TimingSSR], true, _PassEnabled[TimingSSR] });
  oTimings.push_back({ "Lighting", _PassTimes[TimingLighting], true, _PassEnabled[TimingLighting] });
  oTimings.push_back({ "Transparency", _PassTimes[TimingTransparency], true, _PassEnabled[TimingTransparency] });
  oTimings.push_back({ "Wireframe", _PassTimes[TimingWireframe], true, _PassEnabled[TimingWireframe] });
  oTimings.push_back({ "SSR source copy", _PassTimes[TimingSSRSourceCopy], true, _PassEnabled[TimingSSRSourceCopy] });
  oTimings.push_back({ "Composite / screen", _PassTimes[TimingCompositeScreen], true, _PassEnabled[TimingCompositeScreen] });
  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int DeferredRenderer::UnloadScene()
{
  _FrameNum = 0;
  GLUtil::DeleteTBO(_TexIndTBO);
  for ( GLTexture & texture : _TexArrayTEX )
    GLUtil::DeleteTEX(texture);

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

  _HasShadowLight = false;
  _ShadowCasters.clear();
  _LocalShadowCasterCount = 0;
  _DirectionalShadowCasterCount = 0;
  _ShadowSceneBoundsInitialized = false;

  return 0;
}

// ----------------------------------------------------------------------------
// ComputeSceneBounds
// ----------------------------------------------------------------------------
void DeferredRenderer::ComputeSceneBounds( bool iResetShadowBounds )
{
  bool initialized = false;
  Vec3 low(-10.f), high(10.f);

  const auto & instances = _Scene.GetMeshInstances();
  const auto & meshes = _Scene.GetMeshes();
  for ( const MeshInstance & inst : instances )
  {
    if ( !inst._Visible )
      continue;

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

  if ( iResetShadowBounds || !_ShadowSceneBoundsInitialized )
  {
    _ShadowSceneBounds = _SceneBounds;
    _ShadowSceneBoundsRadius = _SceneBoundsRadius;
    _ShadowSceneBoundsInitialized = true;
  }
  else
  {
    MathUtil::Minimize(_ShadowSceneBounds._Low, _SceneBounds._Low);
    MathUtil::Maximize(_ShadowSceneBounds._High, _SceneBounds._High);
    _ShadowSceneBoundsRadius = std::max( glm::length( _ShadowSceneBounds._High - _ShadowSceneBounds._Low ) * 0.5f, 1.f );
  }
}

// ----------------------------------------------------------------------------
// ComputeAutoShadowFar
// ----------------------------------------------------------------------------
float DeferredRenderer::ComputeAutoShadowFar( const Vec3 & iLightPos ) const
{
  Vec3 corners[8];
  _ShadowSceneBounds.Corners(corners);

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
  const MaterialPass pass = ClassifyMaterialPass(mat);
  return ( MaterialPass::Blend == pass ) || ( MaterialPass::Transmission == pass );
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
    if ( !inst._Visible )
      continue;

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
  _ShadowCasters.clear();
  _LocalShadowCasterCount = 0;
  _DirectionalShadowCasterCount = 0;
  _HasShadowLight = false;
  _ShadowFar = ( _Settings._ShadowFar > 0.f ) ? ( _Settings._ShadowFar ) : ( std::max( _ShadowSceneBoundsRadius * 2.f, 25.f ) );

  if ( !_Settings._ShadowMapping )
    return 0;

  struct ShadowCandidate
  {
    int _LightIndex = -1;
    LightType _Type = LightType::SphereLight;
    Vec3 _Pos = Vec3(0.f);
    Vec3 _Dir = Vec3(0.f, 1.f, 0.f);
    float _Far = 25.f;
    float _Score = 0.f;
  };

  std::vector<ShadowCandidate> candidates;
  candidates.reserve(_Scene.GetNbLights());
  Vec3 cameraPos = _Scene.GetCamera().GetPos();
  Vec3 sceneCenter = _SceneBounds.Center();
  for ( int i = 0; i < _Scene.GetNbLights(); ++i )
  {
    Light * curLight = _Scene.GetLight(i);
    if ( !curLight )
      continue;

    LightType lightType = (LightType) curLight -> _Type;
    if ( ( LightType::DistantLight != lightType )
      && ( LightType::SphereLight != lightType )
      && ( LightType::RectLight != lightType ) )
      continue;

    if ( !curLight -> _CastShadow || ( curLight -> _Intensity <= 0.f ) )
      continue;

    float emittedPower = glm::length(curLight -> _Emission * curLight -> _Intensity);
    if ( emittedPower <= 0.f )
      continue;

    ShadowCandidate candidate;
    candidate._LightIndex = i;
    candidate._Type = lightType;
    candidate._Pos = curLight -> _Pos;
    candidate._Far = ( curLight -> _ShadowRadius > 0.f ) ? ( curLight -> _ShadowRadius ) : ( _Settings._ShadowFar );

    if ( LightType::DistantLight == lightType )
    {
      candidate._Dir = ( glm::length(curLight -> _Pos) > 0.f ) ? ( glm::normalize(curLight -> _Pos) ) : ( Vec3(0.f, 1.f, 0.f) );
      candidate._Score = 1000000.f + emittedPower;
    }
    else
    {
      if ( candidate._Far <= 0.f )
        candidate._Far = ComputeAutoShadowFar(candidate._Pos);

      float cameraDistance = glm::length(candidate._Pos - cameraPos);
      float sceneDistance = glm::length(candidate._Pos - sceneCenter);
      float relevanceDistance = std::min(cameraDistance, sceneDistance);
      float effectiveRadius = std::max(candidate._Far, 1.f);
      candidate._Score = emittedPower / ( 1.f + relevanceDistance / effectiveRadius );
    }

    candidates.push_back(candidate);
  }

  std::stable_sort(candidates.begin(), candidates.end(), []( const ShadowCandidate & iA, const ShadowCandidate & iB )
  {
    if ( iA._Score == iB._Score )
      return iA._LightIndex < iB._LightIndex;
    return iA._Score > iB._Score;
  });

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

  int maxShadowCasters = std::clamp(_Settings._MaxShadowCastingLights, 1, S_MaxDeferredShadowCasters);
  int selectedCount = std::min(maxShadowCasters, static_cast<int>(candidates.size()));
  _ShadowCasters.reserve(selectedCount);
  for ( int i = 0; i < selectedCount; ++i )
  {
    const ShadowCandidate & candidate = candidates[i];
    ShadowCaster caster;
    caster._LightIndex = candidate._LightIndex;
    caster._Type = candidate._Type;
    caster._Pos = candidate._Pos;
    caster._Dir = candidate._Dir;
    caster._Far = candidate._Far;

    if ( LightType::DistantLight == caster._Type )
    {
      caster._Layer = _DirectionalShadowCasterCount++;

      float lightDistance = std::max( _ShadowSceneBoundsRadius * 2.f, 10.f );
      Vec3 lightPos = _ShadowSceneBounds.Center() + caster._Dir * lightDistance;
      Vec3 up = ( std::abs(glm::dot(caster._Dir, Vec3(0.f, 1.f, 0.f))) > 0.99f ) ? ( Vec3(0.f, 0.f, 1.f) ) : ( Vec3(0.f, 1.f, 0.f) );
      Mat4x4 lightView = glm::lookAt(lightPos, _ShadowSceneBounds.Center(), up);

      Vec3 corners[8];
      _ShadowSceneBounds.Corners(corners);

      Vec3 lightSpaceLow( MAX_FLOAT );
      Vec3 lightSpaceHigh( -MAX_FLOAT );
      for ( const Vec3 & corner : corners )
      {
        Vec4 lightSpaceCorner = lightView * Vec4(corner, 1.f);
        MathUtil::Minimize(lightSpaceLow, Vec3(lightSpaceCorner));
        MathUtil::Maximize(lightSpaceHigh, Vec3(lightSpaceCorner));
      }

      float padXY = std::max( _ShadowSceneBoundsRadius * 0.1f, 1.f );
      float nearPlane = 0.1f;
      float farPlane = lightDistance + _ShadowSceneBoundsRadius * 4.f;
      Mat4x4 shadowProj = glm::ortho(lightSpaceLow.x - padXY, lightSpaceHigh.x + padXY,
                                     lightSpaceLow.y - padXY, lightSpaceHigh.y + padXY,
                                     nearPlane, farPlane);
      caster._DirectionalViewProj = shadowProj * lightView;
    }
    else
    {
      caster._Layer = _LocalShadowCasterCount++;
      _ShadowFar = std::max(_ShadowFar, caster._Far);
      Mat4x4 shadowProj = glm::perspective(MathUtil::ToRadians(90.f), 1.0f, _ShadowNear, caster._Far);
      for ( int face = 0; face < 6; ++face )
      {
        Mat4x4 view = glm::lookAt(caster._Pos, caster._Pos + lookDirs[face], upDirs[face]);
        caster._CubeViewProj[face] = shadowProj * view;
      }
    }

    _ShadowCasters.push_back(caster);
  }

  _HasShadowLight = !_ShadowCasters.empty();
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
  _ShadowLocalCapacity = _LocalShadowCasterCount;
  _ShadowDirectionalCapacity = _DirectionalShadowCasterCount;

  GLTextureDesc shadowCubeDesc;
  shadowCubeDesc._Target         = _ShadowCubeMapTEX._Target;
  shadowCubeDesc._Slot           = _ShadowCubeMapTEX._Slot;
  shadowCubeDesc._Width          = ( _ShadowLocalCapacity > 0 ) ? shadowMapSize : 1;
  shadowCubeDesc._Height         = ( _ShadowLocalCapacity > 0 ) ? shadowMapSize : 1;
  shadowCubeDesc._Depth          = std::max(6, _ShadowLocalCapacity * 6);
  shadowCubeDesc._InternalFormat = _ShadowCubeMapTEX._InternalFormat;
  shadowCubeDesc._DataFormat     = _ShadowCubeMapTEX._DataFormat;
  shadowCubeDesc._DataType       = _ShadowCubeMapTEX._DataType;
  shadowCubeDesc._MinFilter      = GL_LINEAR;
  shadowCubeDesc._MagFilter      = GL_LINEAR;
  shadowCubeDesc._WrapS          = GL_CLAMP_TO_EDGE;
  shadowCubeDesc._WrapT          = GL_CLAMP_TO_EDGE;
  shadowCubeDesc._WrapR          = GL_CLAMP_TO_EDGE;
  GLUtil::CreateTexture(shadowCubeDesc, _ShadowCubeMapTEX);

  GLTextureDesc shadow2DDesc = shadowCubeDesc;
  shadow2DDesc._Target = _Shadow2DMapTEX._Target;
  shadow2DDesc._Slot   = _Shadow2DMapTEX._Slot;
  shadow2DDesc._Width  = ( _ShadowDirectionalCapacity > 0 ) ? shadowMapSize : 1;
  shadow2DDesc._Height = ( _ShadowDirectionalCapacity > 0 ) ? shadowMapSize : 1;
  shadow2DDesc._Depth  = std::max(1, _ShadowDirectionalCapacity);
  GLUtil::CreateTexture(shadow2DDesc, _Shadow2DMapTEX);

  if ( !_HasShadowLight )
    return 0;

  glGenFramebuffers(1, &_ShadowFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _ShadowFBO._Handle);
  if ( _ShadowDirectionalCapacity > 0 )
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _Shadow2DMapTEX._Handle, 0, 0);
  else
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _ShadowCubeMapTEX._Handle, 0, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    GLUtil::DeleteFBO(_ShadowFBO);
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

  GLTextureDesc ssaoDesc;
  ssaoDesc._Target         = _SSAOTEX._Target;
  ssaoDesc._Slot           = _SSAOTEX._Slot;
  ssaoDesc._Width          = RenderWidth();
  ssaoDesc._Height         = RenderHeight();
  ssaoDesc._InternalFormat = _SSAOTEX._InternalFormat;
  ssaoDesc._DataFormat     = _SSAOTEX._DataFormat;
  ssaoDesc._DataType       = _SSAOTEX._DataType;
  ssaoDesc._MinFilter      = GL_LINEAR;
  ssaoDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(ssaoDesc, _SSAOTEX);

  GLFrameBufferDesc ssaoFBODesc;
  ssaoFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_SSAOTEX });
  if ( !GLUtil::CreateFrameBuffer(ssaoFBODesc, _SSAOFBO) )
  {
    std::cout << "DeferredRenderer : SSAO framebuffer not complete !" << std::endl;
    return 1;
  }

  ssaoDesc._Slot           = _SSAOBlurTEX._Slot;
  ssaoDesc._InternalFormat = _SSAOBlurTEX._InternalFormat;
  ssaoDesc._DataFormat     = _SSAOBlurTEX._DataFormat;
  ssaoDesc._DataType       = _SSAOBlurTEX._DataType;
  GLUtil::CreateTexture(ssaoDesc, _SSAOBlurTEX);

  GLFrameBufferDesc ssaoBlurFBODesc;
  ssaoBlurFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_SSAOBlurTEX });
  if ( !GLUtil::CreateFrameBuffer(ssaoBlurFBODesc, _SSAOBlurFBO) )
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

  GLTextureDesc noiseDesc;
  noiseDesc._Target         = _SSAONoiseTEX._Target;
  noiseDesc._Slot           = _SSAONoiseTEX._Slot;
  noiseDesc._Width          = 4;
  noiseDesc._Height         = 4;
  noiseDesc._InternalFormat = _SSAONoiseTEX._InternalFormat;
  noiseDesc._DataFormat     = _SSAONoiseTEX._DataFormat;
  noiseDesc._DataType       = _SSAONoiseTEX._DataType;
  noiseDesc._Data           = noiseData.data();
  noiseDesc._MinFilter      = GL_NEAREST;
  noiseDesc._MagFilter      = GL_NEAREST;
  noiseDesc._WrapS          = GL_REPEAT;
  noiseDesc._WrapT          = GL_REPEAT;
  GLUtil::CreateTexture(noiseDesc, _SSAONoiseTEX);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeSSR
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeSSR()
{
  GLUtil::DeleteFBO(_SSRFBO);
  GLUtil::DeleteFBO(_SSRSourceFBO);
  GLUtil::DeleteTEX(_SSRTEX);
  GLUtil::DeleteTEX(_SSRSourceTEX);

  GLTextureDesc ssrDesc;
  ssrDesc._Target         = _SSRTEX._Target;
  ssrDesc._Slot           = _SSRTEX._Slot;
  ssrDesc._Width          = RenderWidth();
  ssrDesc._Height         = RenderHeight();
  ssrDesc._InternalFormat = _SSRTEX._InternalFormat;
  ssrDesc._DataFormat     = _SSRTEX._DataFormat;
  ssrDesc._DataType       = _SSRTEX._DataType;
  ssrDesc._MinFilter      = GL_LINEAR;
  ssrDesc._MagFilter      = GL_LINEAR;
  ssrDesc._WrapS          = GL_CLAMP_TO_EDGE;
  ssrDesc._WrapT          = GL_CLAMP_TO_EDGE;
  GLUtil::CreateTexture(ssrDesc, _SSRTEX);

  GLFrameBufferDesc ssrFBODesc;
  ssrFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_SSRTEX });
  if ( !GLUtil::CreateFrameBuffer(ssrFBODesc, _SSRFBO) )
  {
    std::cout << "DeferredRenderer : SSR framebuffer not complete !" << std::endl;
    return 1;
  }

  ssrDesc._Slot           = _SSRSourceTEX._Slot;
  ssrDesc._InternalFormat = _SSRSourceTEX._InternalFormat;
  ssrDesc._DataFormat     = _SSRSourceTEX._DataFormat;
  ssrDesc._DataType       = _SSRSourceTEX._DataType;
  GLUtil::CreateTexture(ssrDesc, _SSRSourceTEX);

  GLFrameBufferDesc ssrSourceFBODesc;
  ssrSourceFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_SSRSourceTEX });
  if ( !GLUtil::CreateFrameBuffer(ssrSourceFBODesc, _SSRSourceFBO) )
  {
    std::cout << "DeferredRenderer : SSR source framebuffer not complete !" << std::endl;
    return 1;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, _SSRSourceFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
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
  GLTextureDesc lutDesc;
  lutDesc._Target         = _BRDFLUTTEX._Target;
  lutDesc._Slot           = _BRDFLUTTEX._Slot;
  lutDesc._Width          = lutSize;
  lutDesc._Height         = lutSize;
  lutDesc._InternalFormat = _BRDFLUTTEX._InternalFormat;
  lutDesc._DataFormat     = _BRDFLUTTEX._DataFormat;
  lutDesc._DataType       = _BRDFLUTTEX._DataType;
  lutDesc._MinFilter      = GL_LINEAR;
  lutDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(lutDesc, _BRDFLUTTEX);

  GLFrameBufferDesc lutFBODesc;
  lutFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_BRDFLUTTEX });
  if ( !GLUtil::CreateFrameBuffer(lutFBODesc, _BRDFFBO) )
  {
    std::cout << "DeferredRenderer : BRDF LUT framebuffer not complete !" << std::endl;
    return 1;
  }

  if ( _BRDFLUTShader )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _BRDFFBO._Handle);
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
    _Scene.CompileMeshData( _Settings._TextureSize, true, false, _UseTextureBuckets );

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

  ComputeSceneBounds(true);

  // Materials
  const std::vector<TextureArrayMapping> & textureMappings = _Scene.GetTextureArrayMappings();
  const std::array<CompiledTextureBucket, S_TextureBucketCount> & textureBuckets = _Scene.GetCompiledTextureBuckets();
  if ( textureMappings.size() )
  {
    GLUtil::InitializeTBO(_TexIndTBO, sizeof(TextureArrayMapping) * textureMappings.size(), textureMappings.data(), GL_RGBA32I);
  }

  const int textureArrayCount = _UseTextureBuckets ? S_TextureBucketCount : 1;
  static const unsigned char fallbackPixel[] = { 255, 255, 255, 255 };
  for ( int i = 0; i < textureArrayCount; ++i )
  {
    const CompiledTextureBucket & bucket = textureBuckets[i];
    const bool hasTextureLayers = bucket._LayerCount > 0;
    GLTexture & texture = _TexArrayTEX[i];
    GLTextureDesc texArrayDesc;
    texArrayDesc._Target         = texture._Target;
    texArrayDesc._Slot           = texture._Slot;
    texArrayDesc._Width          = hasTextureLayers ? bucket._Size : 1;
    texArrayDesc._Height         = hasTextureLayers ? bucket._Size : 1;
    texArrayDesc._Depth          = hasTextureLayers ? bucket._LayerCount : 1;
    texArrayDesc._InternalFormat = texture._InternalFormat;
    texArrayDesc._DataFormat     = texture._DataFormat;
    texArrayDesc._DataType       = texture._DataType;
    texArrayDesc._Data           = hasTextureLayers ? bucket._Pixels.data() : fallbackPixel;
    texArrayDesc._MinFilter      = _GenerateMipMaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    texArrayDesc._MagFilter      = GL_LINEAR;
    texArrayDesc._GenerateMipMap = _GenerateMipMaps;
    GLUtil::CreateTexture(texArrayDesc, texture);

    if ( _GenerateMipMaps && _AnisotropicLevel )
      GLUtil::EnableAnisotropyIfAvailable(texture, (float)_AnisotropicLevel);
  }

  GLTextureDesc materialsDesc;
  materialsDesc._Target         = _MaterialsTEX._Target;
  materialsDesc._Slot           = _MaterialsTEX._Slot;
  materialsDesc._Width          = static_cast<GLsizei>((sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size());
  materialsDesc._Height         = 1;
  materialsDesc._InternalFormat = _MaterialsTEX._InternalFormat;
  materialsDesc._DataFormat     = _MaterialsTEX._DataFormat;
  materialsDesc._DataType       = _MaterialsTEX._DataType;
  materialsDesc._Data           = &_Scene.GetMaterials()[0];
  materialsDesc._MinFilter      = GL_NEAREST;
  materialsDesc._MagFilter      = GL_NEAREST;
  GLUtil::CreateTexture(materialsDesc, _MaterialsTEX);

  BindMaterialTextures();
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
    GLTextureDesc envDesc;
    envDesc._Target         = _EnvMapTEX._Target;
    envDesc._Slot           = _EnvMapTEX._Slot;
    envDesc._Width          = _Scene.GetEnvMap().GetWidth();
    envDesc._Height         = _Scene.GetEnvMap().GetHeight();
    envDesc._InternalFormat = _EnvMapTEX._InternalFormat;
    envDesc._DataFormat     = _EnvMapTEX._DataFormat;
    envDesc._DataType       = _EnvMapTEX._DataType;
    envDesc._Data           = _Scene.GetEnvMap().GetRawData();
    envDesc._MinFilter      = GL_LINEAR_MIPMAP_LINEAR;
    envDesc._MagFilter      = GL_LINEAR;
    envDesc._WrapS          = GL_REPEAT;
    envDesc._WrapT          = GL_CLAMP_TO_EDGE;
    envDesc._GenerateMipMap = true;
    GLUtil::CreateTexture(envDesc, _EnvMapTEX);

    _Scene.GetEnvMap().SetHandle(_EnvMapTEX._Handle);
  }
  else
  {
    _Settings._EnableSkybox = false;

    static const float fallbackColor[] = { 0.f, 0.f, 0.f };
    GLTextureDesc envDesc;
    envDesc._Target         = _EnvMapTEX._Target;
    envDesc._Slot           = _EnvMapTEX._Slot;
    envDesc._Width          = 1;
    envDesc._Height         = 1;
    envDesc._InternalFormat = _EnvMapTEX._InternalFormat;
    envDesc._DataFormat     = _EnvMapTEX._DataFormat;
    envDesc._DataType       = _EnvMapTEX._DataType;
    envDesc._Data           = fallbackColor;
    envDesc._MinFilter      = GL_LINEAR;
    envDesc._MagFilter      = GL_LINEAR;
    GLUtil::CreateTexture(envDesc, _EnvMapTEX);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int DeferredRenderer::InitializeFrameBuffers()
{
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * RenderScale());
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * RenderScale());

  GLTextureDesc targetDesc;
  targetDesc._Target = GL_TEXTURE_2D;
  targetDesc._Width  = RenderWidth();
  targetDesc._Height = RenderHeight();

  targetDesc._Slot           = _GAlbedoTEX._Slot;
  targetDesc._InternalFormat = _GAlbedoTEX._InternalFormat;
  targetDesc._DataFormat     = _GAlbedoTEX._DataFormat;
  targetDesc._DataType       = _GAlbedoTEX._DataType;
  GLUtil::CreateTexture(targetDesc, _GAlbedoTEX);

  targetDesc._Slot           = _GNormalTEX._Slot;
  targetDesc._InternalFormat = _GNormalTEX._InternalFormat;
  targetDesc._DataFormat     = _GNormalTEX._DataFormat;
  targetDesc._DataType       = _GNormalTEX._DataType;
  GLUtil::CreateTexture(targetDesc, _GNormalTEX);

  targetDesc._Slot           = _GPositionTEX._Slot;
  targetDesc._InternalFormat = _GPositionTEX._InternalFormat;
  targetDesc._DataFormat     = _GPositionTEX._DataFormat;
  targetDesc._DataType       = _GPositionTEX._DataType;
  GLUtil::CreateTexture(targetDesc, _GPositionTEX);

  targetDesc._Slot           = _GMaterialTEX._Slot;
  targetDesc._InternalFormat = _GMaterialTEX._InternalFormat;
  targetDesc._DataFormat     = _GMaterialTEX._DataFormat;
  targetDesc._DataType       = _GMaterialTEX._DataType;
  GLUtil::CreateTexture(targetDesc, _GMaterialTEX);

  targetDesc._Slot           = _GEmissionTEX._Slot;
  targetDesc._InternalFormat = _GEmissionTEX._InternalFormat;
  targetDesc._DataFormat     = _GEmissionTEX._DataFormat;
  targetDesc._DataType       = _GEmissionTEX._DataType;
  GLUtil::CreateTexture(targetDesc, _GEmissionTEX);

  targetDesc._Slot           = _GDepthTEX._Slot;
  targetDesc._InternalFormat = _GDepthTEX._InternalFormat;
  targetDesc._DataFormat     = _GDepthTEX._DataFormat;
  targetDesc._DataType       = _GDepthTEX._DataType;
  targetDesc._MinFilter      = GL_NEAREST;
  targetDesc._MagFilter      = GL_NEAREST;
  GLUtil::CreateTexture(targetDesc, _GDepthTEX);

  GLFrameBufferDesc gBufferDesc;
  gBufferDesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_GAlbedoTEX });
  gBufferDesc._Attachments.push_back({ GL_COLOR_ATTACHMENT1, &_GNormalTEX });
  gBufferDesc._Attachments.push_back({ GL_COLOR_ATTACHMENT2, &_GPositionTEX });
  gBufferDesc._Attachments.push_back({ GL_COLOR_ATTACHMENT3, &_GMaterialTEX });
  gBufferDesc._Attachments.push_back({ GL_COLOR_ATTACHMENT4, &_GEmissionTEX });
  gBufferDesc._Attachments.push_back({ GL_DEPTH_ATTACHMENT, &_GDepthTEX });
  if ( !GLUtil::CreateFrameBuffer(gBufferDesc, _GBufferFBO) )
  {
    std::cout << "DeferredRenderer : G-buffer framebuffer not complete !" << std::endl;
    return 1;
  }

  // Lighting target (single HDR target)
  targetDesc._Slot           = _LightingTEX._Slot;
  targetDesc._InternalFormat = _LightingTEX._InternalFormat;
  targetDesc._DataFormat     = _LightingTEX._DataFormat;
  targetDesc._DataType       = _LightingTEX._DataType;
  targetDesc._MinFilter      = GL_LINEAR;
  targetDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(targetDesc, _LightingTEX);

  GLFrameBufferDesc lightingDesc;
  lightingDesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_LightingTEX });
  if ( !GLUtil::CreateFrameBuffer(lightingDesc, _LightingFBO) )
  {
    std::cout << "DeferredRenderer : Lighting framebuffer not complete !" << std::endl;
    return 1;
  }

  GLUtil::ActivateTextures(_GBufferFBO);

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
  GLUtil::ResizeFBO(_SSRFBO, RenderWidth(), RenderHeight());
  GLUtil::ResizeFBO(_SSRSourceFBO, RenderWidth(), RenderHeight());
  GLUtil::ActivateTextures(_GBufferFBO);

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
  ConfigureTextureBucketShader(geomFrag, _UseTextureBuckets);
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

  ShaderSource ssrFrag = Shader::LoadShader(PathUtils::GetShaderPath("fragment_SSR.glsl"));
  ShaderProgram* ssrProg = ShaderProgram::LoadShaders(defaultVert, ssrFrag);
  if (!ssrProg)
    return 1;
  _SSRShader.reset(ssrProg);

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
  ConfigureTextureBucketShader(transparentFrag, _UseTextureBuckets);
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
  GLUtil::ActivateTexture(_GNormalTEX, DeferredTexSlot::_GNormal);
  GLUtil::ActivateTexture(_GPositionTEX, DeferredTexSlot::_GPosition);
  GLUtil::ActivateTexture(_GDepthTEX, DeferredTexSlot::_GDepth);
  GLUtil::ActivateTexture(_SSAONoiseTEX, DeferredTexSlot::_GEmission);

  return 0;
}

// ----------------------------------------------------------------------------
// BindSSRPassTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindSSRPassTextures()
{
  GLUtil::ActivateTexture(_GNormalTEX, DeferredTexSlot::_GNormal);
  GLUtil::ActivateTexture(_GPositionTEX, DeferredTexSlot::_GPosition);
  GLUtil::ActivateTexture(_GMaterialTEX, DeferredTexSlot::_GMaterial);
  GLUtil::ActivateTexture(_GDepthTEX, DeferredTexSlot::_GDepth);
  GLUtil::ActivateTexture(_SSRSourceTEX, DeferredTexSlot::_GEmission);
  GLUtil::ActivateTexture(_EnvMapTEX, DeferredTexSlot::_TexInd);

  return 0;
}

// ----------------------------------------------------------------------------
// BindMaterialTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindMaterialTextures()
{
  GLUtil::ActivateTexture(_TexIndTBO._Tex, DeferredMaterialPassTexSlot::_TextureIndices);
  const int textureArrayCount = _UseTextureBuckets ? S_TextureBucketCount : 1;
  for ( int i = 0; i < textureArrayCount; ++i )
    GLUtil::ActivateTexture(_TexArrayTEX[i], DeferredMaterialPassTexSlot::_TextureArray0 + i);
  GLUtil::ActivateTexture(_MaterialsTEX, DeferredMaterialPassTexSlot::_Materials);

  return 0;
}

// ----------------------------------------------------------------------------
// BindLightingTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindLightingTextures()
{
  GLUtil::ActivateTextures(_GBufferFBO);

  GLUtil::ActivateTexture(_EnvMapTEX, DeferredLightingPassTexSlot::_EnvMap);
  GLUtil::ActivateTexture(_BRDFLUTTEX, DeferredLightingPassTexSlot::_BRDFLUT);
  GLUtil::ActivateTexture(_ShadowCubeMapTEX, DeferredLightingPassTexSlot::_ShadowCubeMap);
  GLUtil::ActivateTexture(_Shadow2DMapTEX, DeferredLightingPassTexSlot::_Shadow2DMap);
  GLUtil::ActivateTexture(_SSAOBlurTEX, DeferredLightingPassTexSlot::_SSAO);
  GLUtil::ActivateTexture(_SSRTEX, DeferredLightingPassTexSlot::_SSR);

  return 0;
}

// ----------------------------------------------------------------------------
// BindTransparentTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindTransparentTextures()
{
  BindMaterialTextures();

  GLUtil::ActivateTexture(_EnvMapTEX, DeferredTransparentPassTexSlot::_EnvMap);
  GLUtil::ActivateTexture(_BRDFLUTTEX, DeferredTransparentPassTexSlot::_BRDFLUT);
  GLUtil::ActivateTexture(_ShadowCubeMapTEX, DeferredTransparentPassTexSlot::_ShadowCubeMap);
  GLUtil::ActivateTexture(_Shadow2DMapTEX, DeferredTransparentPassTexSlot::_Shadow2DMap);

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int DeferredRenderer::BindRenderToScreenTextures()
{
  GLUtil::ActivateTexture(_LightingTEX, 0);

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
    GLTextureDesc materialsDesc;
    materialsDesc._Target         = _MaterialsTEX._Target;
    materialsDesc._Slot           = _MaterialsTEX._Slot;
    materialsDesc._Width          = static_cast<GLsizei>((sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size());
    materialsDesc._Height         = 1;
    materialsDesc._InternalFormat = _MaterialsTEX._InternalFormat;
    materialsDesc._DataFormat     = _MaterialsTEX._DataFormat;
    materialsDesc._DataType       = _MaterialsTEX._DataType;
    materialsDesc._Data           = &_Scene.GetMaterials()[0];
    materialsDesc._MinFilter      = GL_NEAREST;
    materialsDesc._MagFilter      = GL_NEAREST;
    GLUtil::CreateTexture(materialsDesc, _MaterialsTEX);
  }

  if ( _GeometryShader )
  {
    _GeometryShader -> Use();
    _GeometryShader -> SetUniform("u_CameraPos", camPos);
    _GeometryShader -> SetUniform("u_View", V);
    _GeometryShader -> SetUniform("u_Proj", P);
    _GeometryShader -> SetUniform("u_TexIndTexture",    (int)DeferredMaterialPassTexSlot::_TextureIndices);
    const int textureArrayCount = _UseTextureBuckets ? S_TextureBucketCount : 1;
    for ( int i = 0; i < textureArrayCount; ++i )
      _GeometryShader -> SetUniform("u_TexArrayTexture" + std::to_string(i), (int)( DeferredMaterialPassTexSlot::_TextureArray0 + i ));
    if ( !_UseTextureBuckets )
      _GeometryShader -> SetUniform("u_TextureArraySize", _Scene.GetCompiledTextureBuckets()[0]._Size);
    _GeometryShader -> SetUniform("u_MaterialsTexture", (int)DeferredMaterialPassTexSlot::_Materials);
    _GeometryShader -> StopUsing();
  }

  if ( _SSAOShader )
  {
    _SSAOShader -> Use();
    _SSAOShader -> SetUniform("u_GNormal", (int)DeferredTexSlot::_GNormal);
    _SSAOShader -> SetUniform("u_GPosition", (int)DeferredTexSlot::_GPosition);
    _SSAOShader -> SetUniform("u_GDepth", (int)DeferredTexSlot::_GDepth);
    _SSAOShader -> SetUniform("u_SSAONoise", (int)DeferredTexSlot::_GEmission);
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
    _SSAOBlurShader -> SetUniform("u_SSAOInput", 0);
    _SSAOBlurShader -> SetUniform("u_GDepth", (int)DeferredTexSlot::_GDepth);
    _SSAOBlurShader -> SetUniform("u_GNormal", (int)DeferredTexSlot::_GNormal);
    _SSAOBlurShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));
    _SSAOBlurShader -> SetUniform("u_EnableBlur", _Settings._SSAOBlur ? 1 : 0);
    _SSAOBlurShader -> StopUsing();
  }

  if ( _SSRShader )
  {
    _SSRShader -> Use();
    _SSRShader -> SetUniform("u_GNormal", (int)DeferredTexSlot::_GNormal);
    _SSRShader -> SetUniform("u_GPosition", (int)DeferredTexSlot::_GPosition);
    _SSRShader -> SetUniform("u_GMaterial", (int)DeferredTexSlot::_GMaterial);
    _SSRShader -> SetUniform("u_GDepth", (int)DeferredTexSlot::_GDepth);
    _SSRShader -> SetUniform("u_SSRSource", (int)DeferredTexSlot::_GEmission);
    _SSRShader -> SetUniform("u_EnvMap", (int)DeferredTexSlot::_TexInd);
    _SSRShader -> SetUniform("u_View", V);
    _SSRShader -> SetUniform("u_Proj", P);
    _SSRShader -> SetUniform("u_Camera._Pos", camPos);
    _SSRShader -> SetUniform("u_Camera._Up", camUp);
    _SSRShader -> SetUniform("u_Camera._Right", camRight);
    _SSRShader -> SetUniform("u_Camera._Forward", camForward);
    _SSRShader -> SetUniform("u_Camera._FOV", camFov);
    if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    {
      _SSRShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));
      _SSRShader -> SetUniform("u_EnableSSR", _Settings._SSR ? 1 : 0);
      _SSRShader -> SetUniform("u_SSRMaxSteps", std::min(std::max(_Settings._SSRMaxSteps, 4), 128));
      _SSRShader -> SetUniform("u_SSRStepSize", _Settings._SSRStepSize);
      _SSRShader -> SetUniform("u_SSRMaxDistance", _Settings._SSRMaxDistance);
      _SSRShader -> SetUniform("u_SSRThickness", _Settings._SSRThickness);
      _SSRShader -> SetUniform("u_SSRMaxRoughness", _Settings._SSRMaxRoughness);
      _SSRShader -> SetUniform("u_SSRFade", _Settings._SSRFade);
      _SSRShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
      _SSRShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
      _SSRShader -> SetUniform("u_EnvMapRes", (float)_Scene.GetEnvMap().GetWidth(), (float)_Scene.GetEnvMap().GetHeight());
    }
    _SSRShader -> StopUsing();
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
      || ( _DirtyStates & (unsigned long)DirtyState::SceneCamera )
      || ( _DirtyStates & (unsigned long)DirtyState::Textures )
      || ( _DirtyStates & (unsigned long)DirtyState::SceneInstances ) )
    {
      _LightingShader -> SetUniform("u_Resolution", float(RenderWidth()), float(RenderHeight()));

      _LightingShader -> SetUniform("u_GAlbedo",   (int)DeferredTexSlot::_GAlbedo);
      _LightingShader -> SetUniform("u_GNormal",   (int)DeferredTexSlot::_GNormal);
      _LightingShader -> SetUniform("u_GPosition", (int)DeferredTexSlot::_GPosition);
      _LightingShader -> SetUniform("u_GMaterial", (int)DeferredTexSlot::_GMaterial);
      _LightingShader -> SetUniform("u_GEmission", (int)DeferredTexSlot::_GEmission);
      _LightingShader -> SetUniform("u_GDepth",    (int)DeferredTexSlot::_GDepth);

      _LightingShader -> SetUniform("u_SSAOMap", (int)DeferredLightingPassTexSlot::_SSAO);
      _LightingShader -> SetUniform("u_EnableSSAO", _Settings._SSAO ? 1 : 0);
      _LightingShader -> SetUniform("u_SSAOIntensity", _Settings._SSAOIntensity);
      _LightingShader -> SetUniform("u_SSRMap", (int)DeferredLightingPassTexSlot::_SSR);
      _LightingShader -> SetUniform("u_EnableSSR", _Settings._SSR ? 1 : 0);
      _LightingShader -> SetUniform("u_SSRIntensity", _Settings._SSRIntensity);
      _LightingShader -> SetUniform("u_SSRMaxRoughness", _Settings._SSRMaxRoughness);

      _LightingShader -> SetUniform("u_ShadowCubeMaps", (int)DeferredLightingPassTexSlot::_ShadowCubeMap);
      _LightingShader -> SetUniform("u_Shadow2DMaps", (int)DeferredLightingPassTexSlot::_Shadow2DMap);
      _LightingShader -> SetUniform("u_EnableShadowMapping", ( _Settings._ShadowMapping && _HasShadowLight ) ? ( 1 ) : ( 0 ));
      _LightingShader -> SetUniform("u_NbShadowCasters", static_cast<int>(_ShadowCasters.size()));
      _LightingShader -> SetUniform("u_ShadowBias", _Settings._ShadowBias);
      for ( int i = 0; i < static_cast<int>(_ShadowCasters.size()); ++i )
      {
        const ShadowCaster & caster = _ShadowCasters[i];
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_LightIndex"), caster._LightIndex);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Type"), (int)caster._Type);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Layer"), caster._Layer);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Far"), caster._Far);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Pos"), caster._Pos);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Dir"), caster._Dir);
        _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_DirectionalViewProj"), caster._DirectionalViewProj);
        for ( int face = 0; face < 6; ++face )
          _LightingShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_CubeViewProj[" + std::to_string(face) + "]"), caster._CubeViewProj[face]);
      }

      _LightingShader -> SetUniform("u_BRDFLUT", (int)DeferredLightingPassTexSlot::_BRDFLUT);
      _LightingShader -> SetUniform("u_EnableSpecularIBL", _Settings._SpecularIBL ? 1 : 0);
      _LightingShader -> SetUniform("u_SpecularIBLIntensity", _Settings._SpecularIBLIntensity);
      _LightingShader -> SetUniform("u_SpecularIBLMaxRoughness", _Settings._SpecularIBLMaxRoughness);
      _LightingShader -> SetUniform("u_EnablePBRDirectLighting", _Settings._PBRDirectLighting ? 1 : 0);
      _LightingShader -> SetUniform("u_DirectLightIntensity", _Settings._DirectLightIntensity);
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
      _LightingShader -> SetUniform("u_Ambient", _Settings._EnableUniformLight ? ( _Settings._UniformLightCol * 0.08f ) : Vec3(0.f));
      _LightingShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
      _LightingShader -> SetUniform("u_EnableBackground" , (int)_Settings._EnableBackGround);
      _LightingShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
      _LightingShader -> SetUniform("u_EnvMapRes", (float)_Scene.GetEnvMap().GetWidth(), (float)_Scene.GetEnvMap().GetHeight());
      _LightingShader -> SetUniform("u_EnvMap", (int)DeferredLightingPassTexSlot::_EnvMap);
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
    _TransparentShader -> SetUniform("u_TexIndTexture",    (int)DeferredMaterialPassTexSlot::_TextureIndices);
    const int textureArrayCount = _UseTextureBuckets ? S_TextureBucketCount : 1;
    for ( int i = 0; i < textureArrayCount; ++i )
      _TransparentShader -> SetUniform("u_TexArrayTexture" + std::to_string(i), (int)( DeferredMaterialPassTexSlot::_TextureArray0 + i ));
    if ( !_UseTextureBuckets )
      _TransparentShader -> SetUniform("u_TextureArraySize", _Scene.GetCompiledTextureBuckets()[0]._Size);
    _TransparentShader -> SetUniform("u_MaterialsTexture", (int)DeferredMaterialPassTexSlot::_Materials);

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
      _TransparentShader -> SetUniform("u_EnvMap", (int)DeferredTransparentPassTexSlot::_EnvMap);
      _TransparentShader -> SetUniform("u_BRDFLUT", (int)DeferredTransparentPassTexSlot::_BRDFLUT);
      _TransparentShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
      _TransparentShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
      _TransparentShader -> SetUniform("u_EnablePBRDirectLighting", _Settings._PBRDirectLighting ? 1 : 0);
      _TransparentShader -> SetUniform("u_DirectLightIntensity", _Settings._DirectLightIntensity);
      _TransparentShader -> SetUniform("u_SpecularIBLMaxRoughness", _Settings._SpecularIBLMaxRoughness);
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
      || ( _DirtyStates & (unsigned long)DirtyState::SceneCamera )
      || ( _DirtyStates & (unsigned long)DirtyState::Textures )
      || ( _DirtyStates & (unsigned long)DirtyState::SceneInstances ) )
    {
      _TransparentShader -> SetUniform("u_ShadowCubeMaps", (int)DeferredTransparentPassTexSlot::_ShadowCubeMap);
      _TransparentShader -> SetUniform("u_Shadow2DMaps", (int)DeferredTransparentPassTexSlot::_Shadow2DMap);
      _TransparentShader -> SetUniform("u_EnableShadowMapping", ( _Settings._ShadowMapping && _HasShadowLight ) ? ( 1 ) : ( 0 ));
      _TransparentShader -> SetUniform("u_NbShadowCasters", static_cast<int>(_ShadowCasters.size()));
      _TransparentShader -> SetUniform("u_ShadowBias", _Settings._ShadowBias);
      for ( int i = 0; i < static_cast<int>(_ShadowCasters.size()); ++i )
      {
        const ShadowCaster & caster = _ShadowCasters[i];
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_LightIndex"), caster._LightIndex);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Type"), (int)caster._Type);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Layer"), caster._Layer);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Far"), caster._Far);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Pos"), caster._Pos);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_Dir"), caster._Dir);
        _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_DirectionalViewProj"), caster._DirectionalViewProj);
        for ( int face = 0; face < 6; ++face )
          _TransparentShader -> SetUniform(GLUtil::UniformArrayElementName("u_ShadowCasters", i, "_CubeViewProj[" + std::to_string(face) + "]"), caster._CubeViewProj[face]);
      }
    }

    _TransparentShader -> StopUsing();
  }

  if ( _CompositeShader )
  {
    _CompositeShader -> Use();

    _CompositeShader -> SetUniform("u_ScreenTexture", 0);
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

  if ( !_ShadowDirectionalShader || !_ShadowCubeShader )
    return 0;

  glViewport(0, 0, _ShadowMapSize, _ShadowMapSize);
  glBindFramebuffer(GL_FRAMEBUFFER, _ShadowFBO._Handle);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  const std::vector<MeshInstance> & instances = _Scene.GetMeshInstances();
  auto renderOpaqueInstances = [&]( ShaderProgram * iShader )
  {
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

      iShader -> SetUniform("u_Model", inst._Transform);
      glBindVertexArray(vao);
      glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
    }
  };

  for ( const ShadowCaster & caster : _ShadowCasters )
  {
    if ( LightType::DistantLight == caster._Type )
    {
      if ( !_Shadow2DMapTEX._Handle )
        continue;

      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _Shadow2DMapTEX._Handle, 0, caster._Layer);
      glClear(GL_DEPTH_BUFFER_BIT);

      _ShadowDirectionalShader -> Use();
      _ShadowDirectionalShader -> SetUniform("u_LightViewProj", caster._DirectionalViewProj);
      renderOpaqueInstances(_ShadowDirectionalShader.get());
      _ShadowDirectionalShader -> StopUsing();
    }
    else
    {
      if ( !_ShadowCubeMapTEX._Handle )
        continue;

      _ShadowCubeShader -> Use();
      _ShadowCubeShader -> SetUniform("u_LightPos", caster._Pos);
      _ShadowCubeShader -> SetUniform("u_FarPlane", caster._Far);

      for ( int face = 0; face < 6; ++face )
      {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, _ShadowCubeMapTEX._Handle, 0, caster._Layer * 6 + face);
        glClear(GL_DEPTH_BUFFER_BIT);
        _ShadowCubeShader -> SetUniform("u_LightViewProj", caster._CubeViewProj[face]);
        renderOpaqueInstances(_ShadowCubeShader.get());
      }

      _ShadowCubeShader -> StopUsing();
    }
  }

  glBindVertexArray(0);
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
  GLUtil::ActivateTexture(_SSAOTEX, 0);
  GLUtil::ActivateTexture(_GDepthTEX, DeferredTexSlot::_GDepth);
  GLUtil::ActivateTexture(_GNormalTEX, DeferredTexSlot::_GNormal);
  _Quad.Render(*_SSAOBlurShader);
  _SSAOBlurShader -> StopUsing();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderSSR
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderSSR()
{
  if ( !_SSRFBO._Handle || !_SSRShader )
    return 1;

  glBindFramebuffer(GL_FRAMEBUFFER, _SSRFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glClearColor(0.f, 0.f, 0.f, 0.f);
  glClear(GL_COLOR_BUFFER_BIT);

  _SSRShader -> Use();
  BindSSRPassTextures();
  _Quad.Render(*_SSRShader);
  _SSRShader -> StopUsing();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return 0;
}

// ----------------------------------------------------------------------------
// UpdateSSRSource
// ----------------------------------------------------------------------------
int DeferredRenderer::UpdateSSRSource()
{
  if ( !_LightingFBO._Handle || !_SSRSourceFBO._Handle )
    return 1;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, _LightingFBO._Handle);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, _SSRSourceFBO._Handle);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0, 0, RenderWidth(), RenderHeight(),
                    0, 0, RenderWidth(), RenderHeight(),
                    GL_COLOR_BUFFER_BIT, GL_NEAREST);
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
  BindTransparentTextures();

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
  _PassEnabled.fill(false);

  if ( _Settings._ShadowMapping && _HasShadowLight )
  {
    BeginTimer(TimingShadowMap);
    RenderShadowMap();
    EndTimer(TimingShadowMap);
  }

  if (_GeometryShader)
  {
    BeginTimer(TimingGBuffer);

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

    BindMaterialTextures();

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

    EndTimer(TimingGBuffer);
  }
  else
    SetTimingEnabled(TimingGBuffer, false);

  const bool ssaoPassEnabled = _Settings._SSAO || ( 0 != ( _DebugMode & (int)DeferredDebugModes::SSAO ) );
  const bool ssrPassEnabled = _Settings._SSR || ( 0 != ( _DebugMode & (int)DeferredDebugModes::SSR ) );

  if ( ssaoPassEnabled )
  {
    BeginTimer(TimingSSAO);
    RenderSSAO();
    EndTimer(TimingSSAO);
  }

  if ( ssrPassEnabled )
  {
    BeginTimer(TimingSSR);
    RenderSSR();
    EndTimer(TimingSSR);
  }

  if (_LightingShader)
  {
    BeginTimer(TimingLighting);

    // Lighting pass: sample G-buffer and compute shading into lighting FBO
    glBindFramebuffer(GL_FRAMEBUFFER, _LightingFBO._Handle);
    glViewport(0, 0, RenderWidth(), RenderHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

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

    EndTimer(TimingLighting);
  }

  BeginTimer(TimingTransparency);
  RenderTransparent();
  EndTimer(TimingTransparency);
  if ( !_Settings._Transparency || _TransparentMeshInstanceIDs.empty() )
    SetTimingEnabled(TimingTransparency, false);

  // DEBUG : Wireframe overlay. Render lines into lighting target on top of shaded image
  if ( ( _DebugMode & (int)DeferredDebugModes::Wires ) && _WireframeShader )
  { 
    BeginTimer(TimingWireframe);

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

    EndTimer(TimingWireframe);
  }

  if ( 0 == ( _DebugMode & ~(int)DeferredDebugModes::Wires ) )
  {
    BeginTimer(TimingSSRSourceCopy);
    UpdateSSRSource();
    EndTimer(TimingSSRSourceCopy);
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
    BeginTimer(TimingCompositeScreen);

    _CompositeShader -> Use();

    this -> BindRenderToScreenTextures();

    _Quad.Render(*_CompositeShader);

    _CompositeShader -> StopUsing();

    EndTimer(TimingCompositeScreen);
  }
  else
    SetTimingEnabled(TimingCompositeScreen, false);

  return 0;
}

// ----------------------------------------------------------------------------
// ReadbackFinalColor
// ----------------------------------------------------------------------------
int DeferredRenderer::ReadbackFinalColor( RenderImage & oImage )
{
  if ( !_LightingTEX._Handle || ( RenderWidth() <= 0 ) || ( RenderHeight() <= 0 ) )
    return 1;

  oImage._Width = RenderWidth();
  oImage._Height = RenderHeight();
  oImage._Pixels.resize((size_t)oImage._Width * (size_t)oImage._Height * 4u);

  while ( GL_NO_ERROR != glGetError() ) {}
  glBindTexture(GL_TEXTURE_2D, _LightingTEX._Handle);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, oImage._Pixels.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  FlipImageVertically(oImage);

  return ( GL_NO_ERROR == glGetError() ) ? 0 : 1;
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
int DeferredRenderer::RenderToFile(const std::filesystem::path& iFilePath)
{
  // Render current lighting texture to an intermediary FBO bound to a temporary texture,
  // then read back pixels similar to PathTracer::RenderToFile
  GLFrameBuffer temporaryFBO;
  GLTexture temporaryTEX = { 0, GL_TEXTURE_2D, 5 };

  int w = _Settings._WindowResolution.x;
  int h = _Settings._WindowResolution.y;

  // Create temp texture and FBO
  GLTextureDesc tempDesc;
  tempDesc._Target         = temporaryTEX._Target;
  tempDesc._Slot           = temporaryTEX._Slot;
  tempDesc._Width          = w;
  tempDesc._Height         = h;
  tempDesc._InternalFormat = GL_RGBA32F;
  tempDesc._DataFormat     = GL_RGBA;
  tempDesc._DataType       = GL_FLOAT;
  tempDesc._MinFilter      = GL_LINEAR;
  tempDesc._MagFilter      = GL_LINEAR;
  GLUtil::CreateTexture(tempDesc, temporaryTEX);

  GLFrameBufferDesc tempFBODesc;
  tempFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &temporaryTEX });
  if ( !GLUtil::CreateFrameBuffer(tempFBODesc, temporaryFBO) )
  {
    GLUtil::DeleteTEX(temporaryTEX);
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
  glBindTexture(GL_TEXTURE_2D, temporaryTEX._Handle);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData);

  stbi_flip_vertically_on_write(true);
  int saved = stbi_write_png(iFilePath.string().c_str(), w, h, 4, frameData, w * 4);
  delete[] frameData;

  GLUtil::DeleteFBO(temporaryFBO);
  GLUtil::DeleteTEX(temporaryTEX);

  if ( saved && std::filesystem::exists(iFilePath) )
    std::cout << "Frame saved in " << std::filesystem::absolute(iFilePath) << std::endl;
  else
    std::cout << "ERROR : Failed to save screen capture in " << std::filesystem::absolute(iFilePath) << std::endl;

  return 0;
}

} // namespace RTRT
