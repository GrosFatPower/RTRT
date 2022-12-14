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
#include "Math.h"
#include "Loader.h"

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

namespace RTRT
{

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

// ----------------------------------------------------------------------------
// Global functions
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
    default :
      break;
    }
  }
}

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

Test3::Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  _Settings._RenderResolution.x = iScreenWidth;
  _Settings._RenderResolution.y = iScreenHeight;
}

Test3::~Test3()
{
  glDeleteFramebuffers(1, &_FrameBufferID);

  glDeleteBuffers(1, &_VtxBufferID);
  glDeleteBuffers(1, &_VtxNormBufferID);
  glDeleteBuffers(1, &_VtxUVBufferID);
  glDeleteBuffers(1, &_VtxIndBufferID);
  glDeleteBuffers(1, &_TexIndBufferID);

  glDeleteTextures(1, &_ScreenTextureID);
  glDeleteTextures(1, &_VtxTextureID);
  glDeleteTextures(1, &_VtxNormTextureID);
  glDeleteTextures(1, &_VtxUVTextureID);
  glDeleteTextures(1, &_VtxIndTextureID);
  glDeleteTextures(1, &_TexIndTextureID);
  glDeleteTextures(1, &_TexArrayTextureID);
  
  if (_Quad)
    delete _Quad;
  _Quad = nullptr;
  if (_RTTShader)
    delete _RTTShader;
  _RTTShader = nullptr;
  if (_RTSShader)
    delete _RTSShader;
  _RTSShader = nullptr;
  if ( _Scene )
    delete _Scene;
  _Scene = nullptr;

  for ( auto primName : _PrimitiveNames )
    delete primName;
}

int Test3::InitializeFrameBuffer()
{
  // Screen texture
  glGenTextures(1, &_ScreenTextureID);
  glActiveTexture(GL_TEXTURE0);
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

int Test3::RecompileShaders()
{
  if (_RTTShader)
    delete _RTTShader;
  _RTTShader = nullptr;
  if (_RTSShader)
    delete _RTSShader;
  _RTSShader = nullptr;

  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_BasicRT.glsl");

  _RTTShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !_RTTShader )
    return 1;

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  _RTSShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !_RTSShader )
    return 1;

  return 0;
}

int Test3::UpdateUniforms()
{
  if ( _RTTShader )
  {
    _RTTShader -> Use();
    GLuint RTTProgramID = _RTTShader -> GetShaderProgramID();
    glUniform2f(glGetUniformLocation(RTTProgramID, "u_Resolution"), RenderWidth(), RenderHeight());
    glUniform1f(glGetUniformLocation(RTTProgramID, "u_Time"), _CPULoopTime);
    glUniform1i(glGetUniformLocation(RTTProgramID, "u_FrameNum"), _FrameNum);  

    if ( !(_RenderSettingsModified + _SceneCameraModified + _SceneLightsModified + _SceneInstancesModified + _SceneMaterialsModified) )
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
      glUniform1f(glGetUniformLocation(RTTProgramID, "u_SkyboxRotation"), _Settings._SkyBoxRotation / 360.f);
      glUniform1f(glGetUniformLocation(RTTProgramID, "u_Gamma"), _Settings._Gamma);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_ScreenTexture"), 0);
      glUniform1i(glGetUniformLocation(RTTProgramID, "u_SkyboxTexture"), 1);
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
        Light * firstLight = _Scene -> GetLight(0);
        {
          glUniform3f(glGetUniformLocation(RTTProgramID, "u_SphereLight._Pos"), firstLight -> _Pos.x, firstLight -> _Pos.y, firstLight -> _Pos.z);
          glUniform3f(glGetUniformLocation(RTTProgramID, "u_SphereLight._Emission"), firstLight -> _Emission.r, firstLight -> _Emission.g, firstLight -> _Emission.b);
          glUniform1f(glGetUniformLocation(RTTProgramID, "u_SphereLight._Radius"), firstLight -> _Radius);
        }
        _SceneLightsModified = false;
      }

      if ( _SceneMaterialsModified )
      {
        const std::vector<Material> & Materials =  _Scene -> GetMaterials();

        int nbMaterials = Materials.size();
        for ( int i = 0; i < nbMaterials; ++i )
        {
          const Material & curMat = Materials[i];

          glUniform1i(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_ID"             ).c_str()), i);
          glUniform1i(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_BaseColorTexID" ).c_str()), (int)curMat._BaseColorTexId );
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Albedo"         ).c_str()), curMat._Albedo.r, curMat._Albedo.g, curMat._Albedo.b);
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Emission"       ).c_str()), curMat._Emission.r, curMat._Emission.g, curMat._Emission.b);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Metallic"       ).c_str()), curMat._Metallic);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Roughness"      ).c_str()), curMat._Roughness);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_IOR"            ).c_str()), curMat._IOR);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Opacity"        ).c_str()), curMat._Opacity);
        }
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbMaterials"), nbMaterials);

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
        for ( auto prim : PrimitiveInstances )
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

        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxTexture"),      2);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxNormTexture"),  3);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxUVTexture"),    4);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_VtxIndTexture"),   5);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TexIndTexture"),   6);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_TexArrayTexture"), 7);

        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbSpheres"), nbSpheres);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbPlanes"), nbPlanes);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbBoxes"), nbBoxes);
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbTriangles"), _NbTriangles);

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
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_ScreenTexture"), 0);
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_AccumulatedFrames"), _AccumulatedFrames);
    _RTSShader -> StopUsing();
  }
  else
    return 1;

  return 0;
}

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
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  return 0;
}

int Test3::DrawUI()
{
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  {
    ImGui::Begin("Test 3");

    ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);

    ImGui::Text("Window width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);

    ImGui::Text("Accumulated frames : %d", _AccumulatedFrames);

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

      if ( ImGui::SliderFloat("Gamma", &_Settings._Gamma, .5f, 3.f) )
        _RenderSettingsModified = true;

      if ( ImGui::Checkbox("Enable skybox", &_Settings._EnableSkybox) )
        _RenderSettingsModified = true;

      if ( ImGui::SliderFloat("Skybox rotation", &_Settings._SkyBoxRotation, 0.f, 360.f) )
        _RenderSettingsModified = true;

      float rgb[3] = { _Settings._BackgroundColor.r, _Settings._BackgroundColor.g, _Settings._BackgroundColor.b };
      if ( ImGui::ColorEdit3("Background", rgb) )
      {
        _Settings._BackgroundColor = { rgb[0], rgb[1], rgb[2] };
        _RenderSettingsModified = true;
      }
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

    if ( ImGui::CollapsingHeader("Light") )
    {
      Light * firstLight = _Scene -> GetLight(0);

      if ( firstLight )
      {
        float pos[3] = { firstLight -> _Pos.x, firstLight -> _Pos.y, firstLight -> _Pos.z };
        if ( ImGui::InputFloat3("Position", pos) )
        {
          firstLight -> _Pos.x = pos[0];
          firstLight -> _Pos.y = pos[1];
          firstLight -> _Pos.z = pos[2];
          _SceneLightsModified = true;
        }

        float rgb[3] = { firstLight -> _Emission.r, firstLight -> _Emission.g, firstLight -> _Emission.b };
        if ( ImGui::ColorEdit3("Emission", rgb) )
        {
          firstLight -> _Emission.r = rgb[0];
          firstLight -> _Emission.g = rgb[1];
          firstLight -> _Emission.b = rgb[2];
          _SceneLightsModified = true;
        }

        if ( ImGui::SliderFloat("Light radius", &firstLight -> _Radius, 0.001f, 1.f) )
          _SceneLightsModified = true;
      }
    }

    if ( ImGui::CollapsingHeader("Materials") )
    {
      std::vector<Material> & Materials =  _Scene -> GetMaterials();

      int nbMaterials = Materials.size();
      for ( int i = 0; i < nbMaterials; ++i )
      {
        Material & curMat = Materials[i];

        float rgb[3] = { curMat._Albedo.r, curMat._Albedo.g, curMat._Albedo.b };
        ImGui::Text(_Scene -> FindMaterialName(i).c_str());
        ImGui::SameLine();
        if ( ImGui::ColorEdit3(std::string("##MatAlbedo").append(std::to_string(i)).c_str(), rgb) )
        {
          curMat._Albedo.r = rgb[0];
          curMat._Albedo.g = rgb[1];
          curMat._Albedo.b = rgb[2];
          _SceneMaterialsModified = true;
        }

        if ( ImGui::SliderFloat(std::string("Metallic_").append(std::to_string(i)).c_str(), &curMat._Metallic, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat(std::string("Roughness_").append(std::to_string(i)).c_str(), &curMat._Roughness, 0.f, 1.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat(std::string("IOR_").append(std::to_string(i)).c_str(), &curMat._IOR, 1.f, 3.f) )
          _SceneMaterialsModified = true;

        if ( ImGui::SliderFloat(std::string("Opacity_").append(std::to_string(i)).c_str(), &curMat._Opacity, 0.f, 1.f) )
          _SceneMaterialsModified = true;
      }
    }

    if ( _PrimitiveNames.size() && ImGui::CollapsingHeader("Primitives") )
    {
      std::vector<Primitive*>        & Primitives         = _Scene -> GetPrimitives();
      std::vector<PrimitiveInstance> & PrimitiveInstances = _Scene -> GetPrimitiveInstances();
      std::vector<Material>          & Materials          = _Scene -> GetMaterials();

      int nbMaterial = Materials.size();

      std::vector<const char*> PrimitiveNamesCSTR;
      for ( auto primName : _PrimitiveNames )
        PrimitiveNamesCSTR.push_back(primName -> c_str());

      if ( ImGui::Combo("PrimitiveInstances", &_SelectedPrimitive, PrimitiveNamesCSTR.data(), PrimitiveNamesCSTR.size()) )
      {
        PrimitiveInstance & primInstance = PrimitiveInstances[_SelectedPrimitive];

        Vec3 scale;
        glm::quat rotation;
        Vec3 translation;
        MathUtil::Decompose(primInstance._Transform, translation, rotation, scale);

        Camera & cam = _Scene -> GetCamera();
        cam.LookAt(translation);
        _SceneCameraModified = true;
      }

      if ( _SelectedPrimitive >= 0 )
      {
        PrimitiveInstance & primInstance = PrimitiveInstances[_SelectedPrimitive];
        Primitive * prim = Primitives[primInstance._PrimID];

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
          static char * MaterialNames[99] = { NULL };
          static int nbMaterials = 0;

          if ( _SceneMaterialsModified || !nbMaterials )
          {
            nbMaterials = 0;
            for ( int i = 0; i < 99; ++i )
            {
              if ( MaterialNames[i] )
                delete[] MaterialNames[i];
            }

            for ( auto mat : Materials )
            {
              std::string materialName = _Scene ->FindMaterialName((int)mat._ID);
              MaterialNames[nbMaterials] = new char[materialName.size()+1];
              strcpy(MaterialNames[nbMaterials], materialName.c_str());
              nbMaterials++;
              if ( nbMaterials >= 99 )
                break;
            }
          }

          int matID = primInstance._MaterialID;
          if ( ImGui::Combo("MaterialNames", &matID, MaterialNames, nbMaterials) )
          {
            if ( matID >= 0 )
            {
              primInstance._MaterialID = matID;
              _SceneInstancesModified = true;
            }
          }

          // MLK
          //for ( int i = 0; i < 99; ++i )
          //{
          //  if ( MaterialNames[i] )
          //    delete[] MaterialNames[i];
          //}
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

int Test3::UpdateCPUTime()
{
  double oldCpuTime = _CPULoopTime;
  _CPULoopTime = glfwGetTime();

  _TimeDelta = _CPULoopTime - oldCpuTime;
  oldCpuTime = _CPULoopTime;

  _LastDeltas.push_back(_TimeDelta);
  while ( _LastDeltas.size() > 30 )
    _LastDeltas.pop_front();
    
  double totalDelta = 0.;
  for ( auto delta : _LastDeltas )
    totalDelta += delta;
  _AverageDelta = totalDelta / _LastDeltas.size();

  if ( _AverageDelta > 0. )
    _FrameRate = 1. / (float)_AverageDelta;

  return 0;
}

void Test3::AdjustRenderScale()
{
  if ( !_Settings._AutoScale )
    return;
  if ( _LastDeltas.size() < 10 )
    return;

  int newScale = _Settings._RenderScale;

  float diff = _FrameRate - _Settings._TargetFPS;
  if ( abs(diff) > ( _Settings._TargetFPS * .05f ) )
  {
    if ( diff < -10.f )
      newScale -= std::min((int)floor(abs(diff)), 25);
    else if ( ( abs(diff) > 3.f ) && ( _LastDeltas.size() >= 20 ) )
      newScale += MathUtil::Sign(diff) * floor(diff);
    else
      newScale += MathUtil::Sign(diff) * 1;

    newScale = MathUtil::Clamp(newScale, 25, 200);
  }

  if ( newScale != _Settings._RenderScale )
  {
    _Settings._RenderScale = newScale;
    _LastDeltas.clear();
    _RenderSettingsModified = true;

    glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

int Test3::InitializeScene()
{
  if ( _Scene )
    delete _Scene;

  std::string sceneFile = "..\\..\\Assets\\BasicRT_Scene.scene";

  //_Scene = new Scene();
  if ( !Loader::LoadScene(sceneFile, _Scene, _Settings) || !_Scene )
  {
    std::cout << "Failed to load scene : " << sceneFile << std::endl;
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
  }

  const std::vector<PrimitiveInstance> & PrimitiveInstances = _Scene -> GetPrimitiveInstances();
  for ( int i = 0; i < PrimitiveInstances.size(); ++i )
    _PrimitiveNames.push_back(new std::string(_Scene -> FindPrimitiveName(i)));

  int skyboxID =_Scene -> AddTexture(g_AssetsDir + "skyboxes\\alps_field_2k.hdr", 4, TexFormat::TEX_FLOAT);
  {
    std::vector<Texture*> & textures = _Scene -> GetTetxures();

    Texture * skyboxTexture = textures[skyboxID];
    if ( skyboxTexture )
    {
      glGenTextures(1, &_SkyboxTextureID);
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, _SkyboxTextureID);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, skyboxTexture -> GetWidth(), skyboxTexture -> GetHeight(), 0, GL_RGBA, GL_FLOAT, skyboxTexture -> GetFData());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, 0);

      //_Settings._EnableSkybox = true;
    }
    else
      return 1;
  }

  _RenderSettingsModified = true;
  _SceneCameraModified    = true;
  _SceneLightsModified    = true;
  _SceneMaterialsModified = true;
  _SceneInstancesModified = true;

  return 0;
}

int Test3::UpdateScene()
{
  if ( !_Scene )
    return 1;

  // Mouse input
  {
    const float MouseSensitivity[5] = { 1.f, 0.5f, 0.01f, 0.01f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ZoomIn

    glfwGetCursorPos(_MainWindow, &_MouseX, &_MouseY);

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
      float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[4] * deltaY;
      if ( newRadius > 0.f )
        _Scene -> GetCamera().SetRadius(newRadius);
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
  }

  return 0;
}

void Test3::RenderToTexture()
{
  glBindFramebuffer(GL_FRAMEBUFFER, _FrameBufferID);
  glViewport(0, 0, RenderWidth(), RenderHeight());

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _SkyboxTextureID);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_BUFFER, _VtxTextureID);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_BUFFER, _VtxNormTextureID);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_BUFFER, _VtxUVTextureID);
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_BUFFER, _VtxIndTextureID);
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_BUFFER, _TexIndTextureID);
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_2D_ARRAY, _TexArrayTextureID);
  
  _Quad -> Render(*_RTTShader);
}

void Test3::RenderToSceen()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glActiveTexture(GL_TEXTURE0);
  
  glViewport(0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);

  _Quad -> Render(*_RTSShader);
}

int Test3::Run()
{
  int ret = 0;

  if ( !_MainWindow )
    return 1;

  glfwSetWindowTitle(_MainWindow, "Test 3 : Basic ray tracing");
  glfwSetWindowUserPointer(_MainWindow, this);

  glfwSetFramebufferSizeCallback(_MainWindow, Test3::FramebufferSizeCallback);
  glfwSetMouseButtonCallback(_MainWindow, Test3::MousebuttonCallback);
  glfwSetKeyCallback(_MainWindow, Test3::KeyCallback);

  glfwMakeContextCurrent(_MainWindow);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  InitializeUI();

  // Init openGL scene
  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    return 1;
  }

  // Shader compilation
  if ( ( 0 != RecompileShaders() ) || !_RTTShader || !_RTSShader )
  {
    std::cout << "Shader compilation failed!" << std::endl;
    return 1;
  }

  // Quad
  _Quad = new QuadMesh();

  // Frame buffer
  if ( 0 != InitializeFrameBuffer() )
  {
    std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
    return 1;
  }

  // Initialize the scene
  InitializeScene();

  // Main loop
  glfwSetWindowSize(_MainWindow, _Settings._RenderResolution.x, _Settings._RenderResolution.y);
  glViewport(0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y);
  glDisable(GL_DEPTH_TEST);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RenderWidth(), RenderHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  _CPULoopTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    _FrameNum++;

    UpdateCPUTime();

    glfwPollEvents();

    UpdateScene();
    UpdateUniforms();

    // Render to texture
    RenderToTexture();

    // Render to screen
    RenderToSceen();

    // UI
    DrawUI();

    glfwSwapBuffers(_MainWindow);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return ret;
}

}
