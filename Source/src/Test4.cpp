#include "Test4.h"

#include "QuadMesh.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "Scene.h"
#include "Camera.h"
#include "Light.h"
#include "Loader.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>
#include <omp.h>

namespace RTRT
{

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void Test4::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto * const this_ = static_cast<Test4*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  if ( action == GLFW_PRESS )
  {
    std::cout << "EVENT : KEY PRESS" << std::endl;

    switch ( key )
    {
    case GLFW_KEY_W:
      this_ -> _KeyState._KeyUp =    ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_S:
      this_ -> _KeyState._KeyDown =  ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_A:
      this_ -> _KeyState._KeyLeft =  ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_D:
      this_ -> _KeyState._KeyRight = ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    default :
      break;
    }
  }

  if ( action == GLFW_RELEASE )
  {
    std::cout << "EVENT : KEY RELEASE" << std::endl;

    bool updateFrameBuffer = false;

    switch ( key )
    {
    case GLFW_KEY_W:
      this_ -> _KeyState._KeyUp =    ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_S:
      this_ -> _KeyState._KeyDown =  ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_A:
      this_ -> _KeyState._KeyLeft =  ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_D:
      this_ -> _KeyState._KeyRight = ( action == GLFW_PRESS ); this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_PAGE_DOWN:
    {
      this_ -> _Settings._RenderScale = std::max(this_ -> _Settings._RenderScale - 5, 5);
      std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;
      updateFrameBuffer = true;
      this_ -> _UpdateImageTex = true;
      break;
    }
    case GLFW_KEY_PAGE_UP:
    {
      this_ -> _Settings._RenderScale = std::min(this_ -> _Settings._RenderScale + 5, 300);
      std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;
      updateFrameBuffer = true;
      this_ -> _UpdateImageTex = true;
      break;
    }
    case GLFW_KEY_LEFT:
    {
      float fov = std::max(this_ -> _Scene -> GetCamera().GetFOVInDegrees() - 5.f, 10.f);
      this_ -> _Scene -> GetCamera().SetFOVInDegrees(fov);
      std::cout << "FOV = " << this_ -> _Scene -> GetCamera().GetFOVInDegrees() << std::endl;
      this_ -> _UpdateImageTex = true;
      break;
    }
    case GLFW_KEY_RIGHT:
    {
      float fov = std::min(this_ -> _Scene -> GetCamera().GetFOVInDegrees() + 5.f, 150.f);
      this_ -> _Scene -> GetCamera().SetFOVInDegrees(fov);
      std::cout << "FOV = " << this_ -> _Scene -> GetCamera().GetFOVInDegrees() << std::endl;
      this_ -> _UpdateImageTex = true;
      break;
    }
    case GLFW_KEY_DOWN:
    {
      float zNear, zFar;
      this_ -> _Scene -> GetCamera().GetZNearFar(zNear, zFar);
      zNear = std::max(zNear - 1.f, 1.f);
      this_ -> _Scene -> GetCamera().SetZNearFar(zNear, zFar);
      std::cout << "ZNEAR = " << zNear << std::endl;
      this_ -> _UpdateImageTex = true;
      break;
    }
    case GLFW_KEY_UP:
    {
      float zNear, zFar;
      this_ -> _Scene -> GetCamera().GetZNearFar(zNear, zFar);
      zNear = std::min(zNear + 1.f, zFar - 1.f);
      this_ -> _Scene -> GetCamera().SetZNearFar(zNear, zFar);
      std::cout << "ZNEAR = " << zNear << std::endl;
      this_ -> _UpdateImageTex = true;
      break;
    }
    default :
      break;
    }

    if ( updateFrameBuffer )
    {
      this_ -> ResizeImageBuffers();

      glBindTexture(GL_TEXTURE_2D, this_->_ScreenTextureID);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, this_ -> _Settings._RenderResolution.x, this_ -> _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
      glBindTexture(GL_TEXTURE_2D, 0);
    }
  }
}

// ----------------------------------------------------------------------------
// MousebuttonCallback
// ----------------------------------------------------------------------------
void Test4::MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test4*>(glfwGetWindowUserPointer(window));
    if ( !this_ )
      return;

    double mouseX = 0.f, mouseY = 0.f;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    if ( ( GLFW_MOUSE_BUTTON_1 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : LEFT CLICK (" << mouseX << "," << mouseY << ")" << std::endl;
      this_->_MouseState._LeftClick = true;
    }
    else
      this_->_MouseState._LeftClick = false;

    if ( ( GLFW_MOUSE_BUTTON_2 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : RIGHT CLICK (" << mouseX << "," << mouseY << ")" << std::endl;
      this_->_MouseState._RightClick = true;
    }
    else
      this_->_MouseState._RightClick = false;

    if ( ( GLFW_MOUSE_BUTTON_3 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : MIDDLE CLICK (" << mouseX << "," << mouseY << ")" << std::endl;
      this_->_MouseState._MiddleClick = true;
    }
    else
      this_->_MouseState._MiddleClick = false;
  }
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void Test4::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  auto * const this_ = static_cast<Test4*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;

  this_ -> _Settings._WindowResolution.x = width;
  this_ -> _Settings._WindowResolution.y = height;
  this_ -> _Settings._RenderResolution.x = width  * ( this_ -> _Settings._RenderScale * 0.01f );
  this_ -> _Settings._RenderResolution.y = height * ( this_ -> _Settings._RenderScale * 0.01f );

  this_ -> ResizeImageBuffers();

  glViewport(0, 0, this_ -> _Settings._WindowResolution.x, this_ -> _Settings._WindowResolution.y);

  glBindTexture(GL_TEXTURE_2D, this_->_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, this_ -> _Settings._RenderResolution.x, this_ -> _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

   this_ -> _UpdateImageTex = true;
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Test4::Test4( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  _Settings._RenderScale = 100;

  _Settings._WindowResolution.x = iScreenWidth;
  _Settings._WindowResolution.y = iScreenHeight;
  _Settings._RenderResolution.x = iScreenWidth  * ( _Settings._RenderScale * 0.01f );
  _Settings._RenderResolution.y = iScreenHeight * ( _Settings._RenderScale * 0.01f );

  ResizeImageBuffers();
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test4::~Test4()
{
  glDeleteFramebuffers(1, &_FrameBufferID);

  glDeleteTextures(1, &_ScreenTextureID);
  glDeleteTextures(1, &_ImageTextureID);
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
void Test4::ResizeImageBuffers()
{
  _Settings._RenderResolution.x = _Settings._WindowResolution.x * ( _Settings._RenderScale * 0.01f );
  _Settings._RenderResolution.y = _Settings._WindowResolution.y * ( _Settings._RenderScale * 0.01f );

  _Image.resize(_Settings._RenderResolution.x  * _Settings._RenderResolution.y);
  _DepthBuffer.resize(_Settings._RenderResolution.x  * _Settings._RenderResolution.y);
}

// ----------------------------------------------------------------------------
// UpdateCPUTime
// ----------------------------------------------------------------------------
int Test4::UpdateCPUTime()
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

// ----------------------------------------------------------------------------
// InitializeFrameBuffer
// ----------------------------------------------------------------------------
int Test4::InitializeFrameBuffer()
{
  // Screen texture
  glGenTextures(1, &_ScreenTextureID);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  // Image texture
  glGenTextures(1, &_ImageTextureID);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _ImageTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, &_Image[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
int Test4::RecompileShaders()
{
  ShaderSource vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  ShaderSource fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_drawTexture.glsl");

  ShaderProgram * newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RTTShader.reset(newShader);

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  newShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !newShader )
    return 1;
  _RTSShader.reset(newShader);

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateUniforms
// ----------------------------------------------------------------------------
int Test4::UpdateUniforms()
{
  if ( _RTTShader )
  {
    _RTTShader -> Use();
    GLuint RTTProgramID = _RTTShader -> GetShaderProgramID();

    glUniform1i(glGetUniformLocation(RTTProgramID, "u_ImageTexture"), 1);
    glUniform3f(glGetUniformLocation(RTTProgramID, "u_Resolution"), _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0.f);

    _RTTShader -> StopUsing();
  }
  else
    return 1;

  if ( _RTSShader )
  {
    _RTSShader -> Use();
    GLuint RTSProgramID = _RTSShader -> GetShaderProgramID();
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_ScreenTexture"), 0);
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_AccumulatedFrames"), 1);
    _RTSShader -> StopUsing();
  }
  else
    return 1;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateTextures
// ----------------------------------------------------------------------------
int Test4::UpdateTextures()
{
  if ( _UpdateImageTex )
  {
    glBindTexture(GL_TEXTURE_2D, _ImageTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, &_Image[0]);
    glBindTexture(GL_TEXTURE_2D, 0);

    //_UpdateImageTex = false;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
void Test4::RenderToTexture()
{
  glBindFramebuffer(GL_FRAMEBUFFER, _FrameBufferID);
  glViewport(0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _ImageTextureID);
  
  _Quad -> Render(*_RTTShader);
}

// ----------------------------------------------------------------------------
// RenderToSceen
// ----------------------------------------------------------------------------
void Test4::RenderToSceen()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glActiveTexture(GL_TEXTURE0);
  
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);

  _Quad -> Render(*_RTSShader);
}

// ----------------------------------------------------------------------------
// InitializeUI
// ----------------------------------------------------------------------------
int Test4::InitializeUI()
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
  io.Fonts -> AddFontDefault();

  // Setup Platform/Renderer backends
  const char* glsl_version = "#version 130";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  return 0;
}

// ----------------------------------------------------------------------------
// DrawUI
// ----------------------------------------------------------------------------
void Test4::DrawUI()
{
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  // Frame info
  {
    ImGui::Begin("Test 4");

    ImGui::Text("Render time %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);

    ImGui::Text("Window width %d: height : %d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    ImGui::Text("Render width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);
    ImGui::Text("Render scale : %d %%", _Settings._RenderScale);

    ImGui::End();
  }

  // Rendering
  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ----------------------------------------------------------------------------
// UpdateImage
// ----------------------------------------------------------------------------
int Test4::UpdateImage()
{
  if ( _UpdateImageTex )
  {
    int width  = _Settings._RenderResolution.x;
    int height = _Settings._RenderResolution.y;
    int size = width * height;
    float ratio = width / float(height);

    Mat4x4 MV;
    _Scene -> GetCamera().ComputeLookAtMatrix(MV);

    Mat4x4 P;
    _Scene -> GetCamera().ComputePerspectiveProjMatrix(ratio, P);

    Mat4x4 MVP = P * MV;

    const std::vector<Vec3>     & vertices = _Scene -> GetVertices();
    const std::vector<Vec3i>    & indices  = _Scene -> GetIndices();
    const std::vector<Vec3>     & uvMatIDs = _Scene -> GetUVMatID();
    const std::vector<Texture*> & textures = _Scene -> GetTetxures();
    const int nbTris = indices.size() / 3;

    const Vec3 R = { 1.f, 0.f, 0.f };
    const Vec3 G = { 0.f, 1.f, 0.f };
    const Vec3 B = { 0.f, 0.f, 1.f };

    const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
    fill(_Image.begin(), _Image.end(), backgroundColor);
    fill(_DepthBuffer.begin(), _DepthBuffer.end(), 1.f);

    Vec2 bboxMin(std::numeric_limits<float>::infinity());
    Vec2 bboxMax = -bboxMin;

    for ( int i = 0; i < nbTris; ++i )
    {
      Vec3i Index[3];
      Index[0] = indices[i*3];
      Index[1] = indices[i*3+1];
      Index[2] = indices[i*3+2];

      Vec4 ProjVec[3];
      int j;
      for ( int j = 0; j < 3; ++j )
      {
        Vec4 projV4 = MVP * Vec4(vertices[Index[j].x], 1.f);
        ProjVec[j].w = 1.f / projV4.w;
        ProjVec[j].x = projV4.x * ProjVec[j].w;
        ProjVec[j].y = projV4.y * ProjVec[j].w;
        ProjVec[j].z = projV4.z * ProjVec[j].w;

        ProjVec[j].x = ((ProjVec[j].x + 1.f) * .5f * width);
        ProjVec[j].y = ((ProjVec[j].y + 1.f) * .5f * height);
        
        if ( ProjVec[j].x < bboxMin.x )
          bboxMin.x = ProjVec[j].x;
        if ( ProjVec[j].y < bboxMin.y )
          bboxMin.y = ProjVec[j].y;
        if ( ProjVec[j].x > bboxMax.x )
          bboxMax.x = ProjVec[j].x;
        if ( ProjVec[j].y > bboxMax.y )
          bboxMax.y = ProjVec[j].y;
      }

      int xMin = std::max(0, std::min((int)std::floorf(bboxMin.x), width  - 1));
      int yMin = std::max(0, std::min((int)std::floorf(bboxMin.y), height - 1));
      int xMax = std::max(0, std::min((int)std::floorf(bboxMax.x), width  - 1)); 
      int yMax = std::max(0, std::min((int)std::floorf(bboxMax.y), height - 1));

      float area = EdgeFunction(ProjVec[0], ProjVec[1], ProjVec[2]);

      for ( int y = yMin; y <= yMax; ++y )
      {
        for ( int x = xMin; x <= xMax; ++x  )
        {
          Vec3 p(x, y, 1.f);

          float W[3];
          W[0] = EdgeFunction(ProjVec[1], ProjVec[2], p);
          W[1] = EdgeFunction(ProjVec[2], ProjVec[0], p);
          W[2] = EdgeFunction(ProjVec[0], ProjVec[1], p);

          if ( ( W[0] >= 0.f )
            && ( W[1] >= 0.f )
            && ( W[2] >= 0.f ) )
          {
            W[0] /= area;
            W[1] /= area;
            W[2] /= area;

            // pertspective correction
            W[0] *= ProjVec[0].w;
            W[1] *= ProjVec[1].w;
            W[2] *= ProjVec[2].w;
            float perspFactor = 1.f / (W[0] + W[1] + W[2]);

            float depth = W[0] * ProjVec[0].z + W[1] * ProjVec[1].z + W[2] * ProjVec[2].z;
            depth *= perspFactor;
            
            if ( depth < _DepthBuffer[x + width * y] )
            {
              Vec4 pixelColor(1.f);

              if ( Index[0].z >=0 )
              {
                Vec3 UVMatID[3];
                UVMatID[0] = uvMatIDs[Index[0].z];
                UVMatID[1] = uvMatIDs[Index[1].z];
                UVMatID[2] = uvMatIDs[Index[2].z];
              
                if ( UVMatID[0].z >= 0 )
                {
                  float u = W[0] * UVMatID[0].x + W[1] * UVMatID[1].x + W[2] * UVMatID[2].x;
                  float v = W[0] * UVMatID[0].y + W[1] * UVMatID[1].y + W[2] * UVMatID[2].y;
                  u *= perspFactor;
                  v *= perspFactor;
              
                  Texture * tex = textures[UVMatID[0].z];
                  if ( tex )
                    pixelColor = tex -> Sample(u, v);
                }
              }
              else
              {
                pixelColor.x = W[0] * R[0] + W[1] * G[0] + W[2] * B[0];
                pixelColor.y = W[0] * R[1] + W[1] * G[1] + W[2] * B[1];
                pixelColor.z = W[0] * R[2] + W[1] * G[2] + W[2] * B[2];
              }

              _Image[x + width * y] = pixelColor;
              _DepthBuffer[x + width * y] = depth;
            }
          }
        }
      }
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test4::InitializeScene()
{
  std::string sceneFile = "..\\..\\Assets\\TexturedBox.scene";
  //std::string sceneFile = "..\\..\\Assets\\my_cornell_box.scene";

  Scene * newScene = nullptr;
  if ( !Loader::LoadScene(sceneFile, newScene, _Settings) || !newScene )
  {
    std::cout << "Failed to load scene : " << sceneFile << std::endl;
    return 1;
  }
  _Scene.reset(newScene);

  // Scene should contain at least one light
  Light * firstLight = _Scene -> GetLight(0);
  if ( !firstLight )
  {
    Light newLight;
    _Scene -> AddLight(newLight);
  }

  _Scene -> CompileMeshData(_Settings._TextureSize);

  // Resize Image Buffer
  this -> ResizeImageBuffers();

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateScene
// ----------------------------------------------------------------------------
int Test4::UpdateScene()
{
  if ( !_Scene )
    return 1;

  // Mouse input
  {
    const float MouseSensitivity[5] = { 1.f, 0.5f, 0.01f, 0.01f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ZoomIn

    double oldMouseX = _MouseState._MouseX;
    double oldMouseY = _MouseState._MouseY;
    glfwGetCursorPos(_MainWindow, &_MouseState._MouseX, &_MouseState._MouseY);

    double deltaX = _MouseState._MouseX - oldMouseX;
    double deltaY = _MouseState._MouseY - oldMouseY;

    if ( _MouseState._LeftClick )
    {
      _Scene -> GetCamera().OffsetOrientations(MouseSensitivity[0] * deltaX, MouseSensitivity[1] * -deltaY);
    }
    if ( _MouseState._RightClick )
    {
      _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, MouseSensitivity[3] * deltaY);
    }
    if ( _MouseState._MiddleClick )
    {
      if ( std::abs(deltaX) > std::abs(deltaY) )
        _Scene -> GetCamera().Strafe(MouseSensitivity[2] * deltaX, 0.f);
      else
      {
        float newRadius = _Scene -> GetCamera().GetRadius() + MouseSensitivity[4] * deltaY;
        if ( newRadius > 0.f )
          _Scene -> GetCamera().SetRadius(newRadius);
      }
    }
  }

  // Keyboard input
  {
    const float velocity = 100.f;

    if ( _KeyState._KeyUp )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() - _TimeDelta;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
      }
    }
    if ( _KeyState._KeyDown )
    {
      float newRadius = _Scene -> GetCamera().GetRadius() + _TimeDelta;
      if ( newRadius > 0.f )
      {
        _Scene -> GetCamera().SetRadius(newRadius);
      }
    }
    if ( _KeyState._KeyLeft )
    {
      _Scene -> GetCamera().OffsetOrientations(_TimeDelta * velocity, 0.f);
    }
    if ( _KeyState._KeyRight )
    {
      _Scene -> GetCamera().OffsetOrientations(-_TimeDelta * velocity, 0.f);
    }
  }

  return 0;
}

// ----------------------------------------------------------------------------
// Run
// ----------------------------------------------------------------------------
int Test4::Run()
{
  int ret = 0;

  if ( !_MainWindow )
    return 1;

  glfwSetWindowTitle(_MainWindow, "Test 4 : CPU Rasterizer");
  glfwSetWindowUserPointer(_MainWindow, this);

  glfwSetFramebufferSizeCallback(_MainWindow, Test4::FramebufferSizeCallback);
  glfwSetMouseButtonCallback(_MainWindow, Test4::MousebuttonCallback);
  glfwSetKeyCallback(_MainWindow, Test4::KeyCallback);

  glfwMakeContextCurrent(_MainWindow);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  if ( InitializeUI() != 0 )
  {
    std::cout << "Failed to initialize ImGui!" << std::endl;
    glfwTerminate();
    return 1;
  }

  // Init openGL scene
  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    glfwTerminate();
    return 1;
  }

  // Initialize the scene
  if ( 0 != InitializeScene() )
  {
    std::cout << "ERROR: Scene initialization failed!" << std::endl;
    return 1;
  }

  // Shader compilation
  if ( ( 0 != RecompileShaders() ) || !_RTTShader || !_RTSShader )
  {
    std::cout << "Shader compilation failed!" << std::endl;
    return 1;
  }

  // Quad
  _Quad = std::make_unique<QuadMesh>();

  // Frame buffer
  if ( 0 != InitializeFrameBuffer() )
  {
    std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
    return 1;
  }

  // Main loop
  glfwSetWindowSize(_MainWindow, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glDisable(GL_DEPTH_TEST);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  _CPULoopTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    UpdateCPUTime();

    glfwPollEvents();

    UpdateScene();

    UpdateImage();

    UpdateTextures();

    UpdateUniforms();

    // Render to texture
    RenderToTexture();

    // Render to screen
    RenderToSceen();

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
