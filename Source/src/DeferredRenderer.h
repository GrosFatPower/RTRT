#ifndef _DeferredRenderer_
#define _DeferredRenderer_

#include "Renderer.h"
#include "RenderSettings.h"
#include "QuadMesh.h"
#include "GLUtil.h"
#include "ShaderProgram.h"
#include "PathUtils.h"

#include <memory>
#include <vector>

namespace RTRT
{

struct DeferredTexSlot
{
  static const TextureSlot _GAlbedo   = 0;
  static const TextureSlot _GNormal   = 1;
  static const TextureSlot _GPosition = 2;
  static const TextureSlot _GDepth    = 3;
  static const TextureSlot _Lighting  = 4;
  static const TextureSlot _TexInd    = 5;
  static const TextureSlot _TexArray  = 6;
  static const TextureSlot _Materials = 7;
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

protected:

  int UnloadScene();
  int ReloadScene();

  int InitializeFrameBuffers();
  int ResizeRenderTarget();

  int ResizeGBuffer();
  int RecompileShaders();

  int BindGBufferTextures();
  int BindLightingTextures();
  int BindRenderToScreenTextures();

  int UpdateUniforms();

  QuadMesh _Quad;

  // G-buffer FBO and attachments
  GLFrameBuffer _GBufferFBO;
  GLTexture     _GAlbedoTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GAlbedo, GL_RGBA8,  GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture     _GNormalTEX   = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GNormal, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GPositionTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GPosition, GL_RGBA16F, GL_RGBA, GL_FLOAT };
  GLTexture     _GDepthTEX    = { 0, GL_TEXTURE_2D, DeferredTexSlot::_GDepth, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT };

  // Lighting target (single texture)
  GLFrameBuffer _LightingFBO;
  GLTexture     _LightingTEX  = { 0, GL_TEXTURE_2D, DeferredTexSlot::_Lighting, GL_RGBA32F, GL_RGBA, GL_FLOAT };

  // Scene data
  GLTextureBuffer _TexIndTBO  = { 0, { 0, GL_TEXTURE_BUFFER, DeferredTexSlot::_TexInd } };
  GLTexture       _TexArrayTEX  = { 0, GL_TEXTURE_2D_ARRAY, DeferredTexSlot::_TexArray, GL_RGBA8,   GL_RGBA, GL_UNSIGNED_BYTE };
  GLTexture       _MaterialsTEX = { 0, GL_TEXTURE_2D, DeferredTexSlot::_Materials, GL_RGBA32F, GL_RGBA, GL_FLOAT };

  // Shaders
  std::unique_ptr<ShaderProgram> _GeometryShader;
  std::unique_ptr<ShaderProgram> _LightingShader;
  std::unique_ptr<ShaderProgram> _CompositeShader;

  // Frame counters
  unsigned int _FrameNum = 1;

  // GPU mesh resources (one entry per Scene::GetMeshes())
  std::vector<GLuint> _MeshVAOs;
  std::vector<GLuint> _MeshVBOs;
  std::vector<GLuint> _MeshEBOs;
  std::vector<int>    _MeshIndexCount;
};

} // namespace RTRT

#endif /* _DeferredRenderer_ */
