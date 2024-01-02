#include "Test4.h"

#include "QuadMesh.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "Scene.h"
#include "Camera.h"
#include "Light.h"
#include "Loader.h"
#include "JobSystem.h"

#include "tinydir.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <iostream>
#include <execution>
//#include <omp.h>
#include <thread>

constexpr std::execution::parallel_policy policy = std::execution::par;

namespace RTRT
{

const char * Test4::GetTestHeader() { return "Test 4 : CPU Rasterizer"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

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
    case GLFW_KEY_ESCAPE:
      this_ -> _KeyState._KeyEsc = true; break;
    default :
      break;
    }
  }

  else if ( action == GLFW_RELEASE )
  {
    std::cout << "EVENT : KEY RELEASE" << std::endl;

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
      this_ -> _ColorDepthOrNormalsBuffer = ( ( this_ -> _ColorDepthOrNormalsBuffer + 1 ) % 3 ); break;
    case GLFW_KEY_K:
      this_ -> _ShowWires = !this_ -> _ShowWires; break;
    case GLFW_KEY_T:
      this_ -> _BilinearSampling = !this_ -> _BilinearSampling; break;
    case GLFW_KEY_L:
      this_ -> _ShadingType = ( ShadingType::Flat == this_ -> _ShadingType ) ? ( ShadingType::Phong ) : ( ShadingType::Flat ); break;
    case GLFW_KEY_B:
      this_ -> _Settings._EnableBackGround = !this_ -> _Settings._EnableBackGround; break;
    case GLFW_KEY_R:
      this_ -> _ReloadScene = true; break;
    case GLFW_KEY_N:
      this_ -> _CurSceneId++; if ( this_ -> _CurSceneId == this_ -> _SceneFiles.size() ) this_ -> _CurSceneId = 0; this_ -> _ReloadScene = true; break;
    case GLFW_KEY_LEFT_CONTROL:
      this_ -> _KeyState._KeyLeftCTRL = false; break;
    case GLFW_KEY_ESCAPE:
      this_ -> _KeyState._KeyEsc = false; break;
    case GLFW_KEY_PAGE_DOWN:
    {
      this_ -> _Settings._RenderScale = std::max(this_ -> _Settings._RenderScale - 5, 5);
      std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;
      this_ -> _UpdateFrameBuffers = true;
      break;
    }
    case GLFW_KEY_PAGE_UP:
    {
      this_ -> _Settings._RenderScale = std::min(this_ -> _Settings._RenderScale + 5, 300);
      std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;
      this_ -> _UpdateFrameBuffers = true;
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
Test4::Test4( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: BaseTest(iMainWindow, iScreenWidth, iScreenHeight)
{
  _Settings._RenderScale = 100;

  _Settings._WindowResolution.x = iScreenWidth;
  _Settings._WindowResolution.y = iScreenHeight;
  _Settings._RenderResolution.x = iScreenWidth  * ( _Settings._RenderScale * 0.01f );
  _Settings._RenderResolution.y = iScreenHeight * ( _Settings._RenderScale * 0.01f );
  ResizeImageBuffers();

  _NbThreadsMax = std::thread::hardware_concurrency();
  _NbThreads = _NbThreadsMax;

  JobSystem::Get().Initialize(_NbThreads);
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test4::~Test4()
{
  glDeleteFramebuffers(1, &_FrameBufferID);

  glDeleteTextures(1, &_ScreenTextureID);
  glDeleteTextures(1, &_ImageTextureID);

  for ( auto fileName : _SceneNames )
    delete[] fileName;
  for ( auto fileName : _BackgroundNames )
    delete[] fileName;
}

// ----------------------------------------------------------------------------
// InitializeSceneFiles
// ----------------------------------------------------------------------------
int Test4::InitializeSceneFiles()
{
  tinydir_dir dir;
  tinydir_open_sorted(&dir, g_AssetsDir.c_str());
  
  for ( int i = 0; i < dir.n_files; ++i )
  {
    tinydir_file file;
    tinydir_readfile_n(&dir, &file, i);
  
    std::string extension(file.extension);
    if ( "scene" == extension )
    {
      char * filename = new char[256];
      snprintf(filename, 256, "%s", file.name);
      _SceneNames.push_back(filename);
      _SceneFiles.push_back(g_AssetsDir + std::string(file.name));

      std::size_t found = _SceneFiles[_SceneFiles.size()-1].find("TexturedBox.scene");
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
int Test4::InitializeBackgroundFiles()
{
  std::string bgdPath = g_AssetsDir + "skyboxes\\";

  tinydir_dir dir;
  tinydir_open_sorted(&dir, bgdPath.c_str());
  
  for ( int i = 0; i < dir.n_files; ++i )
  {
    tinydir_file file;
    tinydir_readfile_n(&dir, &file, i);
  
    std::string extension(file.extension);
    if ( ( "hdr" == extension ) || ( "HDR" == extension ) )
    {
      char * filename = new char[256];
      snprintf(filename, 256, "%s", file.name);
      _BackgroundNames.push_back(filename);
      _BackgroundFiles.push_back(bgdPath + std::string(file.name));

      std::size_t found = _BackgroundFiles[_BackgroundFiles.size()-1].find("alps_field_2k.hdr");
      if ( std::string::npos != found )
        _CurBackgroundId = _BackgroundFiles.size()-1;
    }
  }
  
  tinydir_close(&dir);

  return 0;
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
  while ( _LastDeltas.size() > 10 )
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
// ---------------------------------------------------------------------------
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
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow.get(), true);
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
    static const char * YESorNO[]               = { "No", "Yes" };
    static const char * COLORorDEPTHorNORMALS[] = { "Color", "Depth", "Normals" };
    static const char * NEARESTorBILNEAR[]      = { "Nearest", "Bilinear" };
    static const char * PHONGorFLAT[]           = { "Flat", "Phong" };

    float zNear = 0.f, zFar = 0.f;
    _Scene -> GetCamera().GetZNearFar(zNear, zFar);

    ImGui::Begin("Test 4");

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,255,0,255));
    ImGui::Text("Frame time : %.3f ms/frame (%.1f FPS)", _AverageDelta * 1000.f, _FrameRate);
    ImGui::PopStyleColor();

    if (ImGui::CollapsingHeader("Scene "))
    {
      int selectedSceneId = _CurSceneId;
      if ( ImGui::Combo("Scene", &selectedSceneId, _SceneNames.data(), _SceneNames.size()) )
      {
        if ( selectedSceneId != _CurSceneId )
        {
          _CurSceneId = selectedSceneId;
          _ReloadScene = true;
        }
      }

      int selectedBgdId = _CurBackgroundId;
      if ( ImGui::Combo("Background", &selectedBgdId, _BackgroundNames.data(), _BackgroundNames.size()) )
      {
        if ( selectedBgdId != _CurBackgroundId )
        {
          _CurBackgroundId = selectedBgdId;
          _ReloadBackground = true;
        }
      }

      int enableBG = !!_Settings._EnableBackGround;
      ImGui::Combo("Show background", &enableBG, YESorNO, 2);
      _Settings._EnableBackGround = !!enableBG;

      float rgb[3] = { _Settings._BackgroundColor.r, _Settings._BackgroundColor.g, _Settings._BackgroundColor.b };
      if ( ImGui::ColorEdit3("Default", rgb) )
      {
        _Settings._BackgroundColor.r = rgb[0];
        _Settings._BackgroundColor.g = rgb[1];
        _Settings._BackgroundColor.b = rgb[2];
      }
    }

    if (ImGui::CollapsingHeader("Rendering"))
    {
      ImGui::Text("Window width %d: height : %d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
      ImGui::Text("Render width %d: height : %d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);

      ImGui::Text("Render image      : %.3f ms/frame", _RenderImgElapsed * 1000.f);
      ImGui::Text("Render background : %.3f ms/frame", _RenderBgdElapsed * 1000.f);
      ImGui::Text("Render scene      : %.3f ms/frame", _RenderScnElapsed * 1000.f);
      ImGui::Text("Render to texture : %.3f ms/frame", _RTTElapsed * 1000.f);
      ImGui::Text("Render to screen  : %.3f ms/frame", _RTSElapsed * 1000.f);
    }

    if (ImGui::CollapsingHeader("Settings"))
    {
      ImGui::Text("Render scale            : %d %%", _Settings._RenderScale);

      int scale = _Settings._RenderScale;
      if ( ImGui::SliderInt("Render scale", &scale, 5, 200) )
      {
        _Settings._RenderScale = scale;
        _UpdateFrameBuffers = true;
      }

      float fov = _Scene -> GetCamera().GetFOVInDegrees();
      if ( ImGui::SliderFloat("FOV", &fov, 5.f, 150.f) )
        _Scene -> GetCamera().SetFOVInDegrees(fov);

      float zVal = zNear;
      if ( ImGui::SliderFloat("zNear", &zVal, 0.01f, std::min(10.f, zFar)) )
      {
        zNear = zVal;
        _Scene -> GetCamera().SetZNearFar(zNear, zFar);
      }

      zVal = zFar;
      if ( ImGui::SliderFloat("zFar", &zVal, zNear + 0.01f, 10000.f) )
      {
        zFar = zVal;
        _Scene -> GetCamera().SetZNearFar(zNear, zFar);
      }

      int bufferChoice = _ColorDepthOrNormalsBuffer;
      ImGui::Combo("Buffer", &bufferChoice, COLORorDEPTHorNORMALS, 3);
      _ColorDepthOrNormalsBuffer = bufferChoice;

      int sampling = (int)_BilinearSampling;
      ImGui::Combo("Texture sampling", &sampling, NEARESTorBILNEAR, 2);
      _BilinearSampling = !!sampling;

      int shadingType = (int)_ShadingType;
      ImGui::Combo("Shading", &shadingType, PHONGorFLAT, 2);
      _ShadingType = (ShadingType)shadingType;

      int numThreads = _NbThreads;
      if ( ImGui::SliderInt("Nb Threads", &numThreads, 1, _NbThreadsMax) && ( numThreads > 0 ) )
      {
        _NbThreads = numThreads;
        JobSystem::Get().Initialize(_NbThreads);
      }
    }

    if (ImGui::CollapsingHeader("Controls"))
    {
      ImGui::Text("W,S,A,D      : Move camera");
      ImGui::Text("Esc          : Exit test");
      ImGui::Text("R            : Reload scene");
      ImGui::Text("N            : Next scene");
      ImGui::Text("Y            : Switch Color/Depth/Normal buffer view");
      ImGui::Text("T            : (toggle) Linear/Bilinear sampling");
      ImGui::Text("L            : (toggle) Phong/Flat shading");
      ImGui::Text("B            : (toggle) Enable/Disable background");
      ImGui::Text("K            : (toggle) Show/no Show wires");
      ImGui::Text("Page up/down : increase/decrease render scale");
      ImGui::Text("left/right   : increase/decrease fov");
      ImGui::Text("up/down      : increase/decrease zNear");
      ImGui::Text("left ctrl + up/down : increase/decrease zFar");
    }

    ImGui::End();
  }

  // Rendering
  ImGui::Render();

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ----------------------------------------------------------------------------
// RenderBackground
// ----------------------------------------------------------------------------
int Test4::RenderBackground( float iTop, float iRight )
{
  double startTime = glfwGetTime();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  float zNear, zFar;
  _Scene -> GetCamera().GetZNearFar(zNear, zFar);

  if ( _Settings._EnableBackGround )
  {
    Vec3 bottomLeft = _Scene -> GetCamera().GetForward() * zNear - iRight * _Scene -> GetCamera().GetRight() - iTop * _Scene -> GetCamera().GetUp();
    Vec3 dX = _Scene -> GetCamera().GetRight() * ( 2 * iRight / width );
    Vec3 dY = _Scene -> GetCamera().GetUp() * ( 2 * iTop / height );

    //std::vector<std::thread> threads;
    //threads.reserve(_NbThreads);

    //for ( int i = 0; i < _NbThreads; ++i )
    //{
    //  int startY = ( height / _NbThreads ) * i;
    //  int endY = ( i == _NbThreads-1 ) ? ( height ) : ( startY + ( height / _NbThreads ) );

    //  threads.emplace_back(std::thread(&Test4::RenderBackgroundRows, this, startY, endY, bottomLeft, dX, dY));
    //}

    //for ( auto & thread : threads )
    //{
    //  thread.join();
    //}

    for ( int i = 0; i < height; ++i )
      JobSystem::Get().Execute([this, i, bottomLeft, dX, dY](){ this -> RenderBackgroundRows(i, i+1, bottomLeft, dX, dY); });

    JobSystem::Get().Wait();
  }
  else
  {
    const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
    std::fill(policy, _ColorBuffer.begin(), _ColorBuffer.end(), backgroundColor);
  }

  _RenderBgdElapsed = glfwGetTime() - startTime;

  return 0;
}

// ----------------------------------------------------------------------------
// RenderBackgroundRows
// ----------------------------------------------------------------------------
void Test4::RenderBackgroundRows( int iStartY, int iEndY, Vec3 iBottomLeft, Vec3 iDX, Vec3 iDY )
{
  int width  = _Settings._RenderResolution.x;

  for ( int y = iStartY; y < iEndY; ++y )
  {
    for ( int x = 0; x < width; ++x  )
    {
      Vec3 worldP = glm::normalize(iBottomLeft + iDX * (float)x + iDY * (float)y);
      _ColorBuffer[x + width * y] = this -> SampleSkybox(worldP);
    }
  }
}

// ----------------------------------------------------------------------------
// SampleSkybox
// ----------------------------------------------------------------------------
Vec4 Test4::SampleSkybox( const Vec3 & iDir )
{
  std::vector<Texture*> & textures = _Scene -> GetTextures();

  if ( ( _SkyboxTexId >= 0 ) && ( _SkyboxTexId < textures.size() ) )
  {
    Texture * skyboxTexture = textures[_SkyboxTexId];

    if ( skyboxTexture )
    {
      float theta = std::asin(iDir.y);
      float phi   = std::atan2(iDir.z, iDir.x);
      Vec2 uv = Vec2(.5f + phi * M_1_PI * .5f, .5f - theta * M_1_PI) + Vec2(_Settings._SkyBoxRotation, 0.0);

      if ( _BilinearSampling )
        return skyboxTexture -> BiLinearSample(uv);
      else
        return skyboxTexture -> Sample(uv);
    }
  }

  return Vec4(0.f);
}

// ----------------------------------------------------------------------------
// RenderScene
// ----------------------------------------------------------------------------
int Test4::RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP )
{
  double startTime = glfwGetTime();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  float zNear, zFar;
  _Scene -> GetCamera().GetZNearFar(zNear, zFar);

  std::fill(policy, _DepthBuffer.begin(), _DepthBuffer.end(), zFar);

  _RenderScnElapsed = glfwGetTime() - startTime;

  return 0;
}

// ----------------------------------------------------------------------------
// UpdateImage
// ----------------------------------------------------------------------------
int Test4::UpdateImage()
{
  double startTime = glfwGetTime();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;
  float ratio = width / float(height);

  Mat4x4 MV;
  _Scene -> GetCamera().ComputeLookAtMatrix(MV);

  float top, right;
  Mat4x4 P;
  _Scene -> GetCamera().ComputePerspectiveProjMatrix(ratio, P, &top, &right);

  Mat4x4 MVP = P * MV;

  RenderBackground(top, right);

  RenderScene(MV, P);

  _RenderImgElapsed = glfwGetTime() - startTime;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test4::InitializeScene()
{
  Scene * newScene = nullptr;
  if ( !Loader::LoadScene(_SceneFiles[_CurSceneId], newScene, _Settings) || !newScene )
  {
    std::cout << "Failed to load scene : " << _SceneFiles[_CurSceneId] << std::endl;
    return 1;
  }
  _Scene.reset(newScene);

  // Scene should contain at least one light
  Light * firstLight = _Scene -> GetLight(0);
  if ( !firstLight )
  {
    Light newLight;
    _Scene -> AddLight(newLight);
    firstLight = _Scene -> GetLight(0);
  }

  _Scene -> CompileMeshData(_Settings._TextureSize);

  // Resize Image Buffer
  this -> ResizeImageBuffers();

  _ReloadScene = false;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeBackground
// ----------------------------------------------------------------------------
int Test4::InitializeBackground()
{
  if ( _BackgroundFiles.size() )
  {
    if ( ( _CurBackgroundId < 0 ) || ( _CurBackgroundId >= _BackgroundFiles.size() ) )
      _CurBackgroundId = 0;

    if ( _Scene )
    {
      _SkyboxTexId = _Scene -> AddTexture(_BackgroundFiles[_CurBackgroundId], 4, TexFormat::TEX_FLOAT);
      if ( _SkyboxTexId >= 0 )
      {
        std::vector<Texture*> & textures = _Scene -> GetTextures();

        Texture * skyboxTexture = textures[_SkyboxTexId];
        if ( !skyboxTexture )
          return 1;
      }
    }
    else
      return 1;
  }

  _ReloadBackground = false;

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
  if ( _ReloadScene )
  {
    if ( 0 != InitializeScene() )
      return 1;

    glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
    glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);

    glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    _ReloadBackground = true;
  }
  if ( !_Scene )
    return 1;

  if ( _ReloadBackground )
  {
    if ( 0 != InitializeBackground() )
      return 1;
  }

  // Mouse input
  {
    const float MouseSensitivity[5] = { 1.f, 0.5f, 0.01f, 0.01f, 0.01f }; // Yaw, Pitch, StafeRight, StrafeUp, ZoomIn

    double oldMouseX = _MouseState._MouseX;
    double oldMouseY = _MouseState._MouseY;
    glfwGetCursorPos(_MainWindow.get(), &_MouseState._MouseX, &_MouseState._MouseY);

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

  if ( _UpdateFrameBuffers )
  {
    this -> ResizeImageBuffers();

    glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
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

  glfwSetWindowTitle(_MainWindow.get(), GetTestHeader());
  glfwSetWindowUserPointer(_MainWindow.get(), this);

  glfwSetFramebufferSizeCallback(_MainWindow.get(), Test4::FramebufferSizeCallback);
  glfwSetMouseButtonCallback(_MainWindow.get(), Test4::MousebuttonCallback);
  glfwSetKeyCallback(_MainWindow.get(), Test4::KeyCallback);

  glfwMakeContextCurrent(_MainWindow.get());
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  if ( 0 != InitializeUI() )
  {
    std::cout << "Failed to initialize ImGui!" << std::endl;
    return 1;
  }

  // Init openGL scene
  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    return 1;
  }

  // Initialize the scene
  if ( ( 0 != InitializeSceneFiles() ) || ( 0 != InitializeScene() ) )
  {
    std::cout << "ERROR: Scene initialization failed!" << std::endl;
    return 1;
  }

  // Initialize the Background
  if ( ( 0 != InitializeBackgroundFiles() ) || ( 0 != InitializeBackground() ) )
  {
    std::cout << "ERROR: Background initialization failed!" << std::endl;
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
  glfwSetWindowSize(_MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glViewport(0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y);
  glDisable(GL_DEPTH_TEST);

  glBindTexture(GL_TEXTURE_2D, _ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  _CPULoopTime = glfwGetTime();
  while ( !glfwWindowShouldClose(_MainWindow.get()) && !_KeyState._KeyEsc )
  {
    UpdateCPUTime();

    glfwPollEvents();

    ProcessInput();

    if ( 0 != UpdateScene() )
      break;

    UpdateImage();

    UpdateTextures();

    UpdateUniforms();

    // Render to texture
    RenderToTexture();

    // Render to screen
    RenderToSceen();

    DrawUI();

    glfwSwapBuffers(_MainWindow.get());
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  return ret;
}

}
