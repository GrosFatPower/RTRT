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
#include <execution>
#include <omp.h>

#define PARALLEL
#ifdef PARALLEL
constexpr std::execution::parallel_policy policy = std::execution::par;
#else
constexpr std::execution::sequenced_policy policy = std::execution::seq;
#endif

//#define SIMUL_RENDERING_PIPELINE

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
      this_ -> _KeyState._KeyUp =    true; break;
    case GLFW_KEY_S:
      this_ -> _KeyState._KeyDown =  true; break;
    case GLFW_KEY_A:
      this_ -> _KeyState._KeyLeft =  true; break;
    case GLFW_KEY_D:
      this_ -> _KeyState._KeyRight = true; break;
    case GLFW_KEY_LEFT_CONTROL:
      this_ -> _KeyState._KeyLeftCTRL = true; break;
    default :
      break;
    }
  }

  else if ( action == GLFW_RELEASE )
  {
    std::cout << "EVENT : KEY RELEASE" << std::endl;

    bool updateFrameBuffer = false;

    switch ( key )
    {
    case GLFW_KEY_W:
      this_ -> _KeyState._KeyUp =    false; break;
    case GLFW_KEY_S:
      this_ -> _KeyState._KeyDown =  false; break;
    case GLFW_KEY_A:
      this_ -> _KeyState._KeyLeft =  false; break;
    case GLFW_KEY_D:
      this_ -> _KeyState._KeyRight = false; break;
    case GLFW_KEY_Y:
      this_ -> _ViewDepthBuffer = !this_ -> _ViewDepthBuffer; break;
    case GLFW_KEY_LEFT_CONTROL:
      this_ -> _KeyState._KeyLeftCTRL = false; break;
    case GLFW_KEY_PAGE_DOWN:
    {
      this_ -> _Settings._RenderScale = std::max(this_ -> _Settings._RenderScale - 5, 5);
      std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;
      updateFrameBuffer = true;
      break;
    }
    case GLFW_KEY_PAGE_UP:
    {
      this_ -> _Settings._RenderScale = std::min(this_ -> _Settings._RenderScale + 5, 300);
      std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;
      updateFrameBuffer = true;
      break;
    }
    case GLFW_KEY_LEFT:
    {
      float fov = std::max(this_ -> _Scene -> GetCamera().GetFOVInDegrees() - 5.f, 10.f);
      this_ -> _Scene -> GetCamera().SetFOVInDegrees(fov);
      std::cout << "FOV = " << this_ -> _Scene -> GetCamera().GetFOVInDegrees() << std::endl;
      break;
    }
    case GLFW_KEY_RIGHT:
    {
      float fov = std::min(this_ -> _Scene -> GetCamera().GetFOVInDegrees() + 5.f, 150.f);
      this_ -> _Scene -> GetCamera().SetFOVInDegrees(fov);
      std::cout << "FOV = " << this_ -> _Scene -> GetCamera().GetFOVInDegrees() << std::endl;
      break;
    }
    case GLFW_KEY_DOWN:
    {
      float zNear, zFar;
      this_ -> _Scene -> GetCamera().GetZNearFar(zNear, zFar);
      if ( this_ -> _KeyState._KeyLeftCTRL )
        zFar = std::max(zFar - 1.f, zNear + 0.1f);
      else
        zNear = std::max(zNear - 0.1f, 0.1f);
      this_ -> _Scene -> GetCamera().SetZNearFar(zNear, zFar);
      std::cout << "ZNEAR = " << zNear << std::endl;
      break;
    }
    case GLFW_KEY_UP:
    {
      float zNear, zFar;
      this_ -> _Scene -> GetCamera().GetZNearFar(zNear, zFar);
      if ( this_ -> _KeyState._KeyLeftCTRL )
        zFar += 1.f;
      else
        zNear = std::min(zNear + 0.1f, zFar - 0.1f);
      this_ -> _Scene -> GetCamera().SetZNearFar(zNear, zFar);
      std::cout << "ZNEAR = " << zNear << std::endl;
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

  _ColorBuffer.resize(_Settings._RenderResolution.x  * _Settings._RenderResolution.y);
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, &_ColorBuffer[0]);
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
  glBindTexture(GL_TEXTURE_2D, _ImageTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, &_ColorBuffer[0]);
  glBindTexture(GL_TEXTURE_2D, 0);

  return 0;
}

// ----------------------------------------------------------------------------
// RenderToTexture
// ----------------------------------------------------------------------------
void Test4::RenderToTexture()
{
  double startTime = glfwGetTime();

  glBindFramebuffer(GL_FRAMEBUFFER, _FrameBufferID);
  glViewport(0, 0, _Settings._RenderResolution.x, _Settings._RenderResolution.y);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, _ImageTextureID);
  
  _Quad -> Render(*_RTTShader);

  _RTTElapsed = glfwGetTime() - startTime;
}

// ----------------------------------------------------------------------------
// RenderToSceen
// ----------------------------------------------------------------------------
void Test4::RenderToSceen()
{
  double startTime = glfwGetTime();

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glActiveTexture(GL_TEXTURE0);
  
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);

  _Quad -> Render(*_RTSShader);

  _RTSElapsed = glfwGetTime() - startTime;
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

    ImGui::Text("Render time : %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);
    ImGui::Text("Render image : %.3f ms/frame", _RenderImgElapsed * 1000.f);
    ImGui::Text("Render to texture : %.3f ms/frame", _RTTElapsed * 1000.f);
    ImGui::Text("Render to screen : %.3f ms/frame", _RTSElapsed * 1000.f);

    ImGui::Text("Window width %d: height : %d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    ImGui::Text("Render width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);
    ImGui::Text("Render scale : %d %%", _Settings._RenderScale);
    ImGui::Text("FOV : %3.0f deg", _Scene -> GetCamera().GetFOVInDegrees());
    
    float near, far;
    _Scene -> GetCamera().GetZNearFar(near, far);
    ImGui::Text("Near : %f", near);
    ImGui::Text("Far : %f", far);

    ImGui::End();
  }

  // Rendering
  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ----------------------------------------------------------------------------
// UpdateImage
// ----------------------------------------------------------------------------
#ifdef SIMUL_RENDERING_PIPELINE
int Test4::UpdateImage()
{
  double startTime = glfwGetTime();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;
  int size = width * height;
  float ratio = width / float(height);

  Mat4x4 MV;
  _Scene -> GetCamera().ComputeLookAtMatrix(MV);

  Mat4x4 P;
  _Scene -> GetCamera().ComputePerspectiveProjMatrix(ratio, P);

  Mat4x4 MVP = P * MV;

  float near, far;
  _Scene -> GetCamera().GetZNearFar(near, far);

  const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
  std::fill(policy, _ColorBuffer.begin(), _ColorBuffer.end(), backgroundColor);
  std::fill(policy, _DepthBuffer.begin(), _DepthBuffer.end(), far);

  const std::vector<Vec3i>    & Indices   = _Scene -> GetIndices();
  const std::vector<Vec3>     & Vertices  = _Scene -> GetVertices();
  const std::vector<Vec3>     & UVMatIDs  = _Scene -> GetUVMatID();
  const int nbTris = Indices.size() / 3;

  for ( int i = 0; i < nbTris; ++i )
  {
    Vec3i Index[3];
    Index[0] = Indices[i*3];
    Index[1] = Indices[i*3+1];
    Index[2] = Indices[i*3+2];

    Vertex Vert[3];
    for ( int j = 0; j < 3; ++j )
    {
      Vert[j]._Position = Vec4(Vertices[Index[j].x], 1.f);
      Vert[j]._UV       = Vec2(UVMatIDs[Index[j].z].x, UVMatIDs[Index[j].z].y);
      Vert[j]._MatID    = (int)UVMatIDs[Index[j].z].z;
      // ToDo
    }

    // ToDo


  }


  _RenderImgElapsed = glfwGetTime() - startTime;

  return 0;
}
#else
int Test4::UpdateImage()
{
  double startTime = glfwGetTime();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;
  int size = width * height;
  float ratio = width / float(height);

  Mat4x4 MV;
  _Scene -> GetCamera().ComputeLookAtMatrix(MV);

  Mat4x4 P;
  _Scene -> GetCamera().ComputePerspectiveProjMatrix(ratio, P);

  Mat4x4 MVTrInv = glm::transpose(glm::inverse(MV));

  Mat4x4 MVP = P * MV;

  float near, far;
  _Scene -> GetCamera().GetZNearFar(near, far);

  const std::vector<Vec3>     & vertices  = _Scene -> GetVertices();
  const std::vector<Vec3i>    & indices   = _Scene -> GetIndices();
  const std::vector<Vec3>     & uvMatIDs  = _Scene -> GetUVMatID();
  const std::vector<Vec3>     & normals   = _Scene -> GetNormals();
  const std::vector<Material> & materials = _Scene -> GetMaterials();
  const std::vector<Texture*> & textures  = _Scene -> GetTextures();
  const int nbVertices = vertices.size();
  const int nbTris = indices.size() / 3;

  const Vec3 R = { 1.f, 0.f, 0.f };
  const Vec3 G = { 0.f, 1.f, 0.f };
  const Vec3 B = { 0.f, 0.f, 1.f };

  const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
  std::fill(policy, _ColorBuffer.begin(), _ColorBuffer.end(), backgroundColor);
  //std::fill(policy, _DepthBuffer.begin(), _DepthBuffer.end(), 1.f);
  std::fill(policy, _DepthBuffer.begin(), _DepthBuffer.end(), far);

  std::vector<Vec4> CamVerts;
  std::vector<Vec4> ProjVerts;
  CamVerts.resize(nbVertices);
  ProjVerts.resize(nbVertices);

//#pragma omp parallel
  { 
    //#pragma omp for
    for ( int i = 0; i < nbVertices; ++i )
    {
      CamVerts[i] = MV * Vec4(vertices[i], 1.f);

      Vec4 projVert = P * CamVerts[i];
      projVert.w = 1.f / projVert.w; // 1.f / -z
      projVert.x *= projVert.w;      // X / -z
      projVert.y *= projVert.w;      // Y / -z
      projVert.z *= projVert.w;      // Z / -z

      projVert.x = ((projVert.x + 1.f) * .5f * width);
      projVert.y = ((projVert.y + 1.f) * .5f * height);
      ProjVerts[i] = projVert;
    }
  }

  Vec2 bboxMin(std::numeric_limits<float>::infinity());
  Vec2 bboxMax = -bboxMin;

  for ( int i = 0; i < nbTris; ++i )
  {
    Vec3i Index[3];
    Index[0] = indices[i*3];
    Index[1] = indices[i*3+1];
    Index[2] = indices[i*3+2];

    Vec4 CamVec[3];
    Vec4 ProjVec[3];
    for ( int j = 0; j < 3; ++j )
    {
      CamVec[j] = CamVerts[Index[j].x];
      ProjVec[j] = ProjVerts[Index[j].x];
        
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

    float invArea = 1.f / EdgeFunction(ProjVec[0], ProjVec[1], ProjVec[2]);

    for ( int y = yMin; y <= yMax; ++y )
    {
      for ( int x = xMin; x <= xMax; ++x  )
      {
        Vec3 p(x + .5f, y + .5f, 0.f);

        float W[3];
        W[0] = EdgeFunction(ProjVec[1], ProjVec[2], p);
        W[1] = EdgeFunction(ProjVec[2], ProjVec[0], p);
        W[2] = EdgeFunction(ProjVec[0], ProjVec[1], p);

        if ( ( W[0] < 0.f )
          || ( W[1] < 0.f )
          || ( W[2] < 0.f ) )
            continue;

        W[0] *= invArea;
        W[1] *= invArea;
        W[2] *= invArea;

        // perspective correction
        W[0] *= ProjVec[0].w; // W0 / z0
        W[1] *= ProjVec[1].w; // W1 / z1
        W[2] *= ProjVec[2].w; // W2 / z2

        float Z = 1.f / (W[0] + W[1] + W[2]);
        W[0] *= Z;
        W[1] *= Z;
        W[2] *= Z;

        float z = W[0] * ProjVec[0].z + W[1] * ProjVec[1].z + W[2] * ProjVec[2].z;
        z = ( z + 1.f ) * .5f;
        if ( ( z < 0.f ) || ( z > 1.f ) )
          continue;

        if ( Z < _DepthBuffer[x + width * y] && ( Z > near) )
        {
          Vec3 color(1.f);

          if ( Index[0].z >=0 )
          {
            Vec3 UVMatID[3];
            UVMatID[0] = uvMatIDs[Index[0].z];
            UVMatID[1] = uvMatIDs[Index[1].z];
            UVMatID[2] = uvMatIDs[Index[2].z];
              
            if ( UVMatID[0].z >= 0 && materials.size() )
            {
              Material Mat[3] = { materials[UVMatID[0].z], materials[UVMatID[1].z], materials[UVMatID[1].z] };

              if ( Mat[0]._BaseColorTexId >= 0 )
              {
                float u = W[0] * UVMatID[0].x + W[1] * UVMatID[1].x + W[2] * UVMatID[2].x;
                float v = W[0] * UVMatID[0].y + W[1] * UVMatID[1].y + W[2] * UVMatID[2].y;
              
                Texture * tex = textures[Mat[0]._BaseColorTexId];
                if ( tex )
                  color = tex -> Sample(u, v);
              }
              else
              {
                color = Mat[0]._Albedo * W[0] +  Mat[1]._Albedo * W[1] + Mat[2]._Albedo * W[2];
              }
            }
          }
          else
          {
            color = W[0] * R + W[1] * G + W[2] * B;
          }

          // Shading
          if ( ( Index[0].y >=0 )
            && ( Index[1].y >=0 )
            && ( Index[2].y >=0 ) )
          {
            Vec3 worldP = vertices[Index[0].x] * W[0] + vertices[Index[1].x] * W[1] + vertices[Index[2].x] * W[2];

            float ambientStrength = .1f;
            float diffuse = 0.f;
            float specular = 0.f;
            Light * firstLight = _Scene -> GetLight(0);
            if ( firstLight )
            {
              Vec3 normal = normals[Index[0].y] * W[0] + normals[Index[1].y] * W[1] + normals[Index[2].y] * W[2];
              normal = glm::normalize(normal);

              Vec3 dirToLight = glm::normalize(firstLight ->_Pos - worldP);
              diffuse = std::max(0.f, glm::dot(normal, dirToLight));

              Vec3 viewDir =  glm::normalize(_Scene -> GetCamera().GetPos() - worldP);
              Vec3 reflectDir = glm::reflect(-dirToLight, normal);

              static float specularStrength = 0.5f;
              specular = pow(std::max(glm::dot(viewDir, reflectDir), 0.f), 32) * specularStrength;

              color *= std::min(diffuse+ambientStrength+specular, 1.f) * glm::normalize(firstLight -> _Emission);
              color = MathUtil::Min(color, Vec3(1.f));
            }
            else
            {
              Vec3 V1V0 = CamVec[1] - CamVec[0];
              Vec3 V2V0 = CamVec[2] - CamVec[0];
              Vec3 normal = glm::cross(V1V0, V2V0);
              normal = glm::normalize(normal);

              Vec3 viewDir =  glm::normalize(-worldP);
              diffuse =  std::max(0.f, glm::dot(normal,viewDir));

              color *= std::min(diffuse+ambientStrength, 1.f);
            }
          }

          if ( _ViewDepthBuffer )
            _ColorBuffer[x + width * y] = Vec4(Vec3(1.f - z), 1.f);
          else
            _ColorBuffer[x + width * y] = Vec4(color, 1.f);
          _DepthBuffer[x + width * y] = Z;
        }
      }
    }
  }

  _RenderImgElapsed = glfwGetTime() - startTime;

  return 0;
}
#endif

// ----------------------------------------------------------------------------
// VertexShader
// ----------------------------------------------------------------------------
void Test4::VertexShader( const Vertex & iVertex, const Mat4x4 iMVP, Vec4 & oVertexPosition, Attributes & oAttrib )
{
  oVertexPosition = iMVP * iVertex._Position;
  oAttrib._UV     = iVertex._UV;
  oAttrib._Color  = iVertex._Color;
  oAttrib._MatID  = iVertex._MatID;
}

// ----------------------------------------------------------------------------
// FragmentShader
// ----------------------------------------------------------------------------
void Test4::FragmentShader( const Vec4 & iCoord, const Attributes & iAttrib, Vec4 & oColor )
{

}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test4::InitializeScene()
{
  //std::string sceneFile = "..\\..\\Assets\\TexturedBoxes.scene";
  //std::string sceneFile = "..\\..\\Assets\\TexturedBox.scene";
  std::string sceneFile = "..\\..\\Assets\\my_cornell_box.scene";

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
// ProcessInput
// ----------------------------------------------------------------------------
void Test4::ProcessInput()
{

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

    ProcessInput();

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
