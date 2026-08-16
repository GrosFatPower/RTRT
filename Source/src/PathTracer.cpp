#include "PathTracer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "ShaderProgram.h"
#include "GLUtil.h"
#include "PathUtils.h"

#include <string>
#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "stb_image_write.h"


namespace fs = std::filesystem;

namespace RTRT
{

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
  GLUtil::DeleteFBO(_RenderTargetFBO);
  GLUtil::DeleteFBO(_RenderTargetLowResFBO);
  GLUtil::DeleteFBO(_RenderTargetTileFBO);
  GLUtil::DeleteFBO(_AccumulateFBO);
  GLUtil::DeleteFBO(_DenoiseFBO);

  UnloadScene( true );
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

  if ( 0 != InitializeStats() )
  {
    std::cout << "PathTracer : Failed to initialize frame statistics !" << std::endl;
    return 1;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Update
// ----------------------------------------------------------------------------
int PathTracer::Update()
{
  if ( Dirty() )
  {
    this -> ResetTiles();
    _NbCompleteFrames = 0;
  }
  else if ( TiledRendering() )
    this -> NextTile();

  if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
    this -> ResizeRenderTarget();

  if ( _DirtyStates & (unsigned long)DirtyState::SceneInstances )
  {
    if ( 0 != this -> ReloadSceneInstances() )
      return 1;
  }

  if ( _DirtyStates & (unsigned long)DirtyState::SceneEnvMap )
    this -> ReloadEnvMap();

  this -> UpdatePathTraceUniforms();
  this -> UpdateAccumulateUniforms();
  this -> UpdateRenderToScreenUniforms();
  this -> UpdateDenoiserUniforms();

  return 0;
}

// ----------------------------------------------------------------------------
// Done
// ----------------------------------------------------------------------------
int PathTracer::Done()
{
  _FrameNum++;

  if ( !Dirty() && !TiledRendering() )
    _NbCompleteFrames++;

  CleanStates();

  UpdateStats();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateStats
// ----------------------------------------------------------------------------
int PathTracer::InitializeStats()
{
  _PathTraceTime      = 0.;
  _AccumulateTime     = 0.;
  _DenoiseTime        = 0.;
  _RenderToScreenTime = 0.;
  _PathTraceTimerWritten = false;
  _AccumulateTimerWritten = false;
  _DenoiseTimerWritten = false;
  _RenderToScreenTimerWritten = false;

  glGenQueries( 1, &_PathTraceTimeId[0] );
  glGenQueries( 1, &_PathTraceTimeId[1] );
  glGenQueries( 1, &_AccumulateTimeId[0] );
  glGenQueries( 1, &_AccumulateTimeId[1] );
  glGenQueries( 1, &_DenoiseTimeId[0] );
  glGenQueries( 1, &_DenoiseTimeId[1] );
  glGenQueries( 1, &_RenderToScreenTimeId[0] );
  glGenQueries( 1, &_RenderToScreenTimeId[1] );

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateStats
// ----------------------------------------------------------------------------
int PathTracer::UpdateStats()
{
  if ( _PathTraceTimerWritten )
  {
    _PathTraceTime = ReadTimer(_PathTraceTimeId);
    _PathTraceTimerWritten = false;
  }
  else
    _PathTraceTime = 0.;

  if ( _AccumulateTimerWritten )
  {
    _AccumulateTime = ReadTimer(_AccumulateTimeId);
    _AccumulateTimerWritten = false;
  }
  else
    _AccumulateTime = 0.;

  if ( _DenoiseTimerWritten )
  {
    _DenoiseTime = ReadTimer(_DenoiseTimeId);
    _DenoiseTimerWritten = false;
  }
  else
    _DenoiseTime = 0.;

  if ( _RenderToScreenTimerWritten )
  {
    _RenderToScreenTime = ReadTimer(_RenderToScreenTimeId);
    _RenderToScreenTimerWritten = false;
  }
  else
    _RenderToScreenTime = 0.;

  return 0;
}

// ----------------------------------------------------------------------------
// GetRenderPassTimings
// ----------------------------------------------------------------------------
int PathTracer::GetRenderPassTimings( std::vector<RenderPassTiming> & oTimings ) const
{
  oTimings.clear();
  oTimings.push_back({ "Path trace", _PathTraceTime, true, true });
  oTimings.push_back({ "Accumulate", _AccumulateTime, true, true });
  oTimings.push_back({ "Denoise", _DenoiseTime, true, _DenoisedThisFrame });
  oTimings.push_back({ "Composite / screen", _RenderToScreenTime, true, true });
  return 0;
}

// ----------------------------------------------------------------------------
// BeginTimer
// ----------------------------------------------------------------------------
void PathTracer::BeginTimer( GLuint iTimerId[2] )
{
#if defined(__APPLE__)
  glBeginQuery(GL_TIME_ELAPSED, iTimerId[0]);
#else
  glQueryCounter(iTimerId[0], GL_TIMESTAMP);
#endif
}

// ----------------------------------------------------------------------------
// EndTimer
// ----------------------------------------------------------------------------
void PathTracer::EndTimer( GLuint iTimerId[2] )
{
#if defined(__APPLE__)
  glEndQuery(GL_TIME_ELAPSED);
#else
  glQueryCounter(iTimerId[1], GL_TIMESTAMP);
#endif
}

// ----------------------------------------------------------------------------
// ReadTimer
// ----------------------------------------------------------------------------
double PathTracer::ReadTimer( GLuint iTimerId[2] )
{
  GLuint64 startTime = 0, endTime = 0, executionTime = 0;
  GLint resultAvailable = 0;

#if defined(__APPLE__)
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( iTimerId[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( iTimerId[0], GL_QUERY_RESULT, &executionTime );
#else
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( iTimerId[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( iTimerId[0], GL_QUERY_RESULT, &startTime );

  resultAvailable = 0;
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( iTimerId[1], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( iTimerId[1], GL_QUERY_RESULT, &endTime );

  executionTime = endTime - startTime;
#endif

  return (double)executionTime / 1000000000.;
}

// ----------------------------------------------------------------------------
// UpdatePathTraceUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdatePathTraceUniforms()
{
  _PathTraceShader -> Use();

  _PathTraceShader -> SetUniform("u_Resolution", (float)RenderWidth(), (float)RenderHeight());
  _PathTraceShader -> SetUniform("u_TiledRendering", ( TiledRendering() && !Dirty() ) ? ( 1 ) : ( 0 ));
  _PathTraceShader -> SetUniform("u_TileOffset", TileOffset());
  _PathTraceShader -> SetUniform("u_InvNbTiles", InvNbTiles());
  _PathTraceShader -> SetUniform("u_Time", (float)glfwGetTime());
  _PathTraceShader -> SetUniform("u_FrameNum", (int)_FrameNum);
  _PathTraceShader -> SetUniform("u_NbCompleteFrames", (int)_NbCompleteFrames);

  if ( _DirtyStates & (unsigned long)DirtyState::RenderSettings )
  {
    _PathTraceShader -> SetUniform("u_NbBounces", _Settings._Bounces);
    _PathTraceShader -> SetUniform("u_NbSamplesPerPixel", _Settings._NbSamplesPerPixel);
    _PathTraceShader -> SetUniform("u_RussianRoulette", (int)_Settings._RussianRoulette);
    _PathTraceShader -> SetUniform("u_BackgroundColor", _Settings._BackgroundColor);
    _PathTraceShader -> SetUniform("u_EnableEnvMap", (int)_Settings._EnableSkybox);
    _PathTraceShader -> SetUniform("u_EnableBackground" , (int)_Settings._EnableBackGround);
    _PathTraceShader -> SetUniform("u_EnvMapRotation", _Settings._SkyBoxRotation / 360.f);
    _PathTraceShader -> SetUniform("u_EnvMapRes", (float)_Scene.GetEnvMap().GetWidth(), (float)_Scene.GetEnvMap().GetHeight());
    _PathTraceShader -> SetUniform("u_EnvMap", (int)PathTracerTexSlot::_EnvMap);
    _PathTraceShader -> SetUniform("u_EnvMapCDF", (int)PathTracerTexSlot::_EnvMapCDF);
    _PathTraceShader -> SetUniform("u_EnvMapTotalWeight", _Scene.GetEnvMap().GetTotalWeight());
    _PathTraceShader -> SetUniform("u_Gamma", _Settings._Gamma);
    _PathTraceShader -> SetUniform("u_Exposure", _Settings._Exposure);
    _PathTraceShader -> SetUniform("u_ToneMapping", ( _Settings._ToneMapping ? 1 : 0 ));
    //_PathTraceShader -> SetUniform("u_ToneMapping", 0);
    _PathTraceShader -> SetUniform("u_DebugMode" , _DebugMode );
  }

  if ( _DirtyStates & (unsigned long)DirtyState::SceneCamera )
  {
    Camera & cam = _Scene.GetCamera();
    _PathTraceShader -> SetUniform("u_Camera._Up", cam.GetUp());
    _PathTraceShader -> SetUniform("u_Camera._Right", cam.GetRight());
    _PathTraceShader -> SetUniform("u_Camera._Forward", cam.GetForward());
    _PathTraceShader -> SetUniform("u_Camera._Pos", cam.GetPos());
    _PathTraceShader -> SetUniform("u_Camera._FOV", cam.GetFOV());
    _PathTraceShader -> SetUniform("u_Camera._FocalDist", cam.GetFocalDist());
    _PathTraceShader -> SetUniform("u_Camera._LensRadius", cam.GetAperture() * .5f);
  }

  if ( _DirtyStates & (unsigned long)DirtyState::SceneLights )
  {
    int nbLights = 0;

    for ( int i = 0; i < _Scene.GetNbLights(); ++i )
    {
      Light * curLight = _Scene.GetLight(i);
      if ( !curLight )
        continue;

      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Pos"     ), curLight -> _Pos);
      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Emission"), curLight -> _Emission * curLight -> _Intensity);
      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_DirU"    ), curLight -> _DirU);
      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_DirV"    ), curLight -> _DirV);
      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Radius"  ), curLight -> _Radius);
      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Area"    ), curLight -> _Area);
      _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Lights",i,"_Type"    ), curLight -> _Type);

      nbLights++;
      if ( nbLights >= 32 )
        break;
    }

    _PathTraceShader -> SetUniform("u_NbLights", nbLights);
    _PathTraceShader -> SetUniform("u_ShowLights", (int)_Settings._ShowLights);
  }

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

  if ( _DirtyStates & (unsigned long)DirtyState::SceneInstances )
  {
    const std::vector<Primitive*>        & Primitives         = _Scene.GetPrimitives();
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

        _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Spheres",nbSpheres,"_MaterialID"), prim._MaterialID);
        _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Spheres",nbSpheres,"_CenterRad"), CenterRad);
        nbSpheres++;
      }
      else if ( curPrimitive -> _Type == PrimitiveType::Plane )
      {
        Plane * curPlane = (Plane *) curPrimitive;
        Vec4 orig = prim._Transform * Vec4(curPlane -> _Origin.x, curPlane -> _Origin.y, curPlane -> _Origin.z, 1.f);
        Vec4 normal = glm::transpose(glm::inverse(prim._Transform)) * Vec4(curPlane -> _Normal.x, curPlane -> _Normal.y, curPlane -> _Normal.z, 1.f);

       _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Planes",nbPlanes,"_MaterialID"), prim._MaterialID);
       _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Planes",nbPlanes,"_Orig"), orig.x, orig.y, orig.z);
       _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Planes",nbPlanes,"_Normal"), normal.x, normal.y, normal.z);
        nbPlanes++;
      }
      else if ( curPrimitive -> _Type == PrimitiveType::Box )
      {
        Box * curBox = (Box *) curPrimitive;

        _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_MaterialID"), prim._MaterialID);
        _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_Low"), curBox -> _Low);
        _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_High"), curBox -> _High);
        _PathTraceShader -> SetUniform(GLUtil::UniformArrayElementName("u_Boxes",nbBoxes,"_Transfom"), prim._Transform);
        nbBoxes++;
      }
    }

    _PathTraceShader -> SetUniform("u_VtxTexture",                    (int)PathTracerTexSlot::_Vertices);
    _PathTraceShader -> SetUniform("u_VtxNormTexture",                (int)PathTracerTexSlot::_Normals);
    _PathTraceShader -> SetUniform("u_VtxUVTexture",                  (int)PathTracerTexSlot::_UVs);
    _PathTraceShader -> SetUniform("u_VtxIndTexture",                 (int)PathTracerTexSlot::_VertInd);
    _PathTraceShader -> SetUniform("u_TexIndTexture",                 (int)PathTracerTexSlot::_TexInd);
    _PathTraceShader -> SetUniform("u_TexArrayTexture",               (int)PathTracerTexSlot::_TexArray);
    _PathTraceShader -> SetUniform("u_MeshBBoxTexture",               (int)PathTracerTexSlot::_MeshBBox);
    _PathTraceShader -> SetUniform("u_MeshIDRangeTexture",            (int)PathTracerTexSlot::_MeshIdRange);
    _PathTraceShader -> SetUniform("u_MaterialsTexture",              (int)PathTracerTexSlot::_Materials);
    _PathTraceShader -> SetUniform("u_TLASNodesTexture",              (int)PathTracerTexSlot::_TLASNodes);
    _PathTraceShader -> SetUniform("u_TLASTransformsTexture",         (int)PathTracerTexSlot::_TLASTransformsID);
    _PathTraceShader -> SetUniform("u_TLASMeshMatIDTexture",          (int)PathTracerTexSlot::_TLASMeshMatID);
    _PathTraceShader -> SetUniform("u_BLASNodesTexture",              (int)PathTracerTexSlot::_BLASNodes);
    _PathTraceShader -> SetUniform("u_BLASNodesRangeTexture",         (int)PathTracerTexSlot::_BLASNodesRange);
    _PathTraceShader -> SetUniform("u_BLASPackedIndicesTexture",      (int)PathTracerTexSlot::_BLASPackedIndices);
    _PathTraceShader -> SetUniform("u_BLASPackedIndicesRangeTexture", (int)PathTracerTexSlot::_BLASPackedIndicesRange);
    _PathTraceShader -> SetUniform("u_BLASPackedVtxTexture",          (int)PathTracerTexSlot::_BLASPackedVertices);
    _PathTraceShader -> SetUniform("u_BLASPackedNormTexture",         (int)PathTracerTexSlot::_BLASPackedNormals);
    _PathTraceShader -> SetUniform("u_BLASPackedUVTexture",           (int)PathTracerTexSlot::_BLASPackedUVs);

    _PathTraceShader -> SetUniform("u_NbSpheres", nbSpheres);
    _PathTraceShader -> SetUniform("u_NbPlanes", nbPlanes);
    _PathTraceShader -> SetUniform("u_NbBoxes", nbBoxes);
    _PathTraceShader -> SetUniform("u_NbTriangles", _NbTriangles);
    _PathTraceShader -> SetUniform("u_NbMeshInstances", _NbMeshInstances);
  }

  _PathTraceShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindPathTraceTextures
// ----------------------------------------------------------------------------
int PathTracer::BindPathTraceTextures()
{
  GLUtil::ActivateTexture(_VtxTBO._Tex);
  GLUtil::ActivateTexture(_VtxNormTBO._Tex);
  GLUtil::ActivateTexture(_VtxUVTBO._Tex);
  GLUtil::ActivateTexture(_VtxIndTBO._Tex);
  GLUtil::ActivateTexture(_TexIndTBO._Tex);
  GLUtil::ActivateTexture(_MeshBBoxTBO._Tex);
  GLUtil::ActivateTexture(_MeshIdRangeTBO._Tex);
  GLUtil::ActivateTexture(_TLASNodesTBO._Tex);
  GLUtil::ActivateTexture(_TLASMeshMatIDTBO._Tex);
  GLUtil::ActivateTexture(_BLASNodesTBO._Tex);
  GLUtil::ActivateTexture(_BLASNodesRangeTBO._Tex);
  GLUtil::ActivateTexture(_BLASPackedIndicesTBO._Tex);
  GLUtil::ActivateTexture(_BLASPackedIndicesRangeTBO._Tex);
  GLUtil::ActivateTexture(_BLASPackedVerticesTBO._Tex);
  GLUtil::ActivateTexture(_BLASPackedNormalsTBO._Tex);
  GLUtil::ActivateTexture(_BLASPackedUVsTBO._Tex);

  GLUtil::ActivateTexture(_TexArrayTEX);

  GLUtil::ActivateTexture(_MaterialsTEX);
  GLUtil::ActivateTexture(_TLASTransformsIDTEX);
  GLUtil::ActivateTexture(_EnvMapTEX);
  GLUtil::ActivateTexture(_EnvMapCDFTEX);

  GLUtil::ActivateTextures(_RenderTargetLowResFBO);
  GLUtil::ActivateTextures(_RenderTargetTileFBO);
  GLUtil::ActivateTextures(_RenderTargetFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// BindAccumulateTextures
// ----------------------------------------------------------------------------
int PathTracer::BindAccumulateTextures()
{
  if ( LowResPass() )
  {
    GLUtil::ActivateTextures(_RenderTargetLowResFBO);
  }
  else if ( TiledRendering() )
  {
    GLUtil::ActivateTextures(_RenderTargetTileFBO);
  }
  else
  {
    GLUtil::ActivateTextures(_RenderTargetFBO);
  }

  GLUtil::ActivateTextures(_AccumulateFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateAccumulateUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateAccumulateUniforms()
{
  _AccumulateShader -> Use();

  _AccumulateShader -> SetUniform("u_PreviousFrame", (int)PathTracerTexSlot::_Accumulate);

  if ( LowResPass() )
    _AccumulateShader -> SetUniform("u_NewFrame", (int)PathTracerTexSlot::_RenderTargetLowRes);
  else if ( TiledRendering() )
    _AccumulateShader -> SetUniform("u_NewFrame", (int)PathTracerTexSlot::_RenderTargetTile);
  else
    _AccumulateShader -> SetUniform("u_NewFrame", (int)PathTracerTexSlot::_RenderTarget);

  _AccumulateShader -> SetUniform("u_NewFrameNormals", (int)PathTracerTexSlot::_RenderTargetNormals);
  _AccumulateShader -> SetUniform("u_NewFramePos", (int)PathTracerTexSlot::_RenderTargetPos);

  if ( !Dirty() && _Settings._Accumulate && ( _NbCompleteFrames > 0 ) )
    _AccumulateShader -> SetUniform("u_Accumulate", 1);
  else
    _AccumulateShader -> SetUniform("u_Accumulate", 0);

  _AccumulateShader -> SetUniform("u_NbCompleteFrames", (int)_NbCompleteFrames);

  _AccumulateShader -> SetUniform("u_TiledRendering", ( TiledRendering() && !Dirty() ) ? ( 1 ) : ( 0 ));
  _AccumulateShader -> SetUniform("u_TileOffset", TileOffset());
  _AccumulateShader -> SetUniform("u_InvNbTiles", InvNbTiles());

  _AccumulateShader -> SetUniform("u_DebugMode" , _DebugMode );

  _AccumulateShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
int PathTracer::RenderToTexture()
{
  _DenoisedThisFrame = false;

  // Path trace
  BeginTimer(_PathTraceTimeId);

  if ( LowResPass() )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._Handle);
    glViewport(0, 0, LowResRenderWidth(), LowResRenderHeight());
  }
  else if ( TiledRendering() )
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._Handle);
    glViewport(0, 0, TileWidth(), TileHeight());
  }
  else
  {
    glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._Handle);
    glViewport(0, 0, RenderWidth(), RenderHeight());
  }

  this -> BindPathTraceTextures();

  _Quad.Render(*_PathTraceShader);

  EndTimer(_PathTraceTimeId);
  _PathTraceTimerWritten = true;

  // Accumulate
  BeginTimer(_AccumulateTimeId);

  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._Handle);
  if ( TiledRendering() && !LowResPass() )
    glViewport(_Settings._TileResolution.x * _CurTile.x, _Settings._TileResolution.y * _CurTile.y, _Settings._TileResolution.x, _Settings._TileResolution.y);
  else
    glViewport(0, 0, RenderWidth(), RenderHeight());

  this -> BindAccumulateTextures();

  _Quad.Render(*_AccumulateShader);

  EndTimer(_AccumulateTimeId);
  _AccumulateTimerWritten = true;

  // Denoise
  if ( Denoise() )
    _DenoisedThisFrame = ( 0 == this -> DenoiseOutput() );

  return 0;
}

// ----------------------------------------------------------------------------
// DenoiseOutput
// ----------------------------------------------------------------------------
int PathTracer::DenoiseOutput()
{
  if ( !_DenoiserShader )
    return 1;

  BeginTimer(_DenoiseTimeId);

  _DenoiserShader -> Use();

#if defined(_WIN32) || defined(_WIN64)
  this -> BindDenoiserImageTextures();

  // Dispatch compute shader (assuming texture size is 512x512)
  const int workGroupSizeX = 16, workGroupSizeY = 16;
  const int nbGroupsX = static_cast<int>(std::ceil(((float)RenderWidth()) / workGroupSizeX));
  const int nbGroupsY = static_cast<int>(std::ceil(((float)RenderHeight())/workGroupSizeY));
  glDispatchCompute(nbGroupsX, nbGroupsY, 1);

  // Ensure GPU has completed work before continuing
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
#else
  this -> BindDenoiserTextures();

  glBindFramebuffer(GL_FRAMEBUFFER, _DenoiseFBO._Handle);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  _Quad.Render(*_DenoiserShader);
#endif

  _DenoiserShader -> StopUsing();

  EndTimer(_DenoiseTimeId);
  _DenoiseTimerWritten = true;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateDenoiserUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateDenoiserUniforms()
{
  if ( !_DenoiserShader )
    return 1;

  _DenoiserShader -> Use();

  _DenoiserShader -> SetUniform("u_DenoisingMethod", (int)_Settings._DenoisingMethod);    // 0: Bilateral, 1: Wavelet, 2: Edge-aware
  _DenoiserShader -> SetUniform("u_SigmaSpatial", _Settings._DenoiserSigmaSpatial);       // Bilateral
  _DenoiserShader -> SetUniform("u_SigmaRange", _Settings._DenoiserSigmaRange);           // Bilateral
  _DenoiserShader -> SetUniform("u_Threshold", _Settings._DenoiserThreshold);             // Wavelet
  _DenoiserShader -> SetUniform("u_WaveletScale", (int)_Settings._DenoisingWaveletScale); // Wavelet
  _DenoiserShader -> SetUniform("u_ColorPhi", _Settings._DenoiserColorPhi);               // Edge-aware
  _DenoiserShader -> SetUniform("u_NormalPhi", _Settings._DenoiserNormalPhi);             // Edge-aware
  _DenoiserShader -> SetUniform("u_PositionPhi", _Settings._DenoiserPositionPhi);         // Edge-aware
#if !defined(_WIN32) && !defined(_WIN64)
  _DenoiserShader -> SetUniform("u_InputImage", (int)PathTracerTexSlot::_Accumulate);
  _DenoiserShader -> SetUniform("u_InputNormals", (int)PathTracerTexSlot::_AccumulateNormals);
  _DenoiserShader -> SetUniform("u_InputPos", (int)PathTracerTexSlot::_AccumulatePos);
  _DenoiserShader -> SetUniform("u_ImageSize", RenderWidth(), RenderHeight());
#endif

  _DenoiserShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindDenoiserTextures
// ----------------------------------------------------------------------------
int PathTracer::BindDenoiserTextures()
{
  GLUtil::ActivateTextures(_AccumulateFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// BindDenoiserImageTextures
// ----------------------------------------------------------------------------
int PathTracer::BindDenoiserImageTextures()
{
  if ( 3 == _AccumulateFBO._Tex.size() )
  {
    glBindImageTexture(0, _AccumulateTEX[0]._Handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, _AccumulateTEX[1]._Handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, _AccumulateTEX[2]._Handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
  }
  glBindImageTexture(3, _DenoisedTEX._Handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateRenderToScreenUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateRenderToScreenUniforms()
{
  _RenderToScreenShader -> Use();

  if ( Denoise() )
    _RenderToScreenShader -> SetUniform("u_ScreenTexture", (int)PathTracerTexSlot::_Denoised);
  else
    _RenderToScreenShader -> SetUniform("u_ScreenTexture", (int)PathTracerTexSlot::_Accumulate);
  _RenderToScreenShader -> SetUniform("u_RenderRes", (float)_Settings._WindowResolution.x, (float)_Settings._WindowResolution.y);
  _RenderToScreenShader -> SetUniform("u_Gamma", _Settings._Gamma);
  _RenderToScreenShader -> SetUniform("u_Exposure", _Settings._Exposure);
  //_RenderToScreenShader -> SetUniform("u_ToneMapping", ( _Settings._ToneMapping ? 1 : 0 ));
  _RenderToScreenShader -> SetUniform("u_ToneMapping", 0);
  _RenderToScreenShader -> SetUniform("u_FXAA", (_Settings._FXAA ?  1 : 0 ));

  _RenderToScreenShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindRenderToScreenTextures
// ----------------------------------------------------------------------------
int PathTracer::BindRenderToScreenTextures()
{
  if ( Denoise() )
    GLUtil::ActivateTexture(_DenoisedTEX);
  else
    GLUtil::ActivateTextures(_AccumulateFBO);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToScreen
// ----------------------------------------------------------------------------
int PathTracer::RenderToScreen()
{
  BeginTimer(_RenderToScreenTimeId);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  this -> BindRenderToScreenTextures();

  _Quad.Render(*_RenderToScreenShader);

  EndTimer(_RenderToScreenTimeId);
  _RenderToScreenTimerWritten = true;

  return 0;
}

// ----------------------------------------------------------------------------
// ReadbackFinalColor
// ----------------------------------------------------------------------------
int PathTracer::ReadbackFinalColor( RenderImage & oImage )
{
  const GLTexture & finalTexture = Denoise() ? _DenoisedTEX : _AccumulateTEX[0];
  if ( !finalTexture._Handle || ( RenderWidth() <= 0 ) || ( RenderHeight() <= 0 ) )
    return 1;

  oImage._Width = RenderWidth();
  oImage._Height = RenderHeight();
  oImage._Pixels.resize((size_t)oImage._Width * (size_t)oImage._Height * 4u);

  while ( GL_NO_ERROR != glGetError() ) {}
  glBindTexture(GL_TEXTURE_2D, finalTexture._Handle);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, oImage._Pixels.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  FlipImageVertically(oImage);

  return ( GL_NO_ERROR == glGetError() ) ? 0 : 1;
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
int PathTracer::RenderToFile( const fs::path & iFilePath )
{
  GLFrameBuffer temporaryFBO;
  GLTexture temporaryTEX = { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Temporary };

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
  if ( !GLUtil::CreateFrameBuffer(tempFBODesc, temporaryFBO) )
  {
    GLUtil::DeleteTEX(temporaryTEX);
    return 1;
  }

  // Render to texture
  {
    glBindFramebuffer(GL_FRAMEBUFFER, temporaryFBO._Handle);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

    GLUtil::ActivateTextures(temporaryFBO);
    this -> BindRenderToScreenTextures();

    _Quad.Render(*_RenderToScreenShader);
  }

  // Retrieve image et save to file
  int saved = 0;
  {
    int w = _Settings._WindowResolution.x;
    int h = _Settings._WindowResolution.y;
    unsigned char * frameData = new unsigned char[w * h * 4];

    GLUtil::ActivateTextures(temporaryFBO);
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
  GLUtil::DeleteTEX(temporaryTEX);

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

  GLUtil::ResizeFBO(_RenderTargetFBO,  RenderWidth(), RenderHeight());
  GLUtil::ResizeFBO(_RenderTargetTileFBO, TileWidth(), TileHeight());
  GLUtil::ResizeFBO(_RenderTargetLowResFBO, LowResRenderWidth(), LowResRenderHeight());
  GLUtil::ResizeFBO(_AccumulateFBO, RenderWidth(), RenderHeight());

  GLUtil::ResizeTexture(_DenoisedTEX, RenderWidth(), RenderHeight());
  glBindFramebuffer(GL_FRAMEBUFFER, _DenoiseFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _DenoisedTEX._Handle, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int PathTracer::InitializeFrameBuffers()
{
  UpdateRenderResolution();

  auto createRenderTexture = []( GLTexture & ioTex, int iWidth, int iHeight )
  {
    GLTextureDesc desc;
    desc._Target         = ioTex._Target;
    desc._Slot           = ioTex._Slot;
    desc._Width          = iWidth;
    desc._Height         = iHeight;
    desc._InternalFormat = ioTex._InternalFormat;
    desc._DataFormat     = ioTex._DataFormat;
    desc._DataType       = ioTex._DataType;
    desc._MinFilter      = GL_LINEAR;
    desc._MagFilter      = GL_LINEAR;
    GLUtil::CreateTexture(desc, ioTex);
  };

  auto createTripleFBO = []( GLTexture ioTex[3], GLFrameBuffer & ioFBO )
  {
    GLFrameBufferDesc desc;
    desc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &ioTex[0] });
    desc._Attachments.push_back({ GL_COLOR_ATTACHMENT1, &ioTex[1] });
    desc._Attachments.push_back({ GL_COLOR_ATTACHMENT2, &ioTex[2] });
    return GLUtil::CreateFrameBuffer(desc, ioFBO);
  };

  for ( int i = 0; i < 3; ++i )
    createRenderTexture(_RenderTargetTEX[i], RenderWidth(), RenderHeight());
  if ( !createTripleFBO(_RenderTargetTEX, _RenderTargetFBO) )
    return 1;

  for ( int i = 0; i < 3; ++i )
    createRenderTexture(_RenderTargetTileTEX[i], TileWidth(), TileHeight());
  if ( !createTripleFBO(_RenderTargetTileTEX, _RenderTargetTileFBO) )
    return 1;

  createRenderTexture(_RenderTargetLowResTEX, LowResRenderWidth(), LowResRenderHeight());
  GLFrameBufferDesc lowResFBODesc;
  lowResFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_RenderTargetLowResTEX });
  if ( !GLUtil::CreateFrameBuffer(lowResFBODesc, _RenderTargetLowResFBO) )
    return 1;

  for ( int i = 0; i < 3; ++i )
    createRenderTexture(_AccumulateTEX[i], RenderWidth(), RenderHeight());
  if ( !createTripleFBO(_AccumulateTEX, _AccumulateFBO) )
    return 1;

  GLTextureDesc denoisedDesc;
  denoisedDesc._Target         = _DenoisedTEX._Target;
  denoisedDesc._Slot           = _DenoisedTEX._Slot;
  denoisedDesc._Width          = RenderWidth();
  denoisedDesc._Height         = RenderHeight();
  denoisedDesc._InternalFormat = _DenoisedTEX._InternalFormat;
  denoisedDesc._DataFormat     = _DenoisedTEX._DataFormat;
  denoisedDesc._DataType       = _DenoisedTEX._DataType;
  denoisedDesc._MinFilter      = GL_LINEAR;
  denoisedDesc._MagFilter      = GL_LINEAR;
  denoisedDesc._WrapS          = GL_CLAMP_TO_EDGE;
  denoisedDesc._WrapT          = GL_CLAMP_TO_EDGE;
  GLUtil::CreateTexture(denoisedDesc, _DenoisedTEX);

  GLFrameBufferDesc denoiseFBODesc;
  denoiseFBODesc._Attachments.push_back({ GL_COLOR_ATTACHMENT0, &_DenoisedTEX, GL_TEXTURE_2D, 0, false });
  if ( !GLUtil::CreateFrameBuffer(denoiseFBODesc, _DenoiseFBO) )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int PathTracer::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("vertex_Default.glsl"));
  ShaderSource fragmentShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("fragment_PathTracer.glsl"));

  ShaderProgram * newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _PathTraceShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("fragment_Accumulate.glsl"));
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _AccumulateShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("fragment_PostProcess.glsl"));
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RenderToScreenShader.reset(newShader);

#if defined(_WIN32) || defined(_WIN64)
  ShaderSource computeShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("compute_Denoiser.glsl"));
  newShader = ShaderProgram::LoadShaders(computeShaderSrc);
  if ( !newShader )
    return 1;
  _DenoiserShader.reset(newShader);
#else
  fragmentShaderSrc = Shader::LoadShader(PathUtils::GetShaderPath("fragment_DenoiserPathTracer.glsl"));
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _DenoiserShader.reset(newShader);
#endif

  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int PathTracer::UnloadScene( bool iDeleteOutputTextures )
{
  _NbTriangles = 0;
  _NbMeshInstances = 0;

  GLUtil::DeleteTBO(_VtxTBO);
  GLUtil::DeleteTBO(_VtxNormTBO);
  GLUtil::DeleteTBO(_VtxUVTBO);
  GLUtil::DeleteTBO(_VtxIndTBO);
  GLUtil::DeleteTBO(_TexIndTBO);
  GLUtil::DeleteTBO(_MeshBBoxTBO);
  GLUtil::DeleteTBO(_MeshIdRangeTBO);
  GLUtil::DeleteTBO(_TLASNodesTBO);
  GLUtil::DeleteTBO(_TLASMeshMatIDTBO);
  GLUtil::DeleteTBO(_BLASNodesTBO);
  GLUtil::DeleteTBO(_BLASNodesRangeTBO);
  GLUtil::DeleteTBO(_BLASPackedIndicesTBO);
  GLUtil::DeleteTBO(_BLASPackedIndicesRangeTBO);
  GLUtil::DeleteTBO(_BLASPackedVerticesTBO);
  GLUtil::DeleteTBO(_BLASPackedNormalsTBO);
  GLUtil::DeleteTBO(_BLASPackedUVsTBO);

  GLUtil::DeleteTEX(_TexArrayTEX);
  GLUtil::DeleteTEX(_MaterialsTEX);
  GLUtil::DeleteTEX(_TLASTransformsIDTEX);

  if ( iDeleteOutputTextures )
  {
    for ( int i = 0; i < 3; ++i )
    {
      GLUtil::DeleteTEX(_RenderTargetTEX[i]);
      GLUtil::DeleteTEX(_RenderTargetTileTEX[i]);
      GLUtil::DeleteTEX(_AccumulateTEX[i]);
    }
    GLUtil::DeleteTEX(_RenderTargetLowResTEX);
    GLUtil::DeleteTEX(_DenoisedTEX);
    GLUtil::DeleteTEX(_EnvMapTEX);
    GLUtil::DeleteTEX(_EnvMapCDFTEX);
  }

  _FrameNum = 0;

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
  _NbMeshInstances = static_cast<int>(_Scene.GetTLASPackedMeshMatID().size());

  if ( _NbTriangles )
  {
    glPixelStorei(GL_PACK_ALIGNMENT, 1); // ??? Necessary

    GLUtil::InitializeTBO(_VtxTBO, sizeof(Vec3) * _Scene.GetVertices().size(), &_Scene.GetVertices()[0], GL_RGB32F);
    GLUtil::InitializeTBO(_VtxNormTBO, sizeof(Vec3) * _Scene.GetNormals().size(), &_Scene.GetNormals()[0], GL_RGB32F);
    
    if ( _Scene.GetUVMatID().size() )
      GLUtil::InitializeTBO(_VtxUVTBO, sizeof(Vec3) * _Scene.GetUVMatID().size(), &_Scene.GetUVMatID()[0], GL_RGB32F);
    
    GLUtil::InitializeTBO(_VtxIndTBO, sizeof(Vec3i) * _Scene.GetIndices().size(), &_Scene.GetIndices()[0], GL_RGB32I);
    
    if ( _Scene.GetTextureArrayIDs().size() )
    {
      GLUtil::InitializeTBO(_TexIndTBO, sizeof(int) * _Scene.GetTextureArrayIDs().size(), &_Scene.GetTextureArrayIDs()[0], GL_R32I);

      GLTextureDesc texArrayDesc;
      texArrayDesc._Target         = _TexArrayTEX._Target;
      texArrayDesc._Slot           = _TexArrayTEX._Slot;
      texArrayDesc._Width          = _Settings._TextureSize.x;
      texArrayDesc._Height         = _Settings._TextureSize.y;
      texArrayDesc._Depth          = _Scene.GetNbCompiledTex();
      texArrayDesc._InternalFormat = _TexArrayTEX._InternalFormat;
      texArrayDesc._DataFormat     = _TexArrayTEX._DataFormat;
      texArrayDesc._DataType       = _TexArrayTEX._DataType;
      texArrayDesc._Data           = &_Scene.GetTextureArray()[0];
      texArrayDesc._MinFilter      = GL_LINEAR;
      texArrayDesc._MagFilter      = GL_LINEAR;
      GLUtil::CreateTexture(texArrayDesc, _TexArrayTEX);
    }

    GLUtil::InitializeTBO(_MeshBBoxTBO, sizeof(Vec3) * _Scene.GetMeshBBoxes().size(), &_Scene.GetMeshBBoxes()[0], GL_RGB32F);
    GLUtil::InitializeTBO(_MeshIdRangeTBO, sizeof(int) * _Scene.GetMeshIdxRange().size(), &_Scene.GetMeshIdxRange()[0], GL_R32I);
    GLUtil::InitializeTBO(_MeshIdRangeTBO, sizeof(int) * _Scene.GetMeshIdxRange().size(), &_Scene.GetMeshIdxRange()[0], GL_R32I);

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

    // BVH
    if ( 0 != this -> UploadTLASData() )
      return 1;

    GLUtil::InitializeTBO(_BLASNodesTBO, sizeof(GpuBvh::Node) * _Scene.GetBLASNode().size(), &_Scene.GetBLASNode()[0], GL_RGB32F);
    GLUtil::InitializeTBO(_BLASNodesRangeTBO, sizeof(Vec2i) * _Scene.GetBLASNodeRange().size(), &_Scene.GetBLASNodeRange()[0], GL_RG32I);
    GLUtil::InitializeTBO(_BLASPackedIndicesTBO, sizeof(Vec3i) * _Scene.GetBLASPackedIndices().size(), &_Scene.GetBLASPackedIndices()[0], GL_RGB32I);
    GLUtil::InitializeTBO(_BLASPackedIndicesRangeTBO, sizeof(Vec2i) * _Scene.GetBLASPackedIndicesRange().size(), &_Scene.GetBLASPackedIndicesRange()[0], GL_RG32I);
    GLUtil::InitializeTBO(_BLASPackedVerticesTBO, sizeof(Vec3) * _Scene.GetBLASPackedVertices().size(), &_Scene.GetBLASPackedVertices()[0], GL_RGB32F);
    GLUtil::InitializeTBO(_BLASPackedNormalsTBO, sizeof(Vec3) * _Scene.GetBLASPackedNormals().size(), &_Scene.GetBLASPackedNormals()[0], GL_RGB32F);
    GLUtil::InitializeTBO(_BLASPackedUVsTBO, sizeof(Vec2) * _Scene.GetBLASPackedUVs().size(), &_Scene.GetBLASPackedUVs()[0], GL_RG32F);
  }

  //this -> ReloadEnvMap();

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadSceneInstances
// ----------------------------------------------------------------------------
int PathTracer::ReloadSceneInstances()
{
  if ( 0 != _Scene.RebuildTLASData() )
    return 1;

  _NbMeshInstances = static_cast<int>(_Scene.GetTLASPackedMeshMatID().size());

  if ( _NbTriangles )
  {
    if ( 0 != this -> UploadTLASData() )
      return 1;
  }

  _FrameNum = 0;
  _NbCompleteFrames = 0;

  return 0;
}

// ----------------------------------------------------------------------------
// UploadTLASData
// ----------------------------------------------------------------------------
int PathTracer::UploadTLASData()
{
  const std::vector<GpuBvh::Node> & TLASNodes = _Scene.GetTLASNode();
  const std::vector<Vec2i>        & TLASMeshMatID = _Scene.GetTLASPackedMeshMatID();

  const GLsizeiptr tlasNodesSize = static_cast<GLsizeiptr>(sizeof(GpuBvh::Node) * TLASNodes.size());
  const void * tlasNodesData = TLASNodes.size() ? static_cast<const void*>(&TLASNodes[0]) : nullptr;
  if ( 0 != this -> UploadOrCreateTBO(_TLASNodesTBO, tlasNodesSize, tlasNodesData, GL_RGB32F) )
    return 1;

  if ( 0 != this -> UploadTLASTransforms() )
    return 1;

  const GLsizeiptr tlasMeshMatIDSize = static_cast<GLsizeiptr>(sizeof(Vec2i) * TLASMeshMatID.size());
  const void * tlasMeshMatIDData = TLASMeshMatID.size() ? static_cast<const void*>(&TLASMeshMatID[0]) : nullptr;
  if ( 0 != this -> UploadOrCreateTBO(_TLASMeshMatIDTBO, tlasMeshMatIDSize, tlasMeshMatIDData, GL_RG32I) )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// UploadTLASTransforms
// ----------------------------------------------------------------------------
int PathTracer::UploadTLASTransforms()
{
  const std::vector<Mat4x4> & TLASTransforms = _Scene.GetTLASPackedTransforms();

  const GLsizei texWidth = static_cast<GLsizei>((sizeof(Mat4x4) / sizeof(Vec4)) * TLASTransforms.size());
  if ( ( texWidth <= 0 ) || TLASTransforms.empty() )
  {
    GLUtil::DeleteTEX(_TLASTransformsIDTEX);
    return 0;
  }

  glBindTexture(GL_TEXTURE_2D, _TLASTransformsIDTEX._Handle);

  GLint curWidth = 0;
  if ( _TLASTransformsIDTEX._Handle )
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &curWidth);

  if ( curWidth == texWidth )
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, 1, GL_RGBA, GL_FLOAT, &TLASTransforms[0]);
  else
  {
    GLTextureDesc transformsDesc;
    transformsDesc._Target         = _TLASTransformsIDTEX._Target;
    transformsDesc._Slot           = _TLASTransformsIDTEX._Slot;
    transformsDesc._Width          = texWidth;
    transformsDesc._Height         = 1;
    transformsDesc._InternalFormat = _TLASTransformsIDTEX._InternalFormat;
    transformsDesc._DataFormat     = _TLASTransformsIDTEX._DataFormat;
    transformsDesc._DataType       = _TLASTransformsIDTEX._DataType;
    transformsDesc._Data           = &TLASTransforms[0];
    transformsDesc._MinFilter      = GL_NEAREST;
    transformsDesc._MagFilter      = GL_NEAREST;
    GLUtil::CreateTexture(transformsDesc, _TLASTransformsIDTEX);
    return 0;
  }

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// UploadOrCreateTBO
// ----------------------------------------------------------------------------
int PathTracer::UploadOrCreateTBO( GLTextureBuffer & ioTBO, GLsizeiptr iSize, const void * iData, GLenum iInternalformat )
{
  if ( ( iSize <= 0 ) || !iData )
  {
    GLUtil::DeleteTBO(ioTBO);
    return 0;
  }

  if ( GLUtil::UpdateTBO(ioTBO, iSize, iData) )
    return 0;

  GLUtil::DeleteTBO(ioTBO);
  GLUtil::InitializeTBO(ioTBO, iSize, iData, iInternalformat);

  return 0;
}

// ----------------------------------------------------------------------------
// ReloadEnvMap
// ----------------------------------------------------------------------------
int PathTracer::ReloadEnvMap()
{
  GLUtil::DeleteTEX(_EnvMapTEX);
  GLUtil::DeleteTEX(_EnvMapCDFTEX);

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
    envDesc._MinFilter      = GL_LINEAR;
    envDesc._MagFilter      = GL_LINEAR;
    GLUtil::CreateTexture(envDesc, _EnvMapTEX);

    _Scene.GetEnvMap().SetHandle(_EnvMapTEX._Handle);

    GLTextureDesc cdfDesc;
    cdfDesc._Target         = _EnvMapCDFTEX._Target;
    cdfDesc._Slot           = _EnvMapCDFTEX._Slot;
    cdfDesc._Width          = _Scene.GetEnvMap().GetWidth();
    cdfDesc._Height         = _Scene.GetEnvMap().GetHeight();
    cdfDesc._InternalFormat = _EnvMapCDFTEX._InternalFormat;
    cdfDesc._DataFormat     = _EnvMapCDFTEX._DataFormat;
    cdfDesc._DataType       = _EnvMapCDFTEX._DataType;
    cdfDesc._Data           = _Scene.GetEnvMap().GetCDF();
    cdfDesc._MinFilter      = GL_NEAREST;
    cdfDesc._MagFilter      = GL_NEAREST;
    GLUtil::CreateTexture(cdfDesc, _EnvMapCDFTEX);
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
