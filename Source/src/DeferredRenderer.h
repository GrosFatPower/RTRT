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
#include <memory>
#include <vector>

namespace RTRT
{

struct DeferredTexSlot
{
  static const TextureSlot _GAlbedo       = 0;
  static const TextureSlot _GNormal       = 1;
  static const TextureSlot _GPosition     = 2;
  static const TextureSlot _GMaterial     = 3;
  static const TextureSlot _GDepth        = 4;
  static const TextureSlot _Lighting      = 5;
  static const TextureSlot _TexInd        = 6;
  static const TextureSlot _TexArray      = 7;
  static const TextureSlot _Materials     = 8;
  static const TextureSlot _EnvMap        = 9;
  static const TextureSlot _ShadowCubeMap = 10;
  static const TextureSlot _Shadow2DMap   = 11;
  static const TextureSlot _SSAO          = 12;
  static const TextureSlot _SSAOBlur      = 13;
  static const TextureSlot _SSAONoise     = 14;
  static const TextureSlot _BRDFLUT       = 15;
};

enum class DeferredDebugModes
{
  ColorBuffer = 0x00,
  DepthBuffer = 0x01,
  Normals     = 0x02,
  Wires       = 0x04,
  Shadows     = 0x08,
  SSAO        = 0x10,
  SpecularIBL = 0x20,
  MaterialParams = 0x40
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

  void SetGenerateMipMaps(bool iGenerate);
  bool GetGenerateMipMaps() const { return _GenerateMipMaps; }
  void SetAnisotropicLevel(int iLevel);
  int GetAnisotropicLevel() const { return _AnisotropicLevel; }

  virtual DeferredRenderer * AsDeferredRenderer() override { return this; }

protected:

  int UnloadScene();
  int ReloadScene();
  int ReloadEnvMap();

  int InitializeFrameBuffers();
  int ResizeRenderTarget();
  int InitializeShadowMap();
  int InitializeSSAO();
  int InitializeBRDFLUT();

  int RecompileShaders();

  int BindGBufferTextures();
  int BindSSAOPassTextures();
  int BindLightingTextures();
  int BindRenderToScreenTextures();

  int UpdateUniforms();
  int UpdateShadowState();
  int RenderShadowMap();
  int RenderSSAO();
  int RenderTransparent();

  void BuildDeferredDrawLists();
  void SortTransparentInstances();
  bool IsTransparentMaterial(int iMaterialID);

  void ComputeSceneBounds();
  float ComputeAutoShadowFar(const Vec3 & iLightPos) const;

  QuadMesh _Quad;

  // G-buffer FBO and attachments
  GLFrameBuffer _GBufferFBO;
  GLTexture     _GAlbedoTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GAlbedo, GL_RGBA8,  GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture     _GNormalTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GNormal, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GPositionTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GPosition, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GMaterialTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GMaterial, GL_RGBA16F, GL_RGBA, GL_FLOAT };
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

  // BRDF LUT target
  GLFrameBuffer _BRDFFBO;
  GLTexture     _BRDFLUTTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_BRDFLUT, GL_RG16F, GL_RG, GL_FLOAT };

  // Shadow target
  GLFrameBuffer _ShadowFBO;
  GLTexture     _ShadowCubeMapTEX = { 0, GL_TEXTURE_CUBE_MAP, DeferredTexSlot::_ShadowCubeMap, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
  GLTexture     _Shadow2DMapTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_Shadow2DMap, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };

  // Scene data
  GLTextureBuffer _TexIndTBO     = { 0, { 0, GL_TEXTURE_BUFFER, DeferredTexSlot::_TexInd } };
  GLTexture       _TexArrayTEX   = { 0, GL_TEXTURE_2D_ARRAY, DeferredTexSlot::_TexArray, GL_RGBA8,   GL_RGBA, GL_UNSIGNED_BYTE };
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
  std::unique_ptr<ShaderProgram> _BRDFLUTShader;
  std::unique_ptr<ShaderProgram> _TransparentShader;

  // Frame counters
  unsigned int _FrameNum = 1;

  // GPU mesh resources (one entry per Scene::GetMeshes())
  std::vector<GLuint> _MeshVAOs;
  std::vector<GLuint> _MeshVBOs;
  std::vector<GLuint> _MeshEBOs;
  std::vector<int>    _MeshIndexCount;
  std::vector<int>    _OpaqueMeshInstanceIDs;
  std::vector<int>    _TransparentMeshInstanceIDs;

  // Scene bounds
  AABB<Vec3> _SceneBounds;
  float      _SceneBoundsRadius = 1.f;

  // Shadow state
  int _ShadowLightIndex      = -1;
  LightType _ShadowLightType = LightType::SphereLight;
  Vec3 _ShadowLightPos       = Vec3(0.f);
  Vec3 _ShadowLightDir       = Vec3(0.f, 1.f, 0.f);
  float _ShadowNear          = 0.1f;
  float _ShadowFar           = 25.f;
  int _ShadowMapSize         = -1;
  bool _HasShadowLight       = false;
  std::array<Mat4x4, 6> _ShadowViewProj;
  Mat4x4 _ShadowDirectionalViewProj = Mat4x4(1.f);

  // SSAO state
  std::array<Vec3, 32> _SSAOKernel;

  // Textures filtering
  bool _GenerateMipMaps = true;
  int  _AnisotropicLevel = 16;
};

} // namespace RTRT

#endif /* _DeferredRenderer_ */
