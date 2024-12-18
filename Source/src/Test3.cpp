#include "Test3.h"
#include "QuadMesh.h"
#include "ShaderProgram.h"
#include "Scene.h"
#include "Camera.h"
#include "Light.h"
#include "Primitive.h"
#include "PrimitiveInstance.h"
#include "Mesh.h"
#include "MeshInstance.h"
#include "Texture.h"
#include "MathUtil.h"
#include "Loader.h"

#include "tinydir.h"

#include "stb_image_write.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtx/euler_angles.hpp"

#include <algorithm>
#include <iostream>

#define TEX_UNIT(x) ( GL_TEXTURE0 + x )

namespace fs = std::filesystem;

namespace RTRT
{

const char * Test3::GetTestHeader() { return "Test 3 : Simple path tracer"; }

// ----------------------------------------------------------------------------
// GLOBAL VARIABLES
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

static bool g_RenderToFile = false;
static fs::path g_FilePath;

// ----------------------------------------------------------------------------
// GLOBAL FUNCTIONS
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void Test3::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto * const this_ = static_cast<Test3*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  if ( ( action == GLFW_PRESS ) || ( action == GLFW_RELEASE ) )
  {
    std::cout << "EVENT : KEY PRESSED" << std::endl;

    switch ( key )
    {
    case GLFW_KEY_W:
      this_ -> _KeyUp =    ( action == GLFW_PRESS ); break;
    case GLFW_KEY_S:
      this_ -> _KeyDown =  ( action == GLFW_PRESS ); break;
    case GLFW_KEY_A:
      this_ -> _KeyLeft =  ( action == GLFW_PRESS ); break;
    case GLFW_KEY_D:
      this_ -> _KeyRight = ( action == GLFW_PRESS ); break;
    case GLFW_KEY_ESCAPE:
      this_ -> _KeyEsc = ( action == GLFW_PRESS ); break;
    case GLFW_KEY_R:
      this_ -> _KeyR = ( action == GLFW_PRESS ); break;
    default :
      break;
    }
  }
}

// ----------------------------------------------------------------------------
// MousebuttonCallback
// ----------------------------------------------------------------------------
void Test3::MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test3*>(glfwGetWindowUserPointer(window));
    if ( !this_ )
      return;

    glfwGetCursorPos(window, &this_->_MouseX, &this_->_MouseY);

    if ( ( GLFW_MOUSE_BUTTON_1 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : LEFT CLICK (" << this_->_MouseX << "," << this_->_MouseY << ")" << std::endl;
      this_->_LeftClick = true;
    }
    else
      this_->_LeftClick = false;

    if ( ( GLFW_MOUSE_BUTTON_2 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : RIGHT CLICK (" << this_->_MouseX << "," << this_->_MouseY << ")" << std::endl;
      this_->_RightClick = true;
    }
    else
      this_->_RightClick = false;

    if ( ( GLFW_MOUSE_BUTTON_3 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : MIDDLE CLICK (" << this_->_MouseX << "," << this_->_MouseY << ")" << std::endl;
      this_->_MiddleClick = true;
    }
    else
      this_->_MiddleClick = false;
  }
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void Test3::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  auto * const this_ = static_cast<Test3*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;

  this_ -> _Settings._RenderResolution.x = width;
  this_ -> _Settings._RenderResolution.y = height;
  glViewport(0, 0, this_ -> _Settings._RenderResolution.x, this_ -> _Settings._RenderResolution.y);
  this_ -> _RenderSettingsModified = true;

  glBindTexture(GL_TEXTURE_2D, this_->_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, this_ -> RenderWidth(), this_ -> RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
}

std::string UniformArrayElementName( const char * iUniformArrayName, int iIndex, const char * iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

// https://github.com/ocornut/imgui/issues/911
bool VectorOfStringGetter(void* data, int n, const char** out_text)
{
  const std::vector<std::string> * v = (std::vector<std::string>*)data;
  *out_text = (*v)[n].c_str();
  return true;
}

void LoadTexture( unsigned int iTexUnit, const Texture * iTexture, GLuint & ioTexID )
{
  if ( ioTexID )
    glDeleteTextures(1, &ioTexID);
  ioTexID = 0;

  if ( iTexture )
  {
    glGenTextures(1, &ioTexID);
    glActiveTexture(TEX_UNIT(iTexUnit));
    glBindTexture(GL_TEXTURE_2D, ioTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, iTexture -> GetWidth(), iTexture -> GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, iTexture -> GetUCData());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Test3::Test3( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: BaseTest(iMainWindow, iScreenWidth, iScreenHeight)
{
  _Settings._RenderResolution.x = iScreenWidth;
  _Settings._RenderResolution.y = iScreenHeight;
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test3::~Test3()
{
  ClearSceneData();

  glDeleteFramebuffers(1, &_FrameBufferID);
  glDeleteTextures(1, &_ScreenTextureID);
  
  if (_Quad)
    delete _Quad;
  _Quad = nullptr;
  if (_RTTShader)
    delete _RTTShader;
  _RTTShader = nullptr;
  if (_RTSShader)
    delete _RTSShader;
  _RTSShader = nullptr;

  for ( auto fileName : _SceneNames )
    delete[] fileName;
}

// ----------------------------------------------------------------------------
// ClearSceneData
// ----------------------------------------------------------------------------
void Test3::ClearSceneData()
{
  glDeleteBuffers(1, &_VtxBufferID);
  glDeleteBuffers(1, &_VtxNormBufferID);
  glDeleteBuffers(1, &_VtxUVBufferID);
  glDeleteBuffers(1, &_VtxIndBufferID);
  glDeleteBuffers(1, &_TexIndBufferID);
  glDeleteBuffers(1, &_MeshBBoxBufferID);
  glDeleteBuffers(1, &_MeshIdRangeBufferID);
  // BVH
  glDeleteBuffers(1, &_TLASNodesBufferID);
  glDeleteBuffers(1, &_TLASMeshMatIDBufferID);
  glDeleteBuffers(1, &_BLASNodesBufferID);
  glDeleteBuffers(1, &_BLASNodesRangeBufferID);
  glDeleteBuffers(1, &_BLASPackedIndicesBufferID);
  glDeleteBuffers(1, &_BLASPackedIndicesRangeBufferID);
  glDeleteBuffers(1, &_BLASPackedVerticesBufferID);
  glDeleteBuffers(1, &_BLASPackedNormalsBufferID);
  glDeleteBuffers(1, &_BLASPackedUVsBufferID);

  glDeleteTextures(1, &_SkyboxTextureID);
  glDeleteTextures(1, &_VtxTextureID);
  glDeleteTextures(1, &_VtxNormTextureID);
  glDeleteTextures(1, &_VtxUVTextureID);
  glDeleteTextures(1, &_VtxIndTextureID);
  glDeleteTextures(1, &_TexIndTextureID);
  glDeleteTextures(1, &_TexArrayTextureID);
  glDeleteTextures(1, &_MeshBBoxTextureID);
  glDeleteTextures(1, &_MeshIdRangeTextureID);
  glDeleteTextures(1, &_MaterialsTextureID);
  glDeleteTextures(1, &_TLASNodesTextureID);
  glDeleteTextures(1, &_TLASMeshMatIDTextureID);
  glDeleteTextures(1, &_TLASTransformsIDTextureID);
  glDeleteTextures(1, &_BLASNodesTextureID);
  glDeleteTextures(1, &_BLASNodesRangeTextureID);
  glDeleteTextures(1, &_BLASPackedIndicesTextureID);
  glDeleteTextures(1, &_BLASPackedIndicesRangeTextureID);
  glDeleteTextures(1, &_BLASPackedVerticesTextureID);
  glDeleteTextures(1, &_BLASPackedNormalsTextureID);
  glDeleteTextures(1, &_BLASPackedUVsTextureID);

  _MaterialNames.clear();
  _PrimitiveNames.clear();

  _SelectedLight     = -1;
  _SelectedMaterial  = -1;
  _SelectedPrimitive = -1;
  _NbTriangles       = 0;
  _NbMeshInstances   = 0;

  if ( _Scene )
    delete _Scene;
  _Scene = nullptr;
}

// ----------------------------------------------------------------------------
// InitializeSceneFiles
// ----------------------------------------------------------------------------
int Test3::InitializeSceneFiles()
{
  tinydir_dir dir;
  tinydir_open_sorted(&dir, g_AssetsDir.c_str());
  
  for ( int i = 0; i < dir.n_files; ++i )
  {
    tinydir_file file;
    tinydir_readfile_n(&dir, &file, i);
  
    std::string extension(file.extension);
    if ( ( "scene" == extension ) || ( "gltf" == extension ) || ( "glb" == extension ) )
    {
      char * filename = new char[256];
      snprintf(filename, 256, "%s", file.name);
      _SceneNames.push_back(filename);
      _SceneFiles.push_back(g_AssetsDir + std::string(file.name));

      std::size_t found = _SceneFiles[_SceneFiles.size()-1].find("Sphere.scene");
      //std::size_t found = _SceneFiles[_SceneFiles.size() - 1].find( "jinx.scene" );
      if ( std::string::npos != found )
        _CurSceneId = _SceneFiles.size()-1;
    }
  }
  
  tinydir_close(&dir);

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeBackgroundFiles
// ----------------------------------------------------------------------------
int Test3::InitializeBackgroundFiles()
{
  std::string bgdPath = g_AssetsDir + "HDR\\";

  tinydir_dir dir;
  tinydir_open_sorted( &dir, bgdPath.c_str() );

  for ( int i = 0; i < dir.n_files; ++i )
  {
    tinydir_file file;
    tinydir_readfile_n( &dir, &file, i );

    std::string extension( file.extension );
    if ( ( "hdr" == extension ) || ( "HDR" == extension ) )
    {
      char * filename = new char[256];
      snprintf( filename, 256, "%s", file.name );
      _BackgroundNames.push_back( filename );
      _BackgroundFiles.push_back( bgdPath + std::string( file.name ) );

      std::size_t found = _BackgroundFiles[_BackgroundFiles.size() - 1].find( "artist_workshop_1k.hdr" );
      if ( std::string::npos != found )
        _CurBackgroundId = _BackgroundFiles.size() - 1;
    }
  }

  tinydir_close( &dir );

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeFrameBuffer
// ----------------------------------------------------------------------------
int Test3::InitializeFrameBuffer()
{
  // Screen texture
  glGenTextures(1, &_ScreenTextureID);
  glActiveTexture(TEX_UNIT(_ScreenTextureUnit));
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  // FrameBuffer
  glGenFramebuffers(1, &_FrameBufferID);
  glBindFramebuffer(GL_FRAMEBUFFER, _FrameBufferID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _ScreenTextureID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// RecompileShaders
// ----------------------------------------------------------------------------
int Test3::RecompileShaders()
{
  if (_RTTShader)
    delete _RTTShader;
  _RTTShader = nullptr;
  if (_RTSShader)
    delete _RTSShader;
  _RTSShader = nullptr;

  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_RTRenderer.glsl");

  _RTTShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !_RTTShader )
    return 1;

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  _RTSShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !_RTSShader )
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateUniforms
// ----------------------------------------------------------------------------
int Test3::UpdateUniforms()
{
  if ( _RTTShader )
  {
    _RTTShader -> Use();
    GLuint RTTProgramID = _RTTShader -> GetShaderProgramID();
    glUniform2f(glGetUniformLocation(RTTProgramID, "u_Resolution"), RenderWidth(), RenderHeight());
    glUniform1f(glGetUniformLocation(RTTProgramID, "u_Time"), _CPULoopTime);
    glUniform1i(glGetUniformLocation(RTTProgramID, "u_FrameNum"), _FrameNum);  

    if ( !(_RenderSettingsModified + _SceneCameraModified + _SceneLightsModified + _SceneInstancesModified + _SceneMaterialsModified) && _AccumulateFrames )
    {
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_Accumulate"), 1);
      _AccumulatedFrames++;
    }
    else
    {
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_Accumulate"), 0);
      _AccumulatedFrames = 1;
    }

    if ( _RenderSettingsModified )
    {
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_Bounces"), _Settings._Bounces);
      glUniform3f(glGetUniformLocation(RTTProgramID, "u_BackgroundColor"), _Settings._BackgroundColor.r, _Settings._BackgroundColor.g, _Settings._BackgroundColor.b);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_EnableSkybox"), (int)_Settings._EnableSkybox);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_EnableBackground" ), (int)_Settings._EnableBackGround);
      glUniform1f(glGetUniformLocation(RTTProgramID, "u_SkyboxRotation"), _Settings._SkyBoxRotation / 360.f);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_ScreenTexture"), _ScreenTextureUnit);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_SkyboxTexture"), _SkyboxTextureUnit);
      glUniform1f(glGetUniformLocation(RTTProgramID, "u_Gamma"), _Settings._Gamma);
      glUniform1f(glGetUniformLocation(RTTProgramID, "u_Exposure"), _Settings._Exposure);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_ToneMapping"), ( _Settings._ToneMapping ? 1 : 0 ));
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_DebugMode" ), _DebugMode );
      _RenderSettingsModified = false;
    }

    if ( _Scene )
    {
      if ( _SceneCameraModified )
      {
        Camera & cam = _Scene -> GetCamera();
        glUniform3f(glGetUniformLocation(RTTProgramID, "u_Camera._Up"), cam.GetUp().x, cam.GetUp().y, cam.GetUp().z);
        glUniform3f(glGetUniformLocation(RTTProgramID, "u_Camera._Right"), cam.GetRight().x, cam.GetRight().y, cam.GetRight().z);
        glUniform3f(glGetUniformLocation(RTTProgramID, "u_Camera._Forward"), cam.GetForward().x, cam.GetForward().y, cam.GetForward().z);
        glUniform3f(glGetUniformLocation(RTTProgramID, "u_Camera._Pos"), cam.GetPos().x, cam.GetPos().y, cam.GetPos().z);
        glUniform1f(glGetUniformLocation(RTTProgramID, "u_Camera._FOV"), cam.GetFOV());
        _SceneCameraModified = false;
      }

      if ( _SceneLightsModified )
      {
        int nbLights = 0;

        for ( int i = 0; i < _Scene -> GetNbLights(); ++i )
        {
          Light * curLight = _Scene -> GetLight(i);
          if ( !curLight )
            continue;

          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_Pos"     ).c_str()), curLight -> _Pos.x, curLight -> _Pos.y, curLight -> _Pos.z);
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_Emission").c_str()), curLight -> _Emission.r, curLight -> _Emission.g, curLight -> _Emission.b);
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_DirU"    ).c_str()), curLight -> _DirU.x, curLight -> _DirU.y, curLight -> _DirU.z);
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_DirV"    ).c_str()), curLight -> _DirV.x, curLight -> _DirV.y, curLight -> _DirV.z);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_Radius"  ).c_str()), curLight -> _Radius);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_Area"    ).c_str()), curLight -> _Area);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Lights",i,"_Type"    ).c_str()), curLight -> _Type);

          nbLights++;
          if ( nbLights >= 32 )
            break;
        }

        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbLights"), nbLights);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_ShowLights"), (int)_Settings._ShowLights);

        _SceneLightsModified = false;
      }

      if ( _SceneMaterialsModified )
      {
        const std::vector<Material> & Materials =  _Scene -> GetMaterials();

        glBindTexture(GL_TEXTURE_2D, _MaterialsTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * _Scene -> GetMaterials().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene -> GetMaterials()[0]);
        glBindTexture(GL_TEXTURE_2D, 0);

        _SceneMaterialsModified = false;
      }

      if ( _SceneInstancesModified )
      {
        //const std::vector<Mesh*>          & Meshes          = _Scene -> GetMeshes();
        const std::vector<Primitive*>        & Primitives         = _Scene -> GetPrimitives();
        //const std::vector<MeshInstance>   & MeshInstances   = _Scene -> GetMeshInstances();
        const std::vector<PrimitiveInstance> & PrimitiveInstances = _Scene -> GetPrimitiveInstances();

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

            glUniform1i(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Spheres",nbSpheres,"_MaterialID").c_str()), prim._MaterialID);
            glUniform4f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Spheres",nbSpheres,"_CenterRad").c_str()), CenterRad.x, CenterRad.y, CenterRad.z, CenterRad.w);
            nbSpheres++;
          }
          else if ( curPrimitive -> _Type == PrimitiveType::Plane )
          {
            Plane * curPlane = (Plane *) curPrimitive;
            Vec4 orig = prim._Transform * Vec4(curPlane -> _Origin.x, curPlane -> _Origin.y, curPlane -> _Origin.z, 1.f);
            Vec4 normal = glm::transpose(glm::inverse(prim._Transform)) * Vec4(curPlane -> _Normal.x, curPlane -> _Normal.y, curPlane -> _Normal.z, 1.f);

            glUniform1i(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Planes",nbPlanes,"_MaterialID").c_str()), prim._MaterialID);
            glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Planes",nbPlanes,"_Orig").c_str()), orig.x, orig.y, orig.z);
            glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Planes",nbPlanes,"_Normal").c_str()), normal.x, normal.y, normal.z);
            nbPlanes++;
          }
          else if ( curPrimitive -> _Type == PrimitiveType::Box )
          {
            Box * curBox = (Box *) curPrimitive;

            glUniform1i(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Boxes",nbBoxes,"_MaterialID").c_str()), prim._MaterialID);
            glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Boxes",nbBoxes,"_Low").c_str()), curBox -> _Low.x, curBox -> _Low.y, curBox -> _Low.z);
            glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Boxes",nbBoxes,"_High").c_str()), curBox -> _High.x, curBox -> _High.y, curBox -> _High.z);
            glUniformMatrix4fv(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Boxes",nbBoxes,"_Transfom").c_str()), 1, GL_FALSE, glm::value_ptr(prim._Transform));
            nbBoxes++;
          }
        }

        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxTexture"),                    _VtxTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxNormTexture"),                _VtxNormTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxUVTexture"),                  _VtxUVTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxIndTexture"),                 _VtxIndTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TexIndTexture"),                 _TexIndTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TexArrayTexture"),               _TexArrayTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_MeshBBoxTexture"),               _MeshBBoxTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_MeshIDRangeTexture"),            _MeshIdRangeTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_MaterialsTexture"),              _MaterialsTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TLASNodesTexture"),              _TLASNodesTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TLASTransformsTexture"),         _TLASTransformsIDTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TLASMeshMatIDTexture"),          _TLASMeshMatIDTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASNodesTexture"),              _BLASNodesTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASNodesRangeTexture"),         _BLASNodesRangeTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASPackedIndicesTexture"),      _BLASPackedIndicesTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASPackedIndicesRangeTexture"), _BLASPackedIndicesRangeTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASPackedVtxTexture"),          _BLASPackedVerticesTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASPackedNormTexture"),         _BLASPackedNormalsTextureUnit);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_BLASPackedUVTexture"),           _BLASPackedUVsTextureUnit);

        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbSpheres"), nbSpheres);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbPlanes"), nbPlanes);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbBoxes"), nbBoxes);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbTriangles"), _NbTriangles);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbMeshInstances"), _NbMeshInstances);

        _SceneInstancesModified = false;
      }
    }
    _RTTShader -> StopUsing();
  }
  else
    return 1;

  if ( _RTSShader )
  {
    _RTSShader -> Use();
    GLuint RTSProgramID = _RTSShader -> GetShaderProgramID();
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_ScreenTexture"), _ScreenTextureUnit);
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_AccumulatedFrames"), _AccumulatedFrames);
    glUniform2f(glGetUniformLocation(RTSProgramID, "u_RenderRes" ), _Settings._RenderResolution.x, _Settings._RenderResolution.y);
    glUniform1f(glGetUniformLocation(RTSProgramID, "u_Gamma"), _Settings._Gamma);
    glUniform1f(glGetUniformLocation(RTSProgramID, "u_Exposure"), _Settings._Exposure);
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_ToneMapping"), 0);
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_FXAA"), (_Settings._FXAA ?  1 : 0 ));
    _RTSShader -> StopUsing();
  }
  else
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeUI
// ----------------------------------------------------------------------------
int Test3::InitializeUI()
{
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Load Fonts
  io.Fonts->AddFontDefault();

  // Setup Platform/Renderer backends
  const char* glsl_version = "#version 130";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
int Test3::DrawUI()
{
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  {
    ImGui::Begin("Test 3");

    ImGui::Text( "Window width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y );

    ImGui::Text( "Accumulated frames : %d", _AccumulatedFrames );

    ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);

    {
      static std::vector<float> s_FrameRateHistory;
      static int                s_LastFrameIndex = -1;
      static double             s_AccumTime = 0.f;
      static float              s_Max = 300.;

      if ( -1 == s_LastFrameIndex )
      {
        s_FrameRateHistory.assign( 120, 0.f );
        s_LastFrameIndex = 0;
        s_FrameRateHistory[s_LastFrameIndex] = (float)_FrameRate;
      }

      s_AccumTime += _TimeDelta;
      while ( s_AccumTime > ( 1.f / 60 ) )
      {
        s_AccumTime -= 0.1;
        s_Max = *std::max_element( s_FrameRateHistory.begin(), s_FrameRateHistory.end() );

        s_LastFrameIndex++;
        if ( s_LastFrameIndex >= 120 )
          s_LastFrameIndex = 0;
        s_FrameRateHistory[s_LastFrameIndex] = (float)_FrameRate;
      }

      int offset = ( s_LastFrameIndex >= 119 ) ? ( 0 ) : ( s_LastFrameIndex + 1 );

      char overlay[32];
      sprintf( overlay, "%.1f FPS", _FrameRate );
      ImGui::PlotLines( "Frame rate", &s_FrameRateHistory[0], s_FrameRateHistory.size(), offset, overlay, -0.1f, s_Max, ImVec2( 0, 80.0f ) );
    }

    int selectedSceneId = _CurSceneId;
    if ( ImGui::Combo("Scene", &selectedSceneId, _SceneNames.data(), _SceneNames.size()) )
    {
      if ( selectedSceneId != _CurSceneId )
      {
        _CurSceneId = selectedSceneId;
        _ReloadScene = true;
      }
    }

    if ( ImGui::Button( "Capture image" ) )
    {
      g_FilePath = "./Test3_" + std::string( _SceneNames[_CurSceneId] ) + "_" + std::to_string( _AccumulatedFrames ) + "frames.png";
      g_RenderToFile = true;
    }

    if ( ImGui::CollapsingHeader("Render settings") )
    {
      if ( ImGui::SliderInt("Bounces", &_Settings._Bounces, 1, 10) )
        _RenderSettingsModified = true;

      if ( ImGui::SliderInt("Render scale", &_Settings._RenderScale, 25, 200) )
      {
        glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);

        _RenderSettingsModified = true;
      }
      ImGui::SameLine();
      if ( ImGui::Checkbox("Auto", &_Settings._AutoScale) )
      {
        _RenderSettingsModified = true;
      }

      if ( _Settings._AutoScale )
      {
        int targetFrameRate = _Settings._TargetFPS;
        if ( ImGui::SliderInt("Target FPS", &targetFrameRate, 1, 200) )
        {
          _Settings._TargetFPS = (float)targetFrameRate;
          _RenderSettingsModified = true;
        }

        this -> AdjustRenderScale();
      }

      if ( ImGui::Checkbox( "Accumulate", &_AccumulateFrames ) )
        _RenderSettingsModified = true;

      if ( ImGui::Checkbox( "FXAA", &_Settings._FXAA ) )
        _RenderSettingsModified = true;

      if ( ImGui::Checkbox( "Tone mapping", &_Settings._ToneMapping ) )
        _RenderSettingsModified = true;

      if ( _Settings._ToneMapping )
      {
        if ( ImGui::SliderFloat( "Gamma", &_Settings._Gamma, .5f, 3.f ) )
          _RenderSettingsModified = true;

        if ( ImGui::SliderFloat( "Exposure", &_Settings._Exposure, .1f, 5.f ) )
          _RenderSettingsModified = true;
      }

      float rgb[3] = { _Settings._BackgroundColor.r, _Settings._BackgroundColor.g, _Settings._BackgroundColor.b };
      if ( ImGui::ColorEdit3("Background", rgb) )
      {
        _Settings._BackgroundColor = { rgb[0], rgb[1], rgb[2] };
        _RenderSettingsModified = true;
      }

      if ( ImGui::Checkbox( "Show background", &_Settings._EnableBackGround ) )
        _RenderSettingsModified = true;

      if ( _BackgroundFiles.size() )
      {
        if ( ImGui::Checkbox("Enable environment mapping", &_Settings._EnableSkybox) )
          _RenderSettingsModified = true;

        if ( _Settings._EnableSkybox )
        {
          int selectedBgdId = _CurBackgroundId;
          if ( ImGui::Combo( "Background", &selectedBgdId, _BackgroundNames.data(), _BackgroundNames.size() ) )
          {
            if ( selectedBgdId != _CurBackgroundId )
            {
              _CurBackgroundId = selectedBgdId;
              _ReloadBackground = true;
            }
            _RenderSettingsModified = true;
          }

          if ( ImGui::SliderFloat("Skybox rotation", &_Settings._SkyBoxRotation, 0.f, 360.f) )
            _RenderSettingsModified = true;

          if ( _SkyboxTextureID >= 0 )
          {
            ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(_SkyboxTextureID);
            ImGui::Image(texture, ImVec2(128, 128));
          }
        }
      }

      static const char * DEBUG_MODES[] = { "Off", "Albedo", "Metalness", "Roughness", "Normals", "UV" };
      if ( ImGui::Combo( "Debug view", &_DebugMode, DEBUG_MODES, 6 ) )
        _RenderSettingsModified = true;
    }

    if ( ImGui::CollapsingHeader("Camera") )
    {
      Camera & cam = _Scene -> GetCamera();

      ImGui::Text("Position : %.2f, %.2f, %.2f", cam.GetPos().x, cam.GetPos().y, cam.GetPos().z);

      ImGui::Text("Yaw : %.2f, Pitch : %.2f, Radius %.2f", cam.GetYaw(), cam.GetPitch(), cam.GetRadius());

      float fov = MathUtil::ToDegrees(cam.GetFOV());
      if ( ImGui::SliderFloat("Fov", &fov, 10.f, 100.f) )
      {
        cam.SetFOV(fov);
        _SceneCameraModified = true;
      }
    }

    if ( ImGui::CollapsingHeader("Lights") )
    {
      if ( ImGui::ListBoxHeader("##Lights") )
      {
        for (int i = 0; i < _Scene -> GetNbLights(); i++)
        {
          std::string lightName("Light#");
          lightName += std::to_string(i);

          bool is_selected = ( _SelectedLight == i );
          if (ImGui::Selectable(lightName.c_str(), is_selected))
          {
            _SelectedLight = i;
          }
        }
        ImGui::ListBoxFooter();
      }

      if (ImGui::Button("Add light"))
      {
        Light newLight;
        _Scene -> AddLight(newLight);
        _SelectedLight = _Scene -> GetNbLights() - 1;
      }

      if ( _SelectedLight >= 0 )
      {
        Light * curLight = _Scene -> GetLight(_SelectedLight);
        if ( curLight )
        {
          const char * LightTypes[3] = { "Quad", "Sphere", "Distant" };

          int lightType = (int)curLight -> _Type;
          if ( ImGui::Combo("Type", &lightType, LightTypes, 3) )
          {
            if ( lightType != (int)curLight -> _Type )
            {
              curLight -> _Type = (float)lightType;
              _SceneLightsModified = true;
            }
          }

          float pos[3] = { curLight -> _Pos.x, curLight -> _Pos.y, curLight -> _Pos.z };
          if ( ImGui::InputFloat3("Position", pos) )
          {
            curLight -> _Pos.x = pos[0];
            curLight -> _Pos.y = pos[1];
            curLight -> _Pos.z = pos[2];
            _SceneLightsModified = true;
          }

          float rgb[3] = { curLight -> _Emission.r, curLight -> _Emission.g, curLight -> _Emission.b };
          if ( ImGui::ColorEdit3("Emission", rgb) )
          {
            curLight -> _Emission.r = rgb[0];
            curLight -> _Emission.g = rgb[1];
            curLight -> _Emission.b = rgb[2];
            _SceneLightsModified = true;
          }

          if ( LightType::SphereLight == (LightType) lightType )
          {
            if ( ImGui::SliderFloat("Light radius", &curLight -> _Radius, 0.001f, 1.f) )
            {
              curLight -> _Area = 4.0f * M_PI * curLight -> _Radius * curLight -> _Radius;
              _SceneLightsModified = true;
            }
          }
          else if ( LightType::RectLight == (LightType) lightType )
          {
            float dirU[3] = { curLight -> _DirU.x, curLight -> _DirU.y, curLight -> _DirU.z };
            if ( ImGui::InputFloat3("DirU", dirU) )
            {
              curLight -> _DirU.x = dirU[0];
              curLight -> _DirU.y = dirU[1];
              curLight -> _DirU.z = dirU[2];
              curLight -> _Area = glm::length(glm::cross(curLight -> _DirU, curLight -> _DirV));
              _SceneLightsModified = true;
            }

            float dirV[3] = { curLight -> _DirV.x, curLight -> _DirV.y, curLight -> _DirV.z };
            if ( ImGui::InputFloat3("DirV", dirV) )
            {
              curLight -> _DirV.x = dirV[0];
              curLight -> _DirV.y = dirV[1];
              curLight -> _DirV.z = dirV[2];
              curLight -> _Area = glm::length(glm::cross(curLight -> _DirU, curLight -> _DirV));
              _SceneLightsModified = true;
            }
          }

          if ( ImGui::Checkbox("Show lights", &_Settings._ShowLights) )
            _SceneLightsModified = true;
        }
      }
    }

    if ( ImGui::CollapsingHeader("Materials") )
    {
      std::vector<Material> & Materials =  _Scene -> GetMaterials();
      std::vector<Texture*> & Textures = _Scene -> GetTextures();

      bool newMaterial = false;
      if ( ImGui::ListBoxHeader("##MaterialNames") )
      {
        for (int i = 0; i < _MaterialNames.size(); i++)
        {
          bool is_selected = ( _SelectedMaterial == i );
          if (ImGui::Selectable(_MaterialNames[i].c_str(), is_selected))
          {
            _SelectedMaterial = i;
            newMaterial = true;
          }
        }
        ImGui::ListBoxFooter();
      }

      if ( _SelectedMaterial >= 0 )
      {
        Material & curMat = Materials[_SelectedMaterial];

        float rgb[3] = { curMat._Albedo.r, curMat._Albedo.g, curMat._Albedo.b };
        if ( ImGui::ColorEdit3("Albedo", rgb) )
        {
          curMat._Albedo.r = rgb[0];
          curMat._Albedo.g = rgb[1];
          curMat._Albedo.b = rgb[2];
          _SceneMaterialsModified = true;
        }

        rgb[0] = curMat._Emission.r, rgb[1] = curMat._Emission.g, rgb[2] = curMat._Emission.b;
        if ( ImGui::ColorEdit3("Emission", rgb) )
        {
          curMat._Emission.r = rgb[0];
          curMat._Emission.g = rgb[1];
          curMat._Emission.b = rgb[2];
          _SceneMaterialsModified = true;
        }

        if ( ImGui::SliderFloat("Metallic", &curMat._Metallic, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Roughness", &curMat._Roughness, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Reflectance", &curMat._Reflectance, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Subsurface", &curMat._Subsurface, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Sheen Tint", &curMat._SheenTint, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Anisotropic", &curMat._Anisotropic, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Specular Trans", &curMat._SpecTrans, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Specular Tint", &curMat._SpecularTint, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Clearcoat", &curMat._Clearcoat, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Clearcoat Gloss", &curMat._ClearcoatGloss, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("IOR", &curMat._IOR, 1.f, 3.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat("Opacity", &curMat._Opacity, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( curMat._BaseColorTexId >= 0 )
        {
          Texture * basecolorTexture = Textures[curMat._BaseColorTexId];
          if ( basecolorTexture )
          {
            static GLuint S_ImGUI_TexID = 0;
            if ( newMaterial )
              LoadTexture(_IMGUIMateralialTextureUnit, basecolorTexture, S_ImGUI_TexID);

            if ( S_ImGUI_TexID )
            {
              ImGui::Text("Base color :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(S_ImGUI_TexID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }

        if ( curMat._MetallicRoughnessTexID >= 0 )
        {
          Texture * metallicRoughnessTexture = Textures[curMat._MetallicRoughnessTexID];
          if ( metallicRoughnessTexture )
          {
            static GLuint S_ImGUI_TexID = 0;
            if ( newMaterial )
              LoadTexture(_IMGUIMateralialTextureUnit, metallicRoughnessTexture, S_ImGUI_TexID);

            if ( S_ImGUI_TexID )
            {
              ImGui::Text("Metallic Roughness :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(S_ImGUI_TexID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }

        if ( curMat._NormalMapTexID >= 0 )
        {
          Texture * normalMapTexture = Textures[curMat._NormalMapTexID];
          if ( normalMapTexture )
          {
            static GLuint S_ImGUI_TexID = 0;
            if ( newMaterial )
              LoadTexture(_IMGUIMateralialTextureUnit, normalMapTexture, S_ImGUI_TexID);

            if ( S_ImGUI_TexID )
            {
              ImGui::Text("Normal map :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(S_ImGUI_TexID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }

        if ( curMat._EmissionMapTexID >= 0 )
        {
          Texture * emissionMapTexture = Textures[curMat._EmissionMapTexID];
          if ( emissionMapTexture )
          {
            static GLuint S_ImGUI_TexID = 0;
            if ( newMaterial )
              LoadTexture(_IMGUIMateralialTextureUnit, emissionMapTexture, S_ImGUI_TexID);

            if ( S_ImGUI_TexID )
            {
              ImGui::Text("Emission map :");
              ImTextureID texture = (ImTextureID)static_cast<uintptr_t>(S_ImGUI_TexID);
              ImGui::Image(texture, ImVec2(256, 256));
            }
          }
        }
      }
    }

    if ( _PrimitiveNames.size() && ImGui::CollapsingHeader("Primitives") )
    {
      std::vector<Primitive*>        & Primitives         = _Scene -> GetPrimitives();
      std::vector<PrimitiveInstance> & PrimitiveInstances = _Scene -> GetPrimitiveInstances();

      bool lookAtPrimitive = false;
      if ( ImGui::ListBoxHeader("##PrimitivesNames") )
      {
        for (int i = 0; i < _PrimitiveNames.size(); i++)
        {
          bool is_selected = ( _SelectedPrimitive == i );
          if (ImGui::Selectable(_PrimitiveNames[i].c_str(), is_selected))
          {
            _SelectedPrimitive = i;
            lookAtPrimitive = true;
          }
        }
        ImGui::ListBoxFooter();
      }

      if ( _SelectedPrimitive >= 0 )
      {
        PrimitiveInstance & primInstance = PrimitiveInstances[_SelectedPrimitive];
        Primitive * prim = Primitives[primInstance._PrimID];

        if ( lookAtPrimitive )
        {
          Vec3 scale;
          glm::quat rotation;
          Vec3 translation;
          MathUtil::Decompose(primInstance._Transform, translation, rotation, scale);

          Camera & cam = _Scene -> GetCamera();
          cam.LookAt(translation);
          _SceneCameraModified = true;
        }

        Vec3 scale;
        glm::quat rotation;
        Vec3 translation;
        MathUtil::Decompose(primInstance._Transform, translation, rotation, scale);

        // Translation
        float trans[3] = { translation.x, translation.y, translation.z };
        if ( ImGui::InputFloat3("Translation (X,Y,Z)", trans) )
        {
          translation.x = trans[0];
          translation.y = trans[1];
          translation.z = trans[2];
          _SceneInstancesModified = true;
        }

        if ( prim )
        {
          if ( PrimitiveType::Box == prim -> _Type )
          {
            // Rotation
            Vec3 eulerAngles = glm::eulerAngles(rotation); // pitch, yaw, roll
            eulerAngles *= 180.f / M_PI;
            int YawPitchRoll[3] = { (int)roundf(eulerAngles.y), (int)roundf(eulerAngles.x), (int)roundf(eulerAngles.z) };

            if ( ImGui::SliderInt("Yaw", &YawPitchRoll[0], -89, 89) )
              _SceneInstancesModified = true;
            if ( ImGui::SliderInt("Pitch", &YawPitchRoll[1], -179, 179) )
              _SceneInstancesModified = true;
            if ( ImGui::SliderInt("Roll", &YawPitchRoll[2], -179, 179) )
              _SceneInstancesModified = true;

            if ( _SceneInstancesModified )
            {
              eulerAngles.x = YawPitchRoll[1] * ( M_PI / 180.f );
              eulerAngles.y = YawPitchRoll[0] * ( M_PI / 180.f );
              eulerAngles.z = YawPitchRoll[2] * ( M_PI / 180.f );

              rotation = glm::quat(eulerAngles);
            }
          }
          else if ( PrimitiveType::Sphere == prim -> _Type )
          {
            float radius = ((Sphere*)prim) -> _Radius;
            if ( ImGui::SliderFloat("Radius", &radius, .1f, 100.f) )
            {
              ((Sphere*)prim) -> _Radius = radius;
              _SceneInstancesModified = true;
            }
          }
        }

        if ( _SceneInstancesModified )
        {
          Mat4x4 translastionMatrix = glm::translate(Mat4x4(1.f), translation);
          Mat4x4 rotationMatrix = glm::toMat4(rotation);
          Mat4x4 scaleMatrix = glm::scale(Mat4x4(1.f), scale);
          primInstance._Transform = translastionMatrix * rotationMatrix * scaleMatrix;
        }

        {
          int matID = primInstance._MaterialID;
          if ( ImGui::Combo("MaterialNames", &matID, &VectorOfStringGetter, &_MaterialNames, _MaterialNames.size()) )
          {
            if ( matID >= 0 )
            {
              primInstance._MaterialID = matID;
              _SceneInstancesModified = true;
            }
          }
        }
      }
    }

    ImGui::End();
  }

  // Rendering
  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test3::UpdateCPUTime()
{
  double oldCpuTime = _CPULoopTime;
  _CPULoopTime = glfwGetTime();

  _TimeDelta = _CPULoopTime - oldCpuTime;
  oldCpuTime = _CPULoopTime;

  _AccuDelta += _TimeDelta;
  _NbFrames++;

  double nbSeconds = 0.;
  while ( _AccuDelta > 1. )
  {
    _AccuDelta -= 1.;
    nbSeconds++;
  }

  if ( nbSeconds >= 1. )
  {
    nbSeconds += _AccuDelta;

    _FrameRate = (double)_NbFrames / nbSeconds;
    _NbFrames = 0;
  }

  if ( _AverageDelta == 0. )
    _AverageDelta = _TimeDelta;
  else
    _AverageDelta = _TimeDelta * .25 + _AverageDelta * .75;

  return 0;
}

// ----------------------------------------------------------------------------
// AdjustRenderScale
// ----------------------------------------------------------------------------
void Test3::AdjustRenderScale()
{
  if ( !_Settings._AutoScale )
    return;

  int newScale = _Settings._RenderScale;

  float diff = _FrameRate - _Settings._TargetFPS;
  if ( abs(diff) > ( _Settings._TargetFPS * .05f ) )
  {
    double timeSinceLastAdjust = _CPULoopTime - _LastAdjustmentTime;

    if ( timeSinceLastAdjust > 0.25f )
    {
      if ( diff < -5.f )
        newScale -= std::min((int)floor(abs(diff)), 5);
      else
        newScale += MathUtil::Sign(diff) * 1;

      newScale = MathUtil::Clamp(newScale, 25, 200);

      _LastAdjustmentTime = _CPULoopTime;
    }
  }

  if ( newScale != _Settings._RenderScale )
  {
    _Settings._RenderScale = newScale;
    _RenderSettingsModified = true;

    glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test3::InitializeScene()
{
  if ( _Scene )
    ClearSceneData();

  if ( !Loader::LoadScene(_SceneFiles[_CurSceneId], _Scene, _Settings) || !_Scene )
  {
    std::cout << "Failed to load scene : " << _SceneFiles[_CurSceneId] << std::endl;
    return 1;
  }

  // Scene should contain at least one light
  Light * firstLight = _Scene -> GetLight(0);
  if ( !firstLight )
  {
    Light newLight;
    _Scene -> AddLight(newLight);
  }

  // Test - Load first mesh
  _Scene -> CompileMeshData( _Settings._TextureSize );

  _NbTriangles = _Scene -> GetNbFaces();
  _NbMeshInstances = _Scene -> GetNbMeshInstances();
  if ( _NbTriangles )
  {
    glPixelStorei(GL_PACK_ALIGNMENT, 1); // ???

    glGenBuffers(1, &_VtxBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _VtxBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene -> GetVertices().size(), &_Scene -> GetVertices()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_VtxTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _VtxTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _VtxBufferID);

    glGenBuffers(1, &_VtxNormBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _VtxNormBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene -> GetNormals().size(), &_Scene -> GetNormals()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_VtxNormTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _VtxNormTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _VtxNormBufferID);

    if ( _Scene -> GetUVMatID().size() )
    {
      glGenBuffers(1, &_VtxUVBufferID);
      glBindBuffer(GL_TEXTURE_BUFFER, _VtxUVBufferID);
      glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene -> GetUVMatID().size(), &_Scene -> GetUVMatID()[0], GL_STATIC_DRAW);
      glGenTextures(1, &_VtxUVTextureID);
      glBindTexture(GL_TEXTURE_BUFFER, _VtxUVTextureID);
      glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _VtxUVBufferID);
    }

    glGenBuffers(1, &_VtxIndBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _VtxIndBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3i) * _Scene -> GetIndices().size(), &_Scene -> GetIndices()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_VtxIndTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _VtxIndTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32I, _VtxIndBufferID);

    if ( _Scene -> GetTextureArrayIDs().size() )
    {
      glGenBuffers(1, &_TexIndBufferID);
      glBindBuffer(GL_TEXTURE_BUFFER, _TexIndBufferID);
      glBufferData(GL_TEXTURE_BUFFER, sizeof(int) * _Scene -> GetTextureArrayIDs().size(), &_Scene -> GetTextureArrayIDs()[0], GL_STATIC_DRAW);
      glGenTextures(1, &_TexIndTextureID);
      glBindTexture(GL_TEXTURE_BUFFER, _TexIndTextureID);
      glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, _TexIndBufferID);

      glGenTextures(1, &_TexArrayTextureID);
      glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTextureID);
      glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, _Settings._TextureSize.x, _Settings._TextureSize.y, _Scene -> GetNbCompiledTex(), 0, GL_RGBA, GL_UNSIGNED_BYTE, &_Scene -> GetTextureArray()[0]);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }

    glGenBuffers(1, &_MeshBBoxBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _MeshBBoxBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene -> GetMeshBBoxes().size(), &_Scene -> GetMeshBBoxes()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_MeshBBoxTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _MeshBBoxTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _MeshBBoxBufferID);

    glGenBuffers(1, &_MeshIdRangeBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _MeshIdRangeBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(int) * _Scene -> GetMeshIdxRange().size(), &_Scene -> GetMeshIdxRange()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_MeshIdRangeTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _MeshIdRangeTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, _MeshIdRangeBufferID);

    glGenTextures(1, &_MaterialsTextureID);
    glBindTexture(GL_TEXTURE_2D, _MaterialsTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Material) / sizeof(Vec4)) * _Scene -> GetMaterials().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene -> GetMaterials()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    // BVH
    glGenBuffers(1, &_TLASNodesBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _TLASNodesBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GpuBvh::Node) * _Scene -> GetTLASNode().size(), &_Scene -> GetTLASNode()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_TLASNodesTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _TLASNodesTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _TLASNodesBufferID);

    glGenTextures(1, &_TLASTransformsIDTextureID);
    glBindTexture(GL_TEXTURE_2D, _TLASTransformsIDTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, (sizeof(Mat4x4) / sizeof(Vec4)) * _Scene -> GetTLASPackedTransforms().size(), 1, 0, GL_RGBA, GL_FLOAT, &_Scene -> GetTLASPackedTransforms()[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &_TLASMeshMatIDBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _TLASMeshMatIDBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec2i) * _Scene -> GetTLASPackedMeshMatID().size(), &_Scene -> GetTLASPackedMeshMatID()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_TLASMeshMatIDTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _TLASMeshMatIDTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32I, _TLASMeshMatIDBufferID);

    glGenBuffers(1, &_BLASNodesBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASNodesBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(GpuBvh::Node) * _Scene -> GetBLASNode().size(), &_Scene -> GetBLASNode()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASNodesTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASNodesTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _BLASNodesBufferID);

    glGenBuffers(1, &_BLASNodesRangeBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASNodesRangeBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec2i) * _Scene -> GetBLASNodeRange().size(), &_Scene -> GetBLASNodeRange()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASNodesRangeTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASNodesRangeTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32I, _BLASNodesRangeBufferID);

    glGenBuffers(1, &_BLASPackedIndicesBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASPackedIndicesBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3i) * _Scene -> GetBLASPackedIndices().size(), &_Scene -> GetBLASPackedIndices()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASPackedIndicesTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedIndicesTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32I, _BLASPackedIndicesBufferID);

    glGenBuffers(1, &_BLASPackedIndicesRangeBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASPackedIndicesRangeBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec2i) * _Scene -> GetBLASPackedIndicesRange().size(), &_Scene -> GetBLASPackedIndicesRange()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASPackedIndicesRangeTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedIndicesRangeTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32I, _BLASPackedIndicesRangeBufferID);

    glGenBuffers(1, &_BLASPackedVerticesBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASPackedVerticesBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene -> GetBLASPackedVertices().size(), &_Scene -> GetBLASPackedVertices()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASPackedVerticesTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedVerticesTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _BLASPackedVerticesBufferID);

    glGenBuffers(1, &_BLASPackedNormalsBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASPackedNormalsBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec3) * _Scene -> GetBLASPackedNormals().size(), &_Scene -> GetBLASPackedNormals()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASPackedNormalsTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedNormalsTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGB32F, _BLASPackedNormalsBufferID);

    glGenBuffers(1, &_BLASPackedUVsBufferID);
    glBindBuffer(GL_TEXTURE_BUFFER, _BLASPackedUVsBufferID);
    glBufferData(GL_TEXTURE_BUFFER, sizeof(Vec2) * _Scene -> GetBLASPackedUVs().size(), &_Scene -> GetBLASPackedUVs()[0], GL_STATIC_DRAW);
    glGenTextures(1, &_BLASPackedUVsTextureID);
    glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedUVsTextureID);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RG32F, _BLASPackedUVsBufferID);

  }

  const std::vector<Material> & Materials =  _Scene -> GetMaterials();
  for ( auto & mat : Materials )
    _MaterialNames.push_back(_Scene -> FindMaterialName(mat._ID));

  const std::vector<PrimitiveInstance> & PrimitiveInstances = _Scene -> GetPrimitiveInstances();
  for ( int i = 0; i < PrimitiveInstances.size(); ++i )
    _PrimitiveNames.push_back(_Scene -> FindPrimitiveName(i));

  if ( !_Scene -> GetEnvMap().IsInitialized() )
  {
    if ( _CurBackgroundId > 0 )
    {
      _Scene->LoadEnvMap( _BackgroundFiles[_CurBackgroundId] );
      _Settings._EnableSkybox = false;
    }
  }

  if ( _Scene -> GetEnvMap().IsInitialized() )
  {
    glGenTextures(1, &_SkyboxTextureID);
    glActiveTexture(TEX_UNIT(_SkyboxTextureUnit));
    glBindTexture(GL_TEXTURE_2D, _SkyboxTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, _Scene -> GetEnvMap().GetWidth(), _Scene -> GetEnvMap().GetHeight(), 0, GL_RGB, GL_FLOAT, _Scene -> GetEnvMap().GetRawData());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
  else
    _Settings._EnableSkybox = false;

  _RenderSettingsModified = true;
  _SceneCameraModified    = true;
  _SceneLightsModified    = true;
  _SceneMaterialsModified = true;
  _SceneInstancesModified = true;

  _ReloadScene = false;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateScene
// ----------------------------------------------------------------------------
int Test3::UpdateScene()
{
  if ( _ReloadScene )
  {
    if ( 0 != InitializeScene() )
      return 1;

    glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

    glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
  if ( !_Scene )
    return 1;

  if ( _ReloadBackground )
  {
    if ( _CurBackgroundId > 0 )
    {
      _Scene -> LoadEnvMap( _BackgroundFiles[_CurBackgroundId] );

      if ( _Scene -> GetEnvMap().IsInitialized() )
      {
        if ( 0 == _SkyboxTextureID )
          glGenTextures( 1, &_SkyboxTextureID );
        glActiveTexture( TEX_UNIT( _SkyboxTextureUnit ) );
        glBindTexture( GL_TEXTURE_2D, _SkyboxTextureID );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB32F, _Scene -> GetEnvMap().GetWidth(), _Scene -> GetEnvMap().GetHeight(), 0, GL_RGB, GL_FLOAT, _Scene -> GetEnvMap().GetRawData() );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glBindTexture( GL_TEXTURE_2D, 0 );
      }
    }

    _ReloadBackground = false;
  }

  // Mouse input
  {
    const float MouseSensitivity[5] = { 1.f, 0.5f, 0.01f, 0.01f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ZoomIn

    glfwGetCursorPos(_MainWindow.get(), &_MouseX, &_MouseY);

    double deltaX = _MouseX - _OldMouseX;
    double deltaY = _MouseY - _OldMouseY;

    if ( _LeftClick )
    {
      _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);
      _SceneCameraModified = true;
    }
    if ( _RightClick )
    {
      _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, MouseSensitivity[3] * deltaY);
      _SceneCameraModified = true;
    }
    if ( _MiddleClick )
    {
      if ( std::abs(deltaX) > std::abs(deltaY) )
        _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, 0.f);
      else
      {
        float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[4] * deltaY;
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
      _SceneCameraModified = true;
    }

    _OldMouseX = _MouseX;
    _OldMouseY = _MouseY;
  }

  // Keyboard input
  {
    const float velocity = 100.f;

    if ( _KeyUp )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() - _TimeDelta;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _SceneCameraModified = true;
      }
    }
    if ( _KeyDown )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() + _TimeDelta;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
        _SceneCameraModified = true;
      }
    }
    if ( _KeyLeft )
    {
      _Scene -> GetCamera().OffsetOrientations(_TimeDelta * velocity, 0.f);
      _SceneCameraModified = true;
    }
    if ( _KeyRight )
    {
      _Scene -> GetCamera().OffsetOrientations(-_TimeDelta * velocity, 0.f);
      _SceneCameraModified = true;
    }

    if ( _KeyR )
    {
      _ReloadScene = true;
      _KeyR = false;
    }
  }

  bool updateFrameBufferSize = false;
  if ( !_Settings._AutoScale )
  {
    if ( _SceneCameraModified || _RenderSettingsModified )
    {
      if ( ( 25 != _Settings._RenderScale ) && ( 25 != _RealRenderScale ) )
      {
        _RealRenderScale = _Settings._RenderScale;
        _Settings._RenderScale = 25;
        updateFrameBufferSize = true;
      }
    }
    else if ( _Settings._RenderScale != _RealRenderScale )
    {
      _Settings._RenderScale = _RealRenderScale;
      updateFrameBufferSize = true;
    }
  }

  if ( updateFrameBufferSize )
  {
    _RenderSettingsModified = true;

    glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
void Test3::RenderToTexture()
{
  glBindFramebuffer(GL_FRAMEBUFFER, _FrameBufferID);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  glActiveTexture(TEX_UNIT(_ScreenTextureUnit));
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glActiveTexture(TEX_UNIT(_SkyboxTextureUnit));
  glBindTexture(GL_TEXTURE_2D, _SkyboxTextureID);
  glActiveTexture(TEX_UNIT(_VtxTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxTextureID);
  glActiveTexture(TEX_UNIT(_VtxNormTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxNormTextureID);
  glActiveTexture(TEX_UNIT(_VtxUVTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxUVTextureID);
  glActiveTexture(TEX_UNIT(_VtxIndTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _VtxIndTextureID);
  glActiveTexture(TEX_UNIT(_TexIndTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _TexIndTextureID);
  glActiveTexture(TEX_UNIT(_TexArrayTextureUnit));
  glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTextureID);
  glActiveTexture(TEX_UNIT(_MeshBBoxTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _MeshBBoxTextureID);
  glActiveTexture(TEX_UNIT(_MeshIdRangeTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _MeshIdRangeTextureID);
  glActiveTexture(TEX_UNIT(_MaterialsTextureUnit));
  glBindTexture(GL_TEXTURE_2D, _MaterialsTextureID);
  glActiveTexture(TEX_UNIT(_TLASNodesTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _TLASNodesTextureID);
  glActiveTexture(TEX_UNIT(_TLASTransformsIDTextureUnit));
  glBindTexture(GL_TEXTURE_2D, _TLASTransformsIDTextureID);
  glActiveTexture(TEX_UNIT(_TLASMeshMatIDTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _TLASMeshMatIDTextureID);
  glActiveTexture(TEX_UNIT(_BLASNodesTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASNodesTextureID);
  glActiveTexture(TEX_UNIT(_BLASNodesRangeTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASNodesRangeTextureID);
  glActiveTexture(TEX_UNIT(_BLASPackedIndicesTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedIndicesTextureID);
  glActiveTexture(TEX_UNIT(_BLASPackedIndicesRangeTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedIndicesRangeTextureID);
  glActiveTexture(TEX_UNIT(_BLASPackedVerticesTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedVerticesTextureID);
  glActiveTexture(TEX_UNIT(_BLASPackedNormalsTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedNormalsTextureID);
  glActiveTexture(TEX_UNIT(_BLASPackedUVsTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, _BLASPackedUVsTextureID);
  
  _Quad -> Render(*_RTTShader);
}

// ----------------------------------------------------------------------------
// RenderToSceen
// ----------------------------------------------------------------------------
void Test3::RenderToSceen()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glActiveTexture(TEX_UNIT(_ScreenTextureUnit));
  
  glViewport(0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);

  _Quad -> Render(*_RTSShader);
}

// ----------------------------------------------------------------------------
// RenderToFile
// ----------------------------------------------------------------------------
bool Test3::RenderToFile( const std::filesystem::path & iFilepath )
{
  // Generate output texture
  GLuint frameCaptureTextureID = 0;
  glGenTextures( 1, &frameCaptureTextureID );
  glActiveTexture( TEX_UNIT(_TemporaryTextureUnit) );
  glBindTexture( GL_TEXTURE_2D, frameCaptureTextureID );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glBindTexture( GL_TEXTURE_2D, 0 );

  // FrameBuffer
  GLuint frameBufferID = 0;
  glGenFramebuffers( 1, &frameBufferID );
  glBindFramebuffer( GL_FRAMEBUFFER, frameBufferID );
  glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frameCaptureTextureID, 0 );
  if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
  {
    glDeleteTextures( 1, &frameCaptureTextureID );
    std::cout << "ERROR : Failed to save capture in " << iFilepath.c_str() << std::endl;
    return false;
  }

  // Render to texture
  {
    glBindFramebuffer( GL_FRAMEBUFFER, frameBufferID );
    glViewport( 0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y );

    glActiveTexture( TEX_UNIT( _TemporaryTextureUnit ) );
    glBindTexture( GL_TEXTURE_2D, frameCaptureTextureID );
    glActiveTexture( TEX_UNIT( _ScreenTextureUnit ) );
    glBindTexture( GL_TEXTURE_2D, _ScreenTextureID );

    _Quad -> Render( *_RTSShader );

    glBindTexture( GL_TEXTURE_2D, 0 );
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
  }

  // Retrieve image et save to file
  int saved = 0;
  {
    int w = _Settings._RenderResolution.x;
    int h = _Settings._RenderResolution.y;
    unsigned char * frameData = new unsigned char[w * h * 4];

    glActiveTexture( TEX_UNIT( _TemporaryTextureUnit ) );
    glBindTexture( GL_TEXTURE_2D, frameCaptureTextureID );
    glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData );
    stbi_flip_vertically_on_write( true );
    saved = stbi_write_png( iFilepath.string().c_str(), w, h, 4, frameData, w * 4 );

    delete[] frameData;
    glDeleteFramebuffers( 1, &frameBufferID );
    glDeleteTextures( 1, &frameCaptureTextureID );
  }

  if ( saved && fs::exists( iFilepath ) )
    std::cout << "Frame saved in " << fs::absolute(iFilepath) << std::endl;
  else
    std::cout << "ERROR : Failed to save screen capture in " << fs::absolute( iFilepath ) << std::endl;

  return saved ? true : false;
}

// ----------------------------------------------------------------------------
// Run
// ----------------------------------------------------------------------------
int Test3::Run()
{
  int ret = 0;

  do
  {
    if ( !_MainWindow )
    {
      ret = 1;
      break;
    }

    glfwSetWindowTitle( _MainWindow.get(), GetTestHeader() );
    glfwSetWindowUserPointer( _MainWindow.get(), this );

    glfwSetFramebufferSizeCallback( _MainWindow.get(), Test3::FramebufferSizeCallback );
    glfwSetMouseButtonCallback( _MainWindow.get(), Test3::MousebuttonCallback );
    glfwSetKeyCallback( _MainWindow.get(), Test3::KeyCallback );

    glfwMakeContextCurrent( _MainWindow.get() );
    glfwSwapInterval( 1 ); // Enable vsync

    // Setup Dear ImGui context
    InitializeUI();

    // Init openGL scene
    glewExperimental = GL_TRUE;
    if ( glewInit() != GLEW_OK )
    {
      std::cout << "Failed to initialize GLEW!" << std::endl;
      ret = 1;
      break;
    }

    // Shader compilation
    if ( ( 0 != RecompileShaders() ) || !_RTTShader || !_RTSShader )
    {
      std::cout << "Shader compilation failed!" << std::endl;
      ret = 1;
      break;
    }

    // Quad
    _Quad = new QuadMesh();

    // Frame buffer
    if ( 0 != InitializeFrameBuffer() )
    {
      std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
      ret = 1;
      break;
    }

    // Initialize the scene
    if ( ( 0 != InitializeSceneFiles() ) || ( 0 != InitializeBackgroundFiles() ) || ( 0 != InitializeScene() ) )
    {
      std::cout << "ERROR: Scene initialization failed!" << std::endl;
      ret = 1;
      break;
    }

    // Main loop
    glfwSetWindowSize( _MainWindow.get(), _Settings._RenderResolution.x, _Settings._RenderResolution.y );
    glViewport( 0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y );
    glDisable( GL_DEPTH_TEST );

    glBindTexture( GL_TEXTURE_2D, _ScreenTextureID );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL );
    glBindTexture( GL_TEXTURE_2D, 0 );

    _CPULoopTime = glfwGetTime();
    while ( !glfwWindowShouldClose( _MainWindow.get() ) && !_KeyEsc )
    {
      _FrameNum++;

      UpdateCPUTime();

      glfwPollEvents();

      if ( 0 != UpdateScene() )
        break;

      UpdateUniforms();

      // Render to texture
      RenderToTexture();

      // Render to screen
      RenderToSceen();

      // Screenshot
      if ( g_RenderToFile )
      {
        RenderToFile(g_FilePath);
        g_RenderToFile = false;
      }

      // UI
      DrawUI();

      glfwSwapBuffers( _MainWindow.get() );
    }
  } while ( 0 );

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return ret;
}

}
