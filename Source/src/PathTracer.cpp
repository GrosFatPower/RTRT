#include "PathTracer.h"

#include "Scene.h"
#include "ShaderProgram.h"
#include "GLUtil.h"

#include <string>
#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "glm/gtc/type_ptr.hpp"

#define TEX_UNIT(x) ( GL_TEXTURE0 + (int)x._Slot )

namespace RTRT
{

// ----------------------------------------------------------------------------
// HELPER FUNCTIONS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// InitializeTBO
// ----------------------------------------------------------------------------
static void InitializeTBO( PathTracer::GLTextureBuffer & ioTBO, GLsizeiptr iSize, const void * iData, GLenum iInternalformat )
{
  glGenBuffers(1, &ioTBO._ID);
  glBindBuffer(GL_TEXTURE_BUFFER, ioTBO._ID);
  glBufferData(GL_TEXTURE_BUFFER, iSize, iData, GL_STATIC_DRAW);
  glGenTextures(1, &ioTBO._Tex._ID);
  glBindTexture(GL_TEXTURE_BUFFER, ioTBO._Tex._ID);
  glTexBuffer(GL_TEXTURE_BUFFER, iInternalformat, ioTBO._ID);
}

// ----------------------------------------------------------------------------
// ResizeFBO
// ----------------------------------------------------------------------------
static void ResizeFBO( PathTracer::GLFrameBuffer & ioFBO, GLint iInternalFormat, GLsizei iWidth, GLsizei iHeight, GLenum iFormat, GLenum iType )
{
  glBindTexture(GL_TEXTURE_2D, ioFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, iInternalFormat, iWidth, iHeight, 0, iFormat, iType, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, ioFBO._ID);
  glClear(GL_COLOR_BUFFER_BIT);
}

// ----------------------------------------------------------------------------
// METHODS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
PathTracer::PathTracer( Scene & iScene, RenderSettings & iSettings )
: Renderer(iScene, iSettings)
{
  UpdateRenderResolution();
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
PathTracer::~PathTracer()
{
  DeleteFBO(_RenderTargetFBO);
  DeleteFBO(_RenderTargetLowResFBO);
  DeleteFBO(_RenderTargetTileFBO);
  DeleteFBO(_AccumulateFBO);

  UnloadScene();
}

// ----------------------------------------------------------------------------
// DeleteTEX
// ----------------------------------------------------------------------------
void PathTracer::DeleteTEX( GLTexture & ioTEX )
{
  glDeleteTextures(1, &ioTEX._ID);
  ioTEX._ID = 0;
}

// ----------------------------------------------------------------------------
// DeleteFBO
// ----------------------------------------------------------------------------
void PathTracer::DeleteFBO( GLFrameBuffer & ioFBO )
{
  glDeleteFramebuffers(1, &ioFBO._ID);
  DeleteTEX(ioFBO._Tex);
  ioFBO._ID;
}

// ----------------------------------------------------------------------------
// DeleteTBO
// ----------------------------------------------------------------------------
void PathTracer::DeleteTBO( GLTextureBuffer & ioTBO )
{
  glDeleteBuffers(1, &ioTBO._ID);
  DeleteTEX(ioTBO._Tex);
  ioTBO._ID = 0;
}

// ----------------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------------
int PathTracer::Initialize()
{
  if ( 0 != ReloadScene() )
  {
    std::cout << "PathTracer : Failed to load scene !" << std::endl;
    return 1;
  }

  if ( ( 0 != RecompileShaders() ) || !_PathTraceShader || !_AccumulateShader || !_RenderToScreenShader )
  {
    std::cout << "PathTracer : Shader compilation failed !" << std::endl;
    return 1;
  }

  if ( 0 != InitializeFrameBuffers() )
  {
    std::cout << "PathTracer : Failed to initialize frame buffers !" << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int PathTracer::Update()
{
  //if ( 0 != ReloadScene() )
  //{
  //  std::cout << "PathTracer : Failed to reload scene!" << std::endl;
  //  return 1;
  //}

  if ( Dirty() )
    this -> ResetTiles();
  else if ( TiledRendering() )
    this -> NextTile();

  if ( _DirtyState._RenderSettings )
    this -> ResizeRenderTarget();

  this -> UpdatePathTraceUniforms();
  this -> UpdateAccumulateUniforms();
  this -> UpdateRenderToScreenUniforms();

  return 0;
}

// ----------------------------------------------------------------------------
// Done
// ----------------------------------------------------------------------------
int PathTracer::Done()
{
  CleanStates();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdatePathTraceUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdatePathTraceUniforms()
{
  _PathTraceShader -> Use();

  GLuint PTProgramID = _PathTraceShader -> GetShaderProgramID();

  glUniform2f(glGetUniformLocation(PTProgramID, "u_Resolution"), RenderWidth(), RenderHeight());
  glUniform1i(glGetUniformLocation(PTProgramID, "u_TiledRendering"), ( TiledRendering() && !Dirty() ) ? ( 1 ) : ( 0 ));
  glUniform2f(glGetUniformLocation(PTProgramID, "u_TileOffset"), TileOffset().x, TileOffset().y);
  glUniform2f(glGetUniformLocation(PTProgramID, "u_InvNbTiles"), InvNbTiles().x, InvNbTiles().y);
  glUniform1i(glGetUniformLocation(PTProgramID, "u_NbCompleteFrames"), (int)_NbCompleteFrames);
  glUniform1f(glGetUniformLocation(PTProgramID, "u_Time"), glfwGetTime());
  glUniform1i(glGetUniformLocation(PTProgramID, "u_FrameNum"), _FrameNum);

  if ( !Dirty() && _Settings._Accumulate )
  {
    if ( _AccumulatedFrames >= 1 )
      glUniform1i(glGetUniformLocation(PTProgramID, "u_Accumulate"), 1);
    _AccumulatedFrames++;
  }
  else
  {
    glUniform1i(glGetUniformLocation(PTProgramID, "u_Accumulate"), 0);
    _AccumulatedFrames = 1;
  }

  if ( _DirtyState._RenderSettings )
  {
    glUniform1i(glGetUniformLocation(PTProgramID, "u_Bounces"), _Settings._Bounces);
    glUniform3f(glGetUniformLocation(PTProgramID, "u_BackgroundColor"), _Settings._BackgroundColor.r, _Settings._BackgroundColor.g, _Settings._BackgroundColor.b);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_EnableSkybox"), (int)_Settings._EnableSkybox);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_EnableBackground" ), (int)_Settings._EnableBackGround);
    glUniform1f(glGetUniformLocation(PTProgramID, "u_SkyboxRotation"), _Settings._SkyBoxRotation / 360.f);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_ScreenTexture"), (int)TextureSlot::Accumulate);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_SkyboxTexture"), (int)TextureSlot::EnvMap);
    glUniform1f(glGetUniformLocation(PTProgramID, "u_Gamma"), _Settings._Gamma);
    glUniform1f(glGetUniformLocation(PTProgramID, "u_Exposure"), _Settings._Exposure);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_ToneMapping"), ( _Settings._ToneMapping ? 1 : 0 ));
    glUniform1i(glGetUniformLocation(PTProgramID, "u_DebugMode" ), _DebugMode );
  }

  if ( _DirtyState._SceneCamera )
  {
    Camera & cam = _Scene.GetCamera();
    glUniform3f(glGetUniformLocation(PTProgramID, "u_Camera._Up"), cam.GetUp().x, cam.GetUp().y, cam.GetUp().z);
    glUniform3f(glGetUniformLocation(PTProgramID, "u_Camera._Right"), cam.GetRight().x, cam.GetRight().y, cam.GetRight().z);
    glUniform3f(glGetUniformLocation(PTProgramID, "u_Camera._Forward"), cam.GetForward().x, cam.GetForward().y, cam.GetForward().z);
    glUniform3f(glGetUniformLocation(PTProgramID, "u_Camera._Pos"), cam.GetPos().x, cam.GetPos().y, cam.GetPos().z);
    glUniform1f(glGetUniformLocation(PTProgramID, "u_Camera._FOV"), cam.GetFOV());
  }

  if ( _DirtyState._SceneLights )
  {
    int nbLights = 0;

    for ( int i = 0; i < _Scene.GetNbLights(); ++i )
    {
      Light * curLight = _Scene.GetLight(i);
      if ( !curLight )
        continue;

      glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_Pos"     ).c_str()), curLight -> _Pos.x, curLight -> _Pos.y, curLight -> _Pos.z);
      glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_Emission").c_str()), curLight -> _Emission.r, curLight -> _Emission.g, curLight -> _Emission.b);
      glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_DirU"    ).c_str()), curLight -> _DirU.x, curLight -> _DirU.y, curLight -> _DirU.z);
      glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_DirV"    ).c_str()), curLight -> _DirV.x, curLight -> _DirV.y, curLight -> _DirV.z);
      glUniform1f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_Radius"  ).c_str()), curLight -> _Radius);
      glUniform1f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_Area"    ).c_str()), curLight -> _Area);
      glUniform1f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Lights",i,"_Type"    ).c_str()), curLight -> _Type);

      nbLights++;
      if ( nbLights >= 32 )
        break;
    }

    glUniform1i(glGetUniformLocation(PTProgramID, "u_NbLights"), nbLights);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_ShowLights"), (int)_Settings._ShowLights);
  }

  if ( _DirtyState._SceneMaterials )
  {
    const std::vector<Material> & Materials =  _Scene.GetMaterials();

    glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  if ( _DirtyState._SceneInstances )
  {
    //const std::vector<Mesh*>          & Meshes          = _Scene.GetMeshes();
    const std::vector<Primitive*>        & Primitives         = _Scene.GetPrimitives();
    //const std::vector<MeshInstance>   & MeshInstances   = _Scene.GetMeshInstances();
    const std::vector<PrimitiveInstance> & PrimitiveInstances = _Scene.GetPrimitiveInstances();

    int nbSpheres = 0;
    int nbPlanes = 0;
    int nbBoxes = 0;
    for ( auto & prim : PrimitiveInstances )
    {
      if ( ( prim._PrimID < 0 ) || ( prim._PrimID >= Primitives.size() ) )
        continue;

      Primitive * curPrimitive = Primitives[prim._PrimID];
      if ( !curPrimitive )
        continue;

      if ( curPrimitive -> _Type == PrimitiveType::Sphere )
      {
        Sphere * curSphere = (Sphere *) curPrimitive;
        Vec4 CenterRad = prim._Transform * Vec4(0.f, 0.f, 0.f, 1.f);
        CenterRad.w = curSphere -> _Radius;

        glUniform1i(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Spheres",nbSpheres,"_MaterialID").c_str()), prim._MaterialID);
        glUniform4f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Spheres",nbSpheres,"_CenterRad").c_str()), CenterRad.x, CenterRad.y, CenterRad.z, CenterRad.w);
        nbSpheres++;
      }
      else if ( curPrimitive -> _Type == PrimitiveType::Plane )
      {
        Plane * curPlane = (Plane *) curPrimitive;
        Vec4 orig = prim._Transform * Vec4(curPlane -> _Origin.x, curPlane -> _Origin.y, curPlane -> _Origin.z, 1.f);
        Vec4 normal = glm::transpose(glm::inverse(prim._Transform)) * Vec4(curPlane -> _Normal.x, curPlane -> _Normal.y, curPlane -> _Normal.z, 1.f);

        glUniform1i(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Planes",nbPlanes,"_MaterialID").c_str()), prim._MaterialID);
        glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Planes",nbPlanes,"_Orig").c_str()), orig.x, orig.y, orig.z);
        glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Planes",nbPlanes,"_Normal").c_str()), normal.x, normal.y, normal.z);
        nbPlanes++;
      }
      else if ( curPrimitive -> _Type == PrimitiveType::Box )
      {
        Box * curBox = (Box *) curPrimitive;

        glUniform1i(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_MaterialID").c_str()), prim._MaterialID);
        glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_Low").c_str()), curBox -> _Low.x, curBox -> _Low.y, curBox -> _Low.z);
        glUniform3f(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_High").c_str()), curBox -> _High.x, curBox -> _High.y, curBox -> _High.z);
        glUniformMatrix4fv(glGetUniformLocation(PTProgramID, GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_Transfom").c_str()), 1, GL_FALSE, glm::value_ptr(prim._Transform));
        nbBoxes++;
      }
    }

    glUniform1i(glGetUniformLocation(PTProgramID, "u_VtxTexture"),                    (int)TextureSlot::Vertices);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_VtxNormTexture"),                (int)TextureSlot::Normals);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_VtxUVTexture"),                  (int)TextureSlot::UVs);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_VtxIndTexture"),                 (int)TextureSlot::VertInd);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_TexIndTexture"),                 (int)TextureSlot::TexInd);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_TexArrayTexture"),               (int)TextureSlot::TexArray);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_MeshBBoxTexture"),               (int)TextureSlot::MeshBBox);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_MeshIDRangeTexture"),            (int)TextureSlot::MeshIdRange);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_MaterialsTexture"),              (int)TextureSlot::Materials);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_TLASNodesTexture"),              (int)TextureSlot::TLASNodes);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_TLASTransformsTexture"),         (int)TextureSlot::TLASTransformsID);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_TLASMeshMatIDTexture"),          (int)TextureSlot::TLASMeshMatID);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASNodesTexture"),              (int)TextureSlot::BLASNodes);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASNodesRangeTexture"),         (int)TextureSlot::BLASNodesRange);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASPackedIndicesTexture"),      (int)TextureSlot::BLASPackedIndices);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASPackedIndicesRangeTexture"), (int)TextureSlot::BLASPackedIndicesRange);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASPackedVtxTexture"),          (int)TextureSlot::BLASPackedVertices);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASPackedNormTexture"),         (int)TextureSlot::BLASPackedNormals);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_BLASPackedUVTexture"),           (int)TextureSlot::BLASPackedUVs);

    glUniform1i(glGetUniformLocation(PTProgramID, "u_NbSpheres"), nbSpheres);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_NbPlanes"), nbPlanes);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_NbBoxes"), nbBoxes);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_NbTriangles"), _NbTriangles);
    glUniform1i(glGetUniformLocation(PTProgramID, "u_NbMeshInstances"), _NbMeshInstances);
  }

  _PathTraceShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindPathTraceTextures
// ----------------------------------------------------------------------------
int PathTracer::BindPathTraceTextures()
{
  glActiveTexture(TEX_UNIT(_VtxTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxTBO._ID);
  glActiveTexture(TEX_UNIT(_VtxNormTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxNormTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_VtxUVTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxUVTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_VtxIndTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxIndTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_TexIndTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _TexIndTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_MeshBBoxTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _MeshBBoxTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_MeshIdRangeTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _MeshIdRangeTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_TLASNodesTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _TLASNodesTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_TLASMeshMatIDTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _TLASMeshMatIDTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASNodesTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASNodesTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASNodesRangeTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASNodesRangeTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASPackedIndicesTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedIndicesTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASPackedIndicesRangeTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedIndicesRangeTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASPackedVerticesTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedVerticesTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASPackedNormalsTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedNormalsTBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_BLASPackedUVsTBO._Tex));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedUVsTBO._Tex._ID);

  glActiveTexture(TEX_UNIT(_TexArrayTEX));
  glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTEX._ID);
  glActiveTexture(TEX_UNIT(_MaterialsTEX));
  glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._ID);
  glActiveTexture(TEX_UNIT(_TLASTransformsIDTEX));
  glBindTexture(GL_TEXTURE_2D, _TLASTransformsIDTEX._ID);
  glActiveTexture(TEX_UNIT(_EnvMapTEX));
  glBindTexture(GL_TEXTURE_2D, _EnvMapTEX._ID);

  glActiveTexture(TEX_UNIT(_RenderTargetLowResFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetTileFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);

  return 0;
}

// ----------------------------------------------------------------------------
// BindAccumulateTextures
// ----------------------------------------------------------------------------
int PathTracer::BindAccumulateTextures()
{
  if ( LowResPass() )
  {
    glActiveTexture(TEX_UNIT(_RenderTargetLowResFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID);
  }
  else if ( TiledRendering() )
  {
    glActiveTexture(TEX_UNIT(_RenderTargetTileFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID);
  }
  else
  {
    glActiveTexture(TEX_UNIT(_RenderTargetFBO._Tex));
    glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);
  }

  glActiveTexture(TEX_UNIT(_AccumulateFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _AccumulateFBO._Tex._ID);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateAccumulateUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateAccumulateUniforms()
{
  _AccumulateShader -> Use();

  GLuint AccumProgramID = _AccumulateShader -> GetShaderProgramID();
  if ( LowResPass() )
    glUniform1i(glGetUniformLocation(AccumProgramID, "u_Texture"), (int)TextureSlot::RenderTargetLowRes);
  else if ( TiledRendering() )
    glUniform1i(glGetUniformLocation(AccumProgramID, "u_Texture"), (int)TextureSlot::RenderTargetTile);
  else
    glUniform1i(glGetUniformLocation(AccumProgramID, "u_Texture"), (int)TextureSlot::RenderTarget);

  _AccumulateShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int PathTracer::RenderToTexture()
{
  // Path trace
  if ( LowResPass() )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._ID);
    glViewport(0, 0, LowResRenderWidth(), LowResRenderHeight());
  }
  else if ( TiledRendering() )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._ID);
    glViewport(0, 0, TileWidth(), TileHeight());
  }
  else
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._ID);
    glViewport(0, 0, RenderWidth(), RenderHeight());
  }

  this -> BindPathTraceTextures();

  _Quad.Render(*_PathTraceShader);

  // Accumulate
  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._ID);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  this -> BindAccumulateTextures();

  _Quad.Render(*_AccumulateShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderToScreenUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateRenderToScreenUniforms()
{
  _RenderToScreenShader -> Use();

  GLuint RTSProgramID = _RenderToScreenShader -> GetShaderProgramID();
  glUniform1i(glGetUniformLocation(RTSProgramID, "u_ScreenTexture"), (int)TextureSlot::Accumulate);
  glUniform1i(glGetUniformLocation(RTSProgramID, "u_AccumulatedFrames"), (TiledRendering()) ? (0) :(_AccumulatedFrames));
  glUniform2f(glGetUniformLocation(RTSProgramID, "u_RenderRes" ), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glUniform1f(glGetUniformLocation(RTSProgramID, "u_Gamma"), _Settings._Gamma);
  glUniform1f(glGetUniformLocation(RTSProgramID, "u_Exposure"), _Settings._Exposure);
  glUniform1i(glGetUniformLocation(RTSProgramID, "u_ToneMapping"), 0);
  glUniform1i(glGetUniformLocation(RTSProgramID, "u_FXAA"), (_Settings._FXAA ?  1 : 0 ));

  _RenderToScreenShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int PathTracer::BindRenderToScreenTextures()
{
  glActiveTexture(TEX_UNIT(_AccumulateFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _AccumulateFBO._Tex._ID);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToScreen
// ----------------------------------------------------------------------------
int PathTracer::RenderToScreen()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  this -> BindRenderToScreenTextures();

  _Quad.Render(*_RenderToScreenShader);

  _FrameNum++;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderResolution
// ----------------------------------------------------------------------------
int PathTracer::UpdateRenderResolution()
{
  _Settings._RenderResolution.x = int(_Settings._WindowResolution.x * RenderScale());
  _Settings._RenderResolution.y = int(_Settings._WindowResolution.y * RenderScale());

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeRenderTarget
// ----------------------------------------------------------------------------
int PathTracer::ResizeRenderTarget()
{
  UpdateRenderResolution();

  ResizeFBO(_RenderTargetFBO, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT);
  ResizeFBO(_RenderTargetTileFBO, GL_RGBA32F, TileWidth(), TileHeight(), GL_RGBA, GL_FLOAT);
  ResizeFBO(_RenderTargetLowResFBO, GL_RGBA32F, LowResRenderWidth(), LowResRenderHeight(), GL_RGBA, GL_FLOAT);
  ResizeFBO(_AccumulateFBO, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int PathTracer::InitializeFrameBuffers()
{
  UpdateRenderResolution();

  // Render target textures
  glGenTextures(1, &_RenderTargetFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenTextures(1, &_RenderTargetTileFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetTileFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, TileWidth(), TileHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenTextures(1, &_RenderTargetLowResFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_RenderTargetLowResFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, LowResRenderWidth(), LowResRenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);

  glGenTextures(1, &_AccumulateFBO._Tex._ID);
  glActiveTexture(TEX_UNIT(_AccumulateFBO._Tex));
  glBindTexture(GL_TEXTURE_2D, _AccumulateFBO._Tex._ID);
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

  glGenFramebuffers(1, &_RenderTargetTileFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetTileFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glGenFramebuffers(1, &_RenderTargetLowResFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glGenFramebuffers(1, &_AccumulateFBO._ID);
  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._ID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _AccumulateFBO._Tex._ID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int PathTracer::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_RTRenderer.glsl");

  ShaderProgram * newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _PathTraceShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _AccumulateShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Postpro.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RenderToScreenShader.reset(newShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int PathTracer::UnloadScene()
{
  _NbTriangles = 0;
  _NbMeshInstances = 0;

  DeleteTBO(_VtxTBO);
  DeleteTBO(_VtxNormTBO);
  DeleteTBO(_VtxUVTBO);
  DeleteTBO(_VtxIndTBO);
  DeleteTBO(_TexIndTBO);
  DeleteTBO(_MeshBBoxTBO);
  DeleteTBO(_MeshIdRangeTBO);
  DeleteTBO(_TLASNodesTBO);
  DeleteTBO(_TLASMeshMatIDTBO);
  DeleteTBO(_BLASNodesTBO);
  DeleteTBO(_BLASNodesRangeTBO);
  DeleteTBO(_BLASPackedIndicesTBO);
  DeleteTBO(_BLASPackedIndicesRangeTBO);
  DeleteTBO(_BLASPackedVerticesTBO);
  DeleteTBO(_BLASPackedNormalsTBO);
  DeleteTBO(_BLASPackedUVsTBO);

  DeleteTEX(_TexArrayTEX);
  DeleteTEX(_MaterialsTEX);
  DeleteTEX(_TLASTransformsIDTEX);
  DeleteTEX(_EnvMapTEX);

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadScene
// ----------------------------------------------------------------------------
int PathTracer::ReloadScene()
{
  UnloadScene();

  if ( ( _Settings._TextureSize.x > 0 ) && ( _Settings._TextureSize.y > 0 ) )
    _Scene.CompileMeshData( _Settings._TextureSize, true, true );
  else
    return 1;

  _NbTriangles = _Scene.GetNbFaces();
  _NbMeshInstances = _Scene.GetNbMeshInstances();

  if ( _NbTriangles )
  {
    glPixelStorei(GL_PACK_ALIGNMENT, 1); // ??? Necessary

    InitializeTBO(_VtxTBO, sizeof(Vec3) * _Scene.GetVertices().size(), &_Scene.GetVertices()[0], GL_RGB32F);
    InitializeTBO(_VtxNormTBO, sizeof(Vec3) * _Scene.GetNormals().size(), &_Scene.GetNormals()[0], GL_RGB32F);
    
    if ( _Scene.GetUVMatID().size() )
      InitializeTBO(_VtxUVTBO, sizeof(Vec3) * _Scene.GetUVMatID().size(), &_Scene.GetUVMatID()[0], GL_RGB32F);
    
    InitializeTBO(_VtxIndTBO, sizeof(Vec3i) * _Scene.GetIndices().size(), &_Scene.GetIndices()[0], GL_RGB32I);
    
    if ( _Scene.GetTextureArrayIDs().size() )
    {
      InitializeTBO(_TexIndTBO, sizeof(int) * _Scene.GetTextureArrayIDs().size(), &_Scene.GetTextureArrayIDs()[0], GL_R32I);

      glGenTextures(1, &_TexArrayTEX._ID);
      glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTEX._ID);
      glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, _Settings._TextureSize.x, _Settings._TextureSize.y, _Scene.GetNbCompiledTex(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_Scene.GetTextureArray()[0]);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }

    InitializeTBO(_MeshBBoxTBO, sizeof(Vec3) * _Scene.GetMeshBBoxes().size(), &_Scene.GetMeshBBoxes()[0], GL_RGB32F);
    InitializeTBO(_MeshIdRangeTBO, sizeof(int) * _Scene.GetMeshIdxRange().size(), &_Scene.GetMeshIdxRange()[0], GL_R32I);
    InitializeTBO(_MeshIdRangeTBO, sizeof(int) * _Scene.GetMeshIdxRange().size(), &_Scene.GetMeshIdxRange()[0], GL_R32I);

    glGenTextures(1, &_MaterialsTEX._ID);
    glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // BVH
    InitializeTBO(_TLASNodesTBO, sizeof(GpuBvh::Node) * _Scene.GetTLASNode().size(), &_Scene.GetTLASNode()[0], GL_RGB32F);

    glGenTextures(1, &_TLASTransformsIDTEX._ID);
    glBindTexture(GL_TEXTURE_2D, _TLASTransformsIDTEX._ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Mat4x4) / sizeof(Vec4)) * _Scene.GetTLASPackedTransforms().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetTLASPackedTransforms()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    InitializeTBO(_TLASMeshMatIDTBO, sizeof(Vec2i) * _Scene.GetTLASPackedMeshMatID().size(), &_Scene.GetTLASPackedMeshMatID()[0], GL_RG32I);
    InitializeTBO(_BLASNodesTBO, sizeof(GpuBvh::Node) * _Scene.GetBLASNode().size(), &_Scene.GetBLASNode()[0], GL_RGB32F);
    InitializeTBO(_BLASNodesRangeTBO, sizeof(Vec2i) * _Scene.GetBLASNodeRange().size(), &_Scene.GetBLASNodeRange()[0], GL_RG32I);
    InitializeTBO(_BLASPackedIndicesTBO, sizeof(Vec3i) * _Scene.GetBLASPackedIndices().size(), &_Scene.GetBLASPackedIndices()[0], GL_RGB32I);
    InitializeTBO(_BLASPackedIndicesRangeTBO, sizeof(Vec2i) * _Scene.GetBLASPackedIndicesRange().size(), &_Scene.GetBLASPackedIndicesRange()[0], GL_RG32I);
    InitializeTBO(_BLASPackedVerticesTBO, sizeof(Vec3) * _Scene.GetBLASPackedVertices().size(), &_Scene.GetBLASPackedVertices()[0], GL_RGB32F);
    InitializeTBO(_BLASPackedNormalsTBO, sizeof(Vec3) * _Scene.GetBLASPackedNormals().size(), &_Scene.GetBLASPackedNormals()[0], GL_RGB32F);
    InitializeTBO(_BLASPackedUVsTBO, sizeof(Vec2) * _Scene.GetBLASPackedUVs().size(), &_Scene.GetBLASPackedUVs()[0], GL_RG32F);
  }

  if ( _Scene.GetEnvMap().IsInitialized() )
  {
    glGenTextures(1, &_EnvMapTEX._ID);
    glBindTexture(GL_TEXTURE_2D, _EnvMapTEX._ID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, _Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight(), 0, GL_RGB, GL_FLOAT, _Scene.GetEnvMap().GetRawData());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
  else
    _Settings._EnableSkybox = false;

  return 0;
}

// ----------------------------------------------------------------------------
// NextTile
// ----------------------------------------------------------------------------
void PathTracer::NextTile()
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
void PathTracer::ResetTiles()
{
  _CurTile.x = -1;
  _CurTile.y = NbTiles().y - 1;
  _NbCompleteFrames = 0;
}

}
