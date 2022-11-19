#include "Test3.h"
#include "QuadMesh.h"
#include "ShaderProgram.h"
#include "Scene.h"
#include "Camera.h"
#include "Light.h"
#include "Object.h"
#include "ObjectInstance.h"
#include "Math.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

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

  glViewport(0, 0, width, height);

  glBindTexture(GL_TEXTURE_2D, this_->_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
}

std::string UniformArrayElementName( const char * iUniformArrayName, int iIndex, const char * iAttributeName )
{
  return std::string(iUniformArrayName).append("[").append(std::to_string(iIndex)).append("].").append(iAttributeName);
}

Test3::Test3( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  _ScreenWidth  = iScreenWidth;
  _ScreenHeight = iScreenHeight;
}

Test3::~Test3()
{
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
  if ( _Scene )
    delete _Scene;
  _Scene = nullptr;
}

int Test3::InitializeFrameBuffer()
{
  // Screen texture
  glGenTextures(1, &_ScreenTextureID);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _ScreenWidth, _ScreenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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
    glUniform2f(glGetUniformLocation(RTTProgramID, "u_Resolution"), _ScreenWidth, _ScreenHeight);
    glUniform1f(glGetUniformLocation(RTTProgramID, "u_Time"), _CPULoopTime);
    glUniform1i(glGetUniformLocation(RTTProgramID, "u_FrameNum"), _FrameNum);
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
          glUniform3f(glGetUniformLocation(RTTProgramID, "u_SphereLight._Emission"), firstLight -> _Emission.x, firstLight -> _Emission.y, firstLight -> _Emission.z);
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

          glUniform1i(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_ID").c_str()), i);
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Albedo").c_str()), curMat._Albedo.r, curMat._Albedo.g, curMat._Albedo.b);
          glUniform3f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Emission").c_str()), curMat._Emission.r, curMat._Emission.g, curMat._Emission.b);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Metallic").c_str()), curMat._Metallic);
          glUniform1f(glGetUniformLocation(RTTProgramID, UniformArrayElementName("u_Materials",i,"_Roughness").c_str()), curMat._Roughness);
        }
        glUniform1i(glGetUniformLocation(RTTProgramID, "u_NbMaterials"), nbMaterials);

        _SceneMaterialsModified = false;
      }

      if ( _SceneInstancesModified )
      {
        const std::vector<MeshInstance>   & MeshInstances   = _Scene -> GetMeshInstances();
        const std::vector<ObjectInstance> & ObjectInstances = _Scene -> GetObjectInstances();

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
    _RTSShader -> StopUsing();
  }
  else
    return 1;

  return 0;
}

Vec4ui g_Seed;
Vec2i  g_Pixel;

void InitRNG( Vec2 p, unsigned int frame )
{
  g_Pixel = Vec2i(p);
  g_Seed = Vec4ui(p, frame, g_Pixel.x + g_Pixel.y);
}

void pcg4d(Vec4ui & v)
{
  v = v * 1664525u + 1013904223u;
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
  v = v ^ (v >> 16u);
  v.x += v.y * v.w; v.y += v.z * v.x; v.z += v.x * v.y; v.w += v.y * v.z;
}

float rand()
{
  pcg4d(g_Seed); return float(g_Seed.x) / float(0xffffffffu);
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

    InitRNG(Vec2(10,15), _FrameNum);
    ImGui::Text("Rand : %.4f", rand());


    ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);

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
        if ( ImGui::ColorPicker3("Emission", rgb) )
        {
          firstLight -> _Emission.r = rgb[0];
          firstLight -> _Emission.g = rgb[1];
          firstLight -> _Emission.b = rgb[2];
          _SceneLightsModified = true;
        }

        if ( ImGui::SliderFloat("Radius", &firstLight -> _Radius, 1.f, 1000.f) )
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
        if ( ImGui::ColorPicker3(std::string("##MatAlbedo").append(std::to_string(i)).c_str(), rgb) )
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

int Test3::InitializeScene()
{
  if ( _Scene )
    delete _Scene;

  _Scene = new Scene();

  // Camera
  Camera newCamera({0.f, 0.f, 2.f}, { 0.f, 0.f, 0.f }, 90.f);
  _Scene -> SetCamera(newCamera);

  // Lights
  Light newLight;
  newLight._Pos      = { 2.f, 10.f, .5f };
  newLight._Emission = { 1.f,  1.f, .5f };
  newLight._Type     = (float)LightType::SphereLight;
  newLight._Radius   = 100.f;
  _Scene -> AddLight(newLight);

  // Materials
  Material greenMat;
  greenMat._Albedo    = { .1f, .8f, .1f };
  greenMat._Emission  = { 0.f, 0.f, 0.f };
  greenMat._Metallic  = 0.4f;
  greenMat._Roughness = 0.f;
  int greenMatID = _Scene -> AddMaterial(greenMat, "Green");

  Material redMat;
  redMat._Albedo    = { .8f, .1f, .1f };
  redMat._Emission  = { 0.f, 0.f, 0.f };
  redMat._Metallic  = 0.3f;
  redMat._Roughness = 0.5f;
  int redMatID = _Scene -> AddMaterial(redMat, "Red");

  Material blueMat;
  blueMat._Albedo    = { .1f, .1f, .8f };
  blueMat._Emission  = { 0.f, 0.f, 0.f };
  blueMat._Metallic  = 0.1f;
  blueMat._Roughness = 0.7f;
  int blueMatID = _Scene -> AddMaterial(blueMat, "Blue");

  Material orangeMat;
  orangeMat._Albedo    = { .8f, .6f, .2f };
  orangeMat._Emission  = { 0.f, 0.f, 0.f };
  orangeMat._Metallic  = 0.1f;
  orangeMat._Roughness = 0.7f;
  int orangeMatID = _Scene -> AddMaterial(orangeMat, "Orange");

  // Objects
  Sphere smallSphere;
  smallSphere._Radius = .2f;
  int smallSphereID = _Scene -> AddObject(smallSphere);

  Sphere mediumSphere;
  mediumSphere._Radius = .5f;
  int mediumSphereID = _Scene -> AddObject(mediumSphere);

  Sphere bigSphere;
  bigSphere._Radius = .5f;
  int bigSphereID = _Scene -> AddObject(bigSphere);

  Mat4x4 transformMatrix;
  _Scene -> AddObjectInstance(mediumSphereID, greenMatID, transformMatrix);

  transformMatrix = glm::translate(Mat4x4(), Vec3(.5f, 1.f, -.5f));
  _Scene -> AddObjectInstance(smallSphereID, redMatID, transformMatrix);

  transformMatrix = glm::translate(Mat4x4(), Vec3(-.5f, 2.f, -.5f));
  _Scene -> AddObjectInstance(smallSphereID, blueMatID, transformMatrix);

  transformMatrix = glm::translate(Mat4x4(), Vec3(3.f, 2.f, -5.f));
  _Scene -> AddObjectInstance(mediumSphereID, blueMatID, transformMatrix);

  transformMatrix = glm::translate(Mat4x4(), Vec3(-1.f, 1.f, -3.f));
  _Scene -> AddObjectInstance(bigSphereID, redMatID, transformMatrix);

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
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  glViewport(0, 0, _ScreenWidth, _ScreenHeight);
  glDisable(GL_DEPTH_TEST);

  _CPULoopTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    _FrameNum++;

    UpdateCPUTime();

    glfwPollEvents();

    glfwGetFramebufferSize(_MainWindow, &_ScreenWidth, &_ScreenHeight);
    glViewport(0, 0, _ScreenWidth, _ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    UpdateScene();
    UpdateUniforms();

    // Render to texture
    glBindFramebuffer(GL_FRAMEBUFFER, _FrameBufferID);
    _Quad -> Render(*_RTTShader);

    // Render to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    _Quad -> Render(*_RTSShader);

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
