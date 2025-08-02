#include "PathTracer.h"

#include "Scene.h"
#include "EnvMap.h"
#include "ShaderProgram.h"
#include "GLUtil.h"

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

  UnloadScene();
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
  _PathTraceTime       = 0.;
  _AccumulateTime     = 0.;
  _DenoiseTime        = 0.;
  _RenderToScreenTime = 0.;

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
  GLuint64 startTime = 0, endTime = 0, executionTime = 0;
  GLint resultAvailable = 0;

  // Path trace pass
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( _PathTraceTimeId[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( _PathTraceTimeId[0], GL_QUERY_RESULT, &startTime );

  resultAvailable = 0;
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( _PathTraceTimeId[1], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( _PathTraceTimeId[1], GL_QUERY_RESULT, &endTime );

  executionTime = endTime - startTime; // nano seconds
  _PathTraceTime = (double)executionTime / 1000000000.; // Convert to seconds

  // Accumulate pass
  resultAvailable = 0;
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( _AccumulateTimeId[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( _AccumulateTimeId[0], GL_QUERY_RESULT, &startTime );

  resultAvailable = 0;
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( _AccumulateTimeId[1], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( _AccumulateTimeId[1], GL_QUERY_RESULT, &endTime );

  executionTime = endTime - startTime; // nano seconds
  _AccumulateTime = (double)executionTime / 1000000000.; // Convert to seconds

  // Denoise pass
  if ( Denoise() )
  {
    resultAvailable = 0;
    while ( !resultAvailable )
    {
      glGetQueryObjectiv( _DenoiseTimeId[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
    }
    glGetQueryObjectui64v( _DenoiseTimeId[0], GL_QUERY_RESULT, &startTime );

    resultAvailable = 0;
    while ( !resultAvailable )
    {
      glGetQueryObjectiv( _DenoiseTimeId[1], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
    }
    glGetQueryObjectui64v( _DenoiseTimeId[1], GL_QUERY_RESULT, &endTime );

    executionTime = endTime - startTime; // nano seconds
    _DenoiseTime = (double)executionTime / 1000000000.; // Convert to seconds
  }
  else
    _DenoiseTime = 0.;

  // Render to screen pass
  resultAvailable = 0;
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( _RenderToScreenTimeId[0], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( _RenderToScreenTimeId[0], GL_QUERY_RESULT, &startTime );

  resultAvailable = 0;
  while ( !resultAvailable )
  {
    glGetQueryObjectiv( _RenderToScreenTimeId[1], GL_QUERY_RESULT_AVAILABLE, &resultAvailable );
  }
  glGetQueryObjectui64v( _RenderToScreenTimeId[1], GL_QUERY_RESULT, &endTime );

  executionTime = endTime - startTime; // nano seconds
  _RenderToScreenTime = (double)executionTime / 1000000000.; // Convert to seconds

  return 0;
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
    const std::vector<Material> & Materials =  _Scene.GetMaterials();

    glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
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
  // Path trace
  glQueryCounter(_PathTraceTimeId[0], GL_TIMESTAMP);

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

  glQueryCounter(_PathTraceTimeId[1], GL_TIMESTAMP);

  // Accumulate
  glQueryCounter(_AccumulateTimeId[0], GL_TIMESTAMP);

  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._Handle);
  if ( TiledRendering() && !LowResPass() )
    glViewport(_Settings._TileResolution.x * _CurTile.x, _Settings._TileResolution.y * _CurTile.y, _Settings._TileResolution.x, _Settings._TileResolution.y);
  else
    glViewport(0, 0, RenderWidth(), RenderHeight());

  this -> BindAccumulateTextures();

  _Quad.Render(*_AccumulateShader);

  glQueryCounter(_AccumulateTimeId[1], GL_TIMESTAMP);

  // Denoise
  if ( Denoise() )
    this -> DenoiseOutput();

  return 0;
}

// ----------------------------------------------------------------------------
// DenoiseOutput
// ----------------------------------------------------------------------------
int PathTracer::DenoiseOutput()
{
  glQueryCounter(_DenoiseTimeId[0], GL_TIMESTAMP);

  _DenoiserShader -> Use();

  this -> BindDenoiserTextures();

  // Dispatch compute shader (assuming texture size is 512x512)
  const int workGroupSizeX = 16, workGroupSizeY = 16;
  const int nbGroupsX = std::ceil(((float)RenderWidth())/workGroupSizeX);
  const int nbGroupsY = std::ceil(((float)RenderHeight())/workGroupSizeY);
  glDispatchCompute(nbGroupsX, nbGroupsY, 1);

  // Ensure GPU has completed work before continuing
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

  _DenoiserShader -> StopUsing();

  glQueryCounter(_DenoiseTimeId[1], GL_TIMESTAMP);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateDenoiserUniforms
// ----------------------------------------------------------------------------
int PathTracer::UpdateDenoiserUniforms()
{
  _DenoiserShader -> Use();

  _DenoiserShader -> SetUniform("u_DenoisingMethod", (int)_Settings._DenoisingMethod);    // 0: Bilateral, 1: Wavelet, 2: Edge-aware
  _DenoiserShader -> SetUniform("u_SigmaSpatial", _Settings._DenoiserSigmaSpatial);       // Bilateral
  _DenoiserShader -> SetUniform("u_SigmaRange", _Settings._DenoiserSigmaRange);           // Bilateral
  _DenoiserShader -> SetUniform("u_Threshold", _Settings._DenoiserThreshold);             // Wavelet
  _DenoiserShader -> SetUniform("u_WaveletScale", (int)_Settings._DenoisingWaveletScale); // Wavelet
  _DenoiserShader -> SetUniform("u_ColorPhi", _Settings._DenoiserColorPhi);               // Edge-aware
  _DenoiserShader -> SetUniform("u_NormalPhi", _Settings._DenoiserNormalPhi);             // Edge-aware
  _DenoiserShader -> SetUniform("u_PositionPhi", _Settings._DenoiserPositionPhi);         // Edge-aware

  _DenoiserShader -> StopUsing();

  return 0;
}

// ----------------------------------------------------------------------------
// BindDenoiserTextures
// ----------------------------------------------------------------------------
int PathTracer::BindDenoiserTextures()
{
  if ( 3 ==_AccumulateFBO._Tex.size() )
  {
    glBindImageTexture(0, _AccumulateFBO._Tex[0]._Handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, _AccumulateFBO._Tex[1]._Handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, _AccumulateFBO._Tex[2]._Handle, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
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
  glQueryCounter(_RenderToScreenTimeId[0], GL_TIMESTAMP);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  this -> BindRenderToScreenTextures();

  _Quad.Render(*_RenderToScreenShader);

  glQueryCounter(_RenderToScreenTimeId[1], GL_TIMESTAMP);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
int PathTracer::RenderToFile( const fs::path & iFilePath )
{
  GLFrameBuffer temporaryFBO;
  temporaryFBO._Tex.push_back({0, GL_TEXTURE_2D, PathTracerTexSlot::_Temporary});

  // Temporary frame buffer
  glGenTextures(1, &temporaryFBO._Tex[0]._Handle);
  glActiveTexture(GL_TEX_UNIT(temporaryFBO._Tex[0]));
  glBindTexture(GL_TEXTURE_2D, temporaryFBO._Tex[0]._Handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._WindowResolution.x, _Settings._WindowResolution.y, 0, GL_RGBA, GL_FLOAT, nullptr);
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

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffers
// ----------------------------------------------------------------------------
int PathTracer::InitializeFrameBuffers()
{
  UpdateRenderResolution();

  // Render target textures
  _RenderTargetFBO._Tex.clear();
  _RenderTargetFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTarget, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetFBO._Tex[0] ); // Albedo
  _RenderTargetFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetNormals, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetFBO._Tex[1] ); // Normals
  _RenderTargetFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetPos, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetFBO._Tex[2] ); // Positions

  _RenderTargetTileFBO._Tex.clear();
  _RenderTargetTileFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetTile, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, TileWidth(), TileHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetTileFBO._Tex[0] ); // Albedo
  _RenderTargetTileFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetNormals, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, TileWidth(), TileHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetTileFBO._Tex[1] ); // Normals
  _RenderTargetTileFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetPos, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, TileWidth(), TileHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetTileFBO._Tex[2] ); // Positions

  _RenderTargetLowResFBO._Tex.clear();
  _RenderTargetLowResFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_RenderTargetLowRes, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, LowResRenderWidth(), LowResRenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _RenderTargetLowResFBO._Tex[0] );

  _AccumulateFBO._Tex.clear();
  _AccumulateFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_Accumulate, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _AccumulateFBO._Tex[0] ); // Albedo
  _AccumulateFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_AccumulateNormals, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _AccumulateFBO._Tex[1] ); // Normals
  _AccumulateFBO._Tex.push_back( { 0, GL_TEXTURE_2D, PathTracerTexSlot::_AccumulatePos, GL_RGBA32F, GL_RGBA, GL_FLOAT } );
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _AccumulateFBO._Tex[2] ); // Positions

  // Render target Frame buffers
  GLenum DrawBuffers[] =
  {
      GL_COLOR_ATTACHMENT0,
      GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2
  };

  glGenFramebuffers(1, &_RenderTargetFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetFBO._Tex[0]._Handle, 0); // Albedo
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _RenderTargetFBO._Tex[1]._Handle, 0); // Normals
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _RenderTargetFBO._Tex[2]._Handle, 0); // Positions
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glDrawBuffers(3, DrawBuffers);

  glGenFramebuffers(1, &_RenderTargetTileFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetTileFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetTileFBO._Tex[0]._Handle, 0); // Albedo
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _RenderTargetTileFBO._Tex[1]._Handle, 0); // Normals
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _RenderTargetTileFBO._Tex[2]._Handle, 0); // Positions
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glDrawBuffers(3, DrawBuffers);

  glGenFramebuffers(1, &_RenderTargetLowResFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _RenderTargetLowResFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _RenderTargetLowResFBO._Tex[0]._Handle, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glGenFramebuffers(1, &_AccumulateFBO._Handle);
  glBindFramebuffer(GL_FRAMEBUFFER, _AccumulateFBO._Handle);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _AccumulateFBO._Tex[0]._Handle, 0); // Albedo
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, _AccumulateFBO._Tex[1]._Handle, 0); // Normals
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, _AccumulateFBO._Tex[2]._Handle, 0); // Positions
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  glDrawBuffers(3, DrawBuffers);

  // Denoised texture
  GLUtil::GenTexture( GL_TEXTURE_2D, GL_RGBA32F, RenderWidth(), RenderHeight(), GL_RGBA, GL_FLOAT, nullptr, _DenoisedTEX );
  glBindTexture(GL_TEXTURE_2D, _DenoisedTEX._Handle);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int PathTracer::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_PathTracer.glsl");

  ShaderProgram * newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _PathTraceShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Accumulate.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _AccumulateShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Postprocess.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RenderToScreenShader.reset(newShader);

  ShaderSource computeShaderSrc = Shader::LoadShader("..\\..\\shaders\\compute_Denoiser.glsl");
  newShader = ShaderProgram::LoadShaders(computeShaderSrc);
  if ( !newShader )
    return 1;
  _DenoiserShader.reset(newShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UnloadScene
// ----------------------------------------------------------------------------
int PathTracer::UnloadScene()
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

  GLUtil::DeleteTEX(_DenoisedTEX);
  GLUtil::DeleteTEX(_TexArrayTEX);
  GLUtil::DeleteTEX(_MaterialsTEX);
  GLUtil::DeleteTEX(_TLASTransformsIDTEX);
  GLUtil::DeleteTEX(_EnvMapTEX);
  GLUtil::DeleteTEX(_EnvMapCDFTEX);

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
  _NbMeshInstances = _Scene.GetNbMeshInstances();

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

      glGenTextures(1, &_TexArrayTEX._Handle);
      glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTEX._Handle);
      glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, _Settings._TextureSize.x, _Settings._TextureSize.y, _Scene.GetNbCompiledTex(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_Scene.GetTextureArray()[0]);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }

    GLUtil::InitializeTBO(_MeshBBoxTBO, sizeof(Vec3) * _Scene.GetMeshBBoxes().size(), &_Scene.GetMeshBBoxes()[0], GL_RGB32F);
    GLUtil::InitializeTBO(_MeshIdRangeTBO, sizeof(int) * _Scene.GetMeshIdxRange().size(), &_Scene.GetMeshIdxRange()[0], GL_R32I);
    GLUtil::InitializeTBO(_MeshIdRangeTBO, sizeof(int) * _Scene.GetMeshIdxRange().size(), &_Scene.GetMeshIdxRange()[0], GL_R32I);

    glGenTextures(1, &_MaterialsTEX._Handle);
    glBindTexture(GL_TEXTURE_2D, _MaterialsTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * _Scene.GetMaterials().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetMaterials()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // BVH
    GLUtil::InitializeTBO(_TLASNodesTBO, sizeof(GpuBvh::Node) * _Scene.GetTLASNode().size(), &_Scene.GetTLASNode()[0], GL_RGB32F);

    glGenTextures(1, &_TLASTransformsIDTEX._Handle);
    glBindTexture(GL_TEXTURE_2D, _TLASTransformsIDTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Mat4x4) / sizeof(Vec4)) * _Scene.GetTLASPackedTransforms().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene.GetTLASPackedTransforms()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLUtil::InitializeTBO(_TLASMeshMatIDTBO, sizeof(Vec2i) * _Scene.GetTLASPackedMeshMatID().size(), &_Scene.GetTLASPackedMeshMatID()[0], GL_RG32I);
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
// ReloadEnvMap
// ----------------------------------------------------------------------------
int PathTracer::ReloadEnvMap()
{
  GLUtil::DeleteTEX(_EnvMapTEX);
  GLUtil::DeleteTEX(_EnvMapCDFTEX);

  if ( _Scene.GetEnvMap().IsInitialized() )
  {
    glGenTextures(1, &_EnvMapTEX._Handle);
    glBindTexture(GL_TEXTURE_2D, _EnvMapTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, _Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight(), 0, GL_RGB, GL_FLOAT, _Scene.GetEnvMap().GetRawData());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    _Scene.GetEnvMap().SetHandle(_EnvMapTEX._Handle);

    glGenTextures(1, &_EnvMapCDFTEX._Handle);
    glBindTexture(GL_TEXTURE_2D, _EnvMapCDFTEX._Handle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, _Scene.GetEnvMap().GetWidth(), _Scene.GetEnvMap().GetHeight(), 0, GL_RED, GL_FLOAT, _Scene.GetEnvMap().GetCDF());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
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
