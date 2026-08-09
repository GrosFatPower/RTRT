#ifndef _DeferredRenderer_
#define _DeferredRenderer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"
#include "ShaderProgram.h"
#include "PathUtils.h"
#include "Light.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace RTRT
{

static const int S_MaxDeferredShadowCasters = 8;

struct DeferredTexSlot
{
  static constexpr TextureSlot _GAlbedo       = 0;
  static constexpr TextureSlot _GNormal       = 1;
  static constexpr TextureSlot _GPosition     = 2;
  static constexpr TextureSlot _GMaterial     = 3;
  static constexpr TextureSlot _GDepth        = 4;
  static constexpr TextureSlot _GEmission     = 5; // Reuses the lighting slot while the lighting target is not sampled.
  static constexpr TextureSlot _Lighting      = 5;
  static constexpr TextureSlot _TexInd        = 6;
  static constexpr TextureSlot _TexArray      = 7;
  static constexpr TextureSlot _Materials     = 8;
  static constexpr TextureSlot _EnvMap        = 9;
  static constexpr TextureSlot _ShadowCubeMap = 10;
  static constexpr TextureSlot _Shadow2DMap   = 11;
  static constexpr TextureSlot _SSAO          = 12;
  static constexpr TextureSlot _SSAOBlur      = 13;
  static constexpr TextureSlot _SSAONoise     = 14;
  static constexpr TextureSlot _SSR           = 14; // Reuses the SSAO noise slot outside the SSAO pass.
  static constexpr TextureSlot _SSRSource     = 5;  // Reuses the lighting slot outside the lighting/composite passes.
  static constexpr TextureSlot _BRDFLUT       = 15;
};

enum class DeferredDebugModes
{
  ColorBuffer    = 0x000,
  DepthBuffer    = 0x001,
  Normals        = 0x002,
  Wires          = 0x004,
  Shadows        = 0x008,
  SSAO           = 0x010,
  SpecularIBL    = 0x020,
  MaterialParams = 0x040,
  SSR            = 0x080,
  DirectDiffuse  = 0x100,
  DirectSpecular = 0x200,
  Diagnostic     = 0x400,
  Position       = 0x800
};

class DeferredRenderer : public Renderer
{
public:
  DeferredRenderer(Scene& iScene, RenderSettings& iSettings);
  virtual ~DeferredRenderer();

  virtual int Initialize() override;
  virtual int Update() override;
  virtual int Done() override;

  virtual int RenderToTexture() override;
  virtual int RenderToScreen() override;
  virtual int RenderToFile(const std::filesystem::path& iFilePath) override;
  virtual int ReadbackFinalColor( RenderImage & oImage ) override;

  void SetGenerateMipMaps(bool iGenerate);
  bool GetGenerateMipMaps() const { return _GenerateMipMaps; }
  void SetAnisotropicLevel(int iLevel);
  int GetAnisotropicLevel() const { return _AnisotropicLevel; }
  float GetEffectiveShadowFar() const { return _ShadowFar; }
  virtual int GetRenderPassTimings( std::vector<RenderPassTiming> & oTimings ) const override;

  virtual DeferredRenderer * AsDeferredRenderer() override { return this; }

protected:

  int UnloadScene();
  int ReloadScene();
  int ReloadEnvMap();

  int InitializeFrameBuffers();
  int ResizeRenderTarget();
  int InitializeShadowMap();
  int InitializeSSAO();
  int InitializeSSR();
  int InitializeBRDFLUT();
  int InitializeStats();
  int UpdateStats();

  int RecompileShaders();

  int BindGBufferTextures();
  int BindSSAOPassTextures();
  int BindSSRPassTextures();
  int BindLightingTextures();
  int BindRenderToScreenTextures();

  int UpdateUniforms();
  int UpdateShadowState();
  int RenderShadowMap();
  int RenderSSAO();
  int RenderSSR();
  int RenderTransparent();
  int UpdateSSRSource();

  void BuildDeferredDrawLists();
  void SortTransparentInstances();
  bool IsTransparentMaterial(int iMaterialID);
  void BuildTransparentMeshTriangleData( size_t iMeshID, const std::vector<Vec3> & iPositions, const std::vector<uint32_t> & iIndices );
  bool UpdateSortedTransparentMeshIndices( int iMeshID, const Mat4x4 & iModel, const Mat4x4 & iView );

  void BeginTimer( int iTimerID );
  void EndTimer( int iTimerID );
  double ReadTimer( int iTimerID );
  void SetTimingEnabled( int iTimerID, bool iEnabled );

  void ComputeSceneBounds( bool iResetShadowBounds = false );
  float ComputeAutoShadowFar(const Vec3 & iLightPos) const;

  struct ShadowCaster
  {
    int       _LightIndex = -1;
    LightType _Type = LightType::SphereLight;
    int       _Layer = 0;
    Vec3      _Pos = Vec3(0.f);
    Vec3      _Dir = Vec3(0.f, 1.f, 0.f);
    float     _Far = 25.f;
    Mat4x4    _DirectionalViewProj = Mat4x4(1.f);
    std::array<Mat4x4, 6> _CubeViewProj;
  };

  QuadMesh _Quad;

  // G-buffer FBO and attachments
  GLFrameBuffer _GBufferFBO;
  GLTexture     _GAlbedoTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GAlbedo, GL_RGBA8,  GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture     _GNormalTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GNormal, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GPositionTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GPosition, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GMaterialTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GMaterial, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GEmissionTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GEmission, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GDepthTEX    = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GDepth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT };

  // Lighting target (single texture)
  GLFrameBuffer _LightingFBO;
  GLTexture     _LightingTEX  = { 0, GL_TEXTURE_2D, DeferredTexSlot::_Lighting, GL_RGBA32F, GL_RGBA, GL_FLOAT };

  // SSAO targets
  GLFrameBuffer _SSAOFBO;
  GLFrameBuffer _SSAOBlurFBO;
  GLTexture     _SSAOTEX      = { 0, GL_TEXTURE_2D, DeferredTexSlot::_SSAO, GL_R16F, GL_RED, GL_FLOAT };
  GLTexture     _SSAOBlurTEX  = { 0, GL_TEXTURE_2D, DeferredTexSlot::_SSAOBlur, GL_R16F, GL_RED, GL_FLOAT };
  GLTexture     _SSAONoiseTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_SSAONoise, GL_RGBA16F, GL_RGBA, GL_FLOAT };

  // SSR targets
  GLFrameBuffer _SSRFBO;
  GLFrameBuffer _SSRSourceFBO;
  GLTexture     _SSRTEX       = { 0, GL_TEXTURE_2D, DeferredTexSlot::_SSR, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _SSRSourceTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_SSRSource, GL_RGBA32F, GL_RGBA, GL_FLOAT };

  // BRDF LUT target
  GLFrameBuffer _BRDFFBO;
  GLTexture     _BRDFLUTTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_BRDFLUT, GL_RG16F, GL_RG, GL_FLOAT };

  // Shadow target
  GLFrameBuffer _ShadowFBO;
  GLTexture     _ShadowCubeMapTEX = { 0, GL_TEXTURE_CUBE_MAP_ARRAY, DeferredTexSlot::_ShadowCubeMap, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
  GLTexture     _Shadow2DMapTEX   = { 0, GL_TEXTURE_2D_ARRAY, DeferredTexSlot::_Shadow2DMap, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };

  // Scene data
  GLTextureBuffer _TexIndTBO     = { 0, { 0, GL_TEXTURE_BUFFER, DeferredTexSlot::_TexInd } };
  GLTexture       _TexArrayTEX   = { 0, GL_TEXTURE_2D_ARRAY, DeferredTexSlot::_TexArray, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture       _MaterialsTEX  = { 0, GL_TEXTURE_2D, DeferredTexSlot::_Materials, GL_RGBA32F, GL_RGBA, GL_FLOAT };
  GLTexture       _EnvMapTEX     = { 0, GL_TEXTURE_2D, DeferredTexSlot::_EnvMap, GL_RGB32F,  GL_RGB,  GL_FLOAT };

  // Shaders
  std::unique_ptr<ShaderProgram> _GeometryShader;
  std::unique_ptr<ShaderProgram> _LightingShader;
  std::unique_ptr<ShaderProgram> _CompositeShader;
  std::unique_ptr<ShaderProgram> _WireframeShader;
  std::unique_ptr<ShaderProgram> _ShadowCubeShader;
  std::unique_ptr<ShaderProgram> _ShadowDirectionalShader;
  std::unique_ptr<ShaderProgram> _SSAOShader;
  std::unique_ptr<ShaderProgram> _SSAOBlurShader;
  std::unique_ptr<ShaderProgram> _SSRShader;
  std::unique_ptr<ShaderProgram> _BRDFLUTShader;
  std::unique_ptr<ShaderProgram> _TransparentShader;

  // Frame counters
  unsigned int _FrameNum = 1;

  // GPU mesh resources (one entry per Scene::GetMeshes())
  std::vector<GLuint> _MeshVAOs;
  std::vector<GLuint> _MeshVBOs;
  std::vector<GLuint> _MeshEBOs;
  std::vector<int>    _MeshIndexCount;
  std::vector<std::vector<uint32_t>> _TransparentMeshBaseIndices;
  std::vector<std::vector<Vec3>>     _TransparentMeshLocalTriCenters;
  std::vector<std::vector<uint32_t>> _TransparentMeshSortedIndices;
  std::vector<std::vector<int>>      _TransparentMeshSortedTriOrder;
  std::vector<std::vector<float>>    _TransparentMeshTriDepths;
  std::vector<int>    _OpaqueMeshInstanceIDs;
  std::vector<int>    _TransparentMeshInstanceIDs;

  // Scene bounds
  AABB<Vec3> _SceneBounds;
  float      _SceneBoundsRadius = 1.f;
  AABB<Vec3> _ShadowSceneBounds;
  float      _ShadowSceneBoundsRadius = 1.f;
  bool       _ShadowSceneBoundsInitialized = false;

  // Shadow state
  float _ShadowNear                 = 0.1f;
  float _ShadowFar                  = 25.f;
  int _ShadowMapSize                = -1;
  int _ShadowLocalCapacity          = -1;
  int _ShadowDirectionalCapacity    = -1;
  int _LocalShadowCasterCount       = 0;
  int _DirectionalShadowCasterCount = 0;
  bool _HasShadowLight              = false;
  std::vector<ShadowCaster> _ShadowCasters;

  // SSAO state
  std::array<Vec3, 32> _SSAOKernel;

  // Textures filtering
  bool _GenerateMipMaps = true;
  int  _AnisotropicLevel = 16;

  enum TimingID
  {
    TimingShadowMap = 0,
    TimingGBuffer,
    TimingSSAO,
    TimingSSR,
    TimingLighting,
    TimingTransparency,
    TimingWireframe,
    TimingSSRSourceCopy,
    TimingCompositeScreen,
    TimingCount
  };

  std::array<double, TimingCount> _PassTimes = {};
  std::array<bool, TimingCount>   _PassEnabled = {};
  std::array<bool, TimingCount>   _TimerWritten = {};
  std::array<std::array<GLuint, 2>, TimingCount> _TimerIDs = {};
};

} // namespace RTRT

#endif /* _DeferredRenderer_ */
