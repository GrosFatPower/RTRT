#include "Test4.h"

#include "QuadMesh.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "Scene.h"
#include "Camera.h"
#include "Light.h"
#include "Loader.h"
#include "SutherlandHodgman.h"
#include "JobSystem.h"
#include "Util.h"

#include "tinydir.h"

#include "stb_image_write.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <unordered_map>

#include <iostream>
#include <execution>
//#include <omp.h>
#include <thread>

constexpr std::execution::parallel_policy policy = std::execution::par;

namespace fs = std::filesystem;

/**
 * The vertices vector contains a lot of duplicated vertex data,
 * because many vertices are included in multiple triangles.
 * We should keep only the unique vertices and use
 * the index buffer to reuse them whenever they come up.
 * https://en.cppreference.com/w/cpp/utility/hash
 */
namespace std
{
  template <>
  struct hash<RTRT::Test4::Vertex>
  {
    size_t operator()(RTRT::Test4::Vertex const & iV) const
    {
      return
        ( (hash<Vec3>()(iV._WorldPos))
        ^ (hash<Vec3>()(iV._Normal))
        ^ (hash<Vec2>()(iV._UV)) );
    }
  };
}

namespace RTRT
{

const char * Test4::GetTestHeader() { return "Test 4 : CPU Rasterizer"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

static bool g_RenderToFile = false;
static fs::path g_FilePath;

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

  _Settings._Gamma = 1.5f;
  _Settings._Exposure = 1.f;

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
    DeleteTab(fileName);
  for ( auto fileName : _BackgroundNames )
    DeleteTab(fileName);

  DeleteTab(_RasterTrianglesBuf);
  DeleteTab(_NbRasterTriPerBuf);
}

// ----------------------------------------------------------------------------
// InitializeSceneFiles
// ----------------------------------------------------------------------------
int Test4::InitializeSceneFiles()
{
  std::vector<std::string> sceneNames;
  Util::RetrieveSceneFiles(g_AssetsDir, _SceneFiles, &sceneNames);

  for ( int i = 0; i < sceneNames.size(); ++i )
  {
    char * filename = new char[256];
    snprintf(filename, 256, "%s", sceneNames[i].c_str());
    _SceneNames.push_back(filename);

    if ( "TexturedBox.scene" == sceneNames[i] )
      _CurSceneId = i;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeBackgroundFiles
// ----------------------------------------------------------------------------
int Test4::InitializeBackgroundFiles()
{
  std::vector<std::string> backgroundNames;
  Util::RetrieveBackgroundFiles(g_AssetsDir + "HDR\\", _BackgroundFiles, &backgroundNames);

  for ( int i = 0; i < backgroundNames.size(); ++i )
  {
    char * filename = new char[256];
    snprintf(filename, 256, "%s", backgroundNames[i].c_str());
    _BackgroundNames.push_back(filename);

    if ( "alps_field_2k.hdr" == backgroundNames[i] )
      _CurBackgroundId = i;
  }

  return 0;
}

// ----------------------------------------------------------------------------
// ResizeImageBuffers
// ----------------------------------------------------------------------------
void Test4::ResizeImageBuffers()
{
  _Settings._RenderResolution.x = _Settings._WindowResolution.x * ( _Settings._RenderScale * 0.01f );
  _Settings._RenderResolution.y = _Settings._WindowResolution.y * ( _Settings._RenderScale * 0.01f );

  _ImageBuffer._ColorBuffer.resize(_Settings._RenderResolution.x  * _Settings._RenderResolution.y);
  _ImageBuffer._DepthBuffer.resize(_Settings._RenderResolution.x  * _Settings._RenderResolution.y);
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

  _AccuDelta += _TimeDelta;
  _NbFrames++;

  if ( ( _FrameRate == 0. ) && _AccuDelta && ( _NbFrames > 1 ) )
  {
    _FrameRate = _NbFrames / _AccuDelta;
  }
  else if ( _AccuDelta >= 1. )
  {
    _FrameRate = (double)_NbFrames * .75 + _FrameRate * .25;
    while ( _AccuDelta > 1. )
      _AccuDelta -= 1.;
    _NbFrames = 0;
  }
   
  if ( _AverageDelta == 0. )
    _AverageDelta = _TimeDelta;
  else
    _AverageDelta = _TimeDelta * .25 + _AverageDelta * .75;

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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, &_ImageBuffer._ColorBuffer[0]);
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
    glUniform2f(glGetUniformLocation(RTSProgramID, "u_RenderRes"), _Settings._RenderResolution.x, _Settings._RenderResolution.y );
    glUniform1f(glGetUniformLocation(RTSProgramID, "u_Gamma"), _Settings._Gamma );
    glUniform1f(glGetUniformLocation(RTSProgramID, "u_Exposure"), _Settings._Exposure );
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_ToneMapping"), ( _Settings._ToneMapping ? 1 : 0 ) );
    glUniform1i(glGetUniformLocation(RTSProgramID, "u_FXAA"), ( _Settings._FXAA ? 1 : 0 ) );
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
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, &_ImageBuffer._ColorBuffer[0]);
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
// RenderToFile
// ----------------------------------------------------------------------------
bool Test4::RenderToFile( const std::filesystem::path & iFilepath )
{
  // Generate output texture
  GLuint frameCaptureTextureID = 0;
  glGenTextures( 1, &frameCaptureTextureID );
  glActiveTexture( GL_TEXTURE30 );
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

    glActiveTexture( GL_TEXTURE30  );
    glBindTexture( GL_TEXTURE_2D, frameCaptureTextureID );
    glActiveTexture( GL_TEXTURE0 );
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

    glActiveTexture( GL_TEXTURE30 );
    glBindTexture( GL_TEXTURE_2D, frameCaptureTextureID );
    glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData );
    stbi_flip_vertically_on_write( true );
    saved = stbi_write_png( iFilepath.string().c_str(), w, h, 4, frameData, w * 4 );

    delete[] frameData;
    glDeleteFramebuffers( 1, &frameBufferID );
    glDeleteTextures( 1, &frameCaptureTextureID );
  }

  if ( saved && fs::exists( iFilepath ) )
    std::cout << "Frame saved in " << fs::absolute( iFilepath ) << std::endl;
  else
    std::cout << "ERROR : Failed to save screen capture in " << fs::absolute( iFilepath ) << std::endl;

  return saved ? true : false;
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

    if ( ImGui::Button( "Capture image" ) )
    {
      g_FilePath = "./Test4_" + std::string( _SceneNames[_CurSceneId] ) + ".png";
      g_RenderToFile = true;
    }

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

      ImGui::Text("Render image            : %.3f ms/frame", _RenderImgElapsed * 1000.f);
      ImGui::Text("Render background       : %.3f ms/frame", _RenderBgdElapsed * 1000.f);
      ImGui::Text("Render scene            : %.3f ms/frame", _RenderScnElapsed * 1000.f);
      ImGui::Text("Render to texture       : %.3f ms/frame", _RTTElapsed * 1000.f);
      ImGui::Text("Render to screen        : %.3f ms/frame", _RTSElapsed * 1000.f);

      ImGui::Text("Nb threads              : %d", _NbThreads);
      ImGui::Text("Nb vertices             : %d", _Vertices.size());
      ImGui::Text("Nb triangles            : %d", _Triangles.size());

      unsigned int nbRasterTris = 0;
      for ( int i = 0; i < _NbThreads; ++i )
        if ( _NbRasterTriPerBuf[i] ) nbRasterTris += _NbRasterTriPerBuf[i];
      ImGui::Text("Nb raster triangles     : %d", nbRasterTris);
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

      static bool vSync = true;
      if ( ImGui::Checkbox( "VSync", &vSync ) )
      {
        if ( vSync )
          glfwSwapInterval( 1 );
        else
          glfwSwapInterval( 0 );
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

      int useWBuffer = !!_WBuffer;
      ImGui::Combo("W-Buffer", &useWBuffer, YESorNO, 2);
      _WBuffer = !!useWBuffer;

      int showWires = !!_ShowWires;
      ImGui::Combo("Show wires", &showWires, YESorNO, 2);
      _ShowWires = !!showWires;

      ImGui::Checkbox( "FXAA", &_Settings._FXAA );

      ImGui::Checkbox( "Tone mapping", &_Settings._ToneMapping );

      if ( _Settings._ToneMapping )
      {
        ImGui::SliderFloat( "Gamma", &_Settings._Gamma, .5f, 3.f );
        ImGui::SliderFloat( "Exposure", &_Settings._Exposure, .1f, 5.f );
      }

      int numThreads = _NbThreads;
      if ( ImGui::SliderInt("Nb Threads", &numThreads, 1, _NbThreadsMax) && ( numThreads > 0 ) )
      {
        _NbThreads = numThreads;
        JobSystem::Get().Initialize(_NbThreads);

        DeleteTab(_RasterTrianglesBuf);
        DeleteTab(_NbRasterTriPerBuf);
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

    for ( int i = 0; i < height; ++i )
      JobSystem::Get().Execute([this, i, bottomLeft, dX, dY](){ this -> RenderBackgroundRows(i, i+1, bottomLeft, dX, dY); });

    JobSystem::Get().Wait();
  }
  else
  {
    const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
    std::fill(policy, _ImageBuffer._ColorBuffer.begin(), _ImageBuffer._ColorBuffer.end(), backgroundColor);
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
      _ImageBuffer._ColorBuffer[x + width * y] = this -> SampleSkybox(worldP);
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
int Test4::RenderScene( const Mat4x4 & iMV, const Mat4x4 & iP, const Mat4x4 & iRasterM )
{
  double startTime = glfwGetTime();

  if ( _WBuffer )
  {
    float zNear, zFar;
    _Scene -> GetCamera().GetZNearFar(zNear, zFar);
    std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), zFar);
  }
  else
    std::fill(policy, _ImageBuffer._DepthBuffer.begin(), _ImageBuffer._DepthBuffer.end(), 1.f);

  int ko = ProcessVertices(iMV, iP);

  if ( !ko )
    ko = ClipTriangles(iRasterM);

  if ( !ko )
    ko = ProcessFragments();

  _RenderScnElapsed = glfwGetTime() - startTime;

  return ko;
}

// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
int Test4::ProcessVertices( const Mat4x4 & iMV, const Mat4x4 & iP )
{
  if ( !_Scene )
    return 1;

  Mat4x4 MVP = iP * iMV;

  int nbVertices = _Vertices.size();
  _ProjVerticesBuf.resize(nbVertices);
  _ProjVerticesBuf.reserve(nbVertices*2);

  //for ( int i = 0; i < _NbThreads; ++i )
  //{
  //  int startInd = ( _Vertices.size() / _NbThreads ) * i;
  //  int endInd = ( i == _NbThreads-1 ) ? ( _Vertices.size() ) : ( startInd + ( _Vertices.size() / _NbThreads ) );
  //
  //  JobSystem::Get().Execute([this, MVP, startInd, endInd](){ this -> ProcessVertices(MVP, startInd, endInd); });
  //}

  int curInd = 0;
  do
  {
    int nextInd = std::min(curInd + 512, nbVertices);

    JobSystem::Get().Execute([this, MVP, curInd, nextInd](){ this -> ProcessVertices(MVP, curInd, nextInd); });

    curInd = nextInd;

  } while ( curInd < nbVertices );

  JobSystem::Get().Wait();

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessVertices
// ----------------------------------------------------------------------------
void Test4::ProcessVertices( const Mat4x4 & iMVP, int iStartInd, int iEndInd )
{
  for ( int i = iStartInd; i < iEndInd; ++i )
  {
    Vertex & vert = _Vertices[i];

    ProjectedVertex & projVtx = _ProjVerticesBuf[i];
    VertexShader(Vec4(vert._WorldPos ,1.f), vert._UV, vert._Normal, iMVP, projVtx);
  }
}

// ----------------------------------------------------------------------------
// VertexShader
// ----------------------------------------------------------------------------
void Test4::VertexShader( const Vec4 & iVertexPos, const Vec2 & iUV, const Vec3 iNormal, const Mat4x4 iMVP, ProjectedVertex & oProjectedVertex )
{
  oProjectedVertex._ProjPos          = iMVP * iVertexPos;
  oProjectedVertex._Attrib._WorldPos = iVertexPos;
  oProjectedVertex._Attrib._UV       = iUV;
  oProjectedVertex._Attrib._Normal   = iNormal;
}

// ----------------------------------------------------------------------------
// ClipTriangles
// ----------------------------------------------------------------------------
int Test4::ClipTriangles( const Mat4x4 & iRasterM )
{
  int nbTriangles = _Triangles.size();

  if ( !_RasterTrianglesBuf )
  {
    _RasterTrianglesBuf = new std::vector<RasterTriangle>[_NbThreads];
    _NbRasterTriPerBuf = new int[_NbThreads];
    memset(_NbRasterTriPerBuf, 0, sizeof(int) * _NbThreads);

    for ( int i = 0; i < _NbThreads; ++i )
      _RasterTrianglesBuf[i].reserve(std::max(nbTriangles/_NbThreads, 1));
  }

  for ( int i = 0; i < _NbThreads; ++i )
  {
    int startInd = ( nbTriangles / _NbThreads ) * i;
    int endInd = ( i == _NbThreads-1 ) ? ( nbTriangles ) : ( startInd + ( nbTriangles / _NbThreads ) );
    if ( startInd >= endInd )
      continue;

    _RasterTrianglesBuf[i].reserve(endInd - startInd);
  
    JobSystem::Get().Execute([this, iRasterM, i, startInd, endInd](){ this -> ClipTriangles(iRasterM, i, startInd, endInd); });
  }

  JobSystem::Get().Wait();

  return 0;
}

// ----------------------------------------------------------------------------
// ClipTriangles
// SutherlandHodgman algorithm
// ----------------------------------------------------------------------------
void Test4::ClipTriangles( const Mat4x4 & iRasterM, int iThreadBin, int iStartInd, int iEndInd )
{
  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  _NbRasterTriPerBuf[iThreadBin] = 0;
  for ( int i = iStartInd; i < iEndInd; ++i )
  {
    Triangle & tri = _Triangles[i];

    uint32_t clipCode0 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[0]]._ProjPos);
    uint32_t clipCode1 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[1]]._ProjPos);
    uint32_t clipCode2 = SutherlandHodgman::ComputeClipCode(_ProjVerticesBuf[tri._Indices[2]]._ProjPos);

    if (clipCode0 | clipCode1 | clipCode2)
    {
      // Check the clipping codes correctness
      if ( !(clipCode0 & clipCode1 & clipCode2) )
      {
        Polygon poly = SutherlandHodgman::ClipTriangle(
          _ProjVerticesBuf[tri._Indices[0]]._ProjPos,
          _ProjVerticesBuf[tri._Indices[1]]._ProjPos,
          _ProjVerticesBuf[tri._Indices[2]]._ProjPos,
          (clipCode0 ^ clipCode1) | (clipCode1 ^ clipCode2) | (clipCode2 ^ clipCode0));

        for ( int j = 2; j < poly.Size(); ++j )
        {
          // Preserve winding
          Polygon::Point Points[3] = { poly[0], poly[j - 1], poly[j] };

          RasterTriangle rasterTri;
          for ( int k = 0; k < 3; ++k )
          {
            if ( Points[k]._Distances.x == 1.f )
            {
              rasterTri._Indices[k] = tri._Indices[0]; // == V0
            }
            else if ( Points[k]._Distances.y == 1.f )
            {
              rasterTri._Indices[k] = tri._Indices[1]; // == V1
            }
            else if ( Points[k]._Distances.z == 1.f )
            {
              rasterTri._Indices[k] = tri._Indices[2]; // == V2
            }
            else
            {
              ProjectedVertex newProjVert;
              newProjVert._ProjPos = Points[k]._Pos;
              newProjVert._Attrib  = _ProjVerticesBuf[tri._Indices[0]]._Attrib * Points[k]._Distances.x + 
                                     _ProjVerticesBuf[tri._Indices[1]]._Attrib * Points[k]._Distances.y +
                                     _ProjVerticesBuf[tri._Indices[2]]._Attrib * Points[k]._Distances.z;
              {
                std::unique_lock<std::mutex> lock(_ProjVerticesMutex);
                rasterTri._Indices[k] = _ProjVerticesBuf.size();
                _ProjVerticesBuf.emplace_back(newProjVert);
              }
            }

            Vec3 homogeneousProjPos;
            rasterTri._InvW[k] = 1.f / Points[k]._Pos.w;
            homogeneousProjPos.x = Points[k]._Pos.x * rasterTri._InvW[k];
            homogeneousProjPos.y = Points[k]._Pos.y * rasterTri._InvW[k];
            homogeneousProjPos.z = Points[k]._Pos.z * rasterTri._InvW[k];

            rasterTri._V[k] = MathUtil::TransformPoint(homogeneousProjPos, iRasterM);

            rasterTri._BBox.Insert(rasterTri._V[k]);
          }

          float area = MathUtil::EdgeFunction(rasterTri._V[0], rasterTri._V[1], rasterTri._V[2]);
          if ( area > 0.f )
            rasterTri._InvArea = 1.f / area;
          else
            continue;

          rasterTri._MatID  = tri._MatID;
          rasterTri._Normal = tri._Normal;

          if ( _NbRasterTriPerBuf[iThreadBin] < _RasterTrianglesBuf[iThreadBin].size() )
            _RasterTrianglesBuf[iThreadBin][_NbRasterTriPerBuf[iThreadBin]] = rasterTri;
          else
            _RasterTrianglesBuf[iThreadBin].emplace_back(rasterTri);
          _NbRasterTriPerBuf[iThreadBin]++;
        }
      }
    }
    else
    {
      // No clipping needed
      RasterTriangle rasterTri;
      for ( int j = 0; j < 3; ++j )
      {
        rasterTri._Indices[j] = tri._Indices[j];

        ProjectedVertex & projVert = _ProjVerticesBuf[tri._Indices[j]];

        Vec3 homogeneousProjPos;
        rasterTri._InvW[j] = 1.f / projVert._ProjPos.w;
        homogeneousProjPos.x = projVert._ProjPos.x * rasterTri._InvW[j];
        homogeneousProjPos.y = projVert._ProjPos.y * rasterTri._InvW[j];
        homogeneousProjPos.z = projVert._ProjPos.z * rasterTri._InvW[j];

        rasterTri._V[j] = MathUtil::TransformPoint(homogeneousProjPos, iRasterM);

        rasterTri._BBox.Insert(rasterTri._V[j]);
      }

      float area = MathUtil::EdgeFunction(rasterTri._V[0], rasterTri._V[1], rasterTri._V[2]);
      if ( area > 0.f )
        rasterTri._InvArea = 1.f / area;
      else
        continue;

      rasterTri._MatID  = tri._MatID;
      rasterTri._Normal = tri._Normal;

      if ( _NbRasterTriPerBuf[iThreadBin] < _RasterTrianglesBuf[iThreadBin].size() )
        _RasterTrianglesBuf[iThreadBin][_NbRasterTriPerBuf[iThreadBin]] = rasterTri;
      else
        _RasterTrianglesBuf[iThreadBin].emplace_back(rasterTri);
      _NbRasterTriPerBuf[iThreadBin]++;
    }
  }
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
int Test4::ProcessFragments()
{
  if ( !_Scene || !_RasterTrianglesBuf )
    return 1;

  int height = _Settings._RenderResolution.y;

  for ( int i = 0; i < _NbThreads; ++i )
  {
    int startY = ( height / _NbThreads ) * i;
    int endY = ( i == _NbThreads-1 ) ? ( height ) : ( startY + ( height / _NbThreads ) );

    JobSystem::Get().Execute([this, startY, endY](){ this -> ProcessFragments(startY, endY); });
  }

  JobSystem::Get().Wait();

  return 0;
}

// ----------------------------------------------------------------------------
// ProcessFragments
// ----------------------------------------------------------------------------
void Test4::ProcessFragments( int iStartY, int iEndY )
{
  const std::vector<Material> & Materials = _Scene -> GetMaterials();

  int width  = _Settings._RenderResolution.x;
  int height = _Settings._RenderResolution.y;

  float zNear, zFar;
  _Scene -> GetCamera().GetZNearFar(zNear, zFar);

  Uniform uniforms;
  uniforms._CameraPos        = _Scene -> GetCamera().GetPos();
  uniforms._BilinearSampling = _BilinearSampling;
  uniforms._Materials        = &_Scene -> GetMaterials();
  uniforms._Textures         = &_Scene -> GetTextures();
  for ( int i = 0; i < _Scene -> GetNbLights(); ++i )
    uniforms._Lights.push_back(*_Scene -> GetLight(i));

  for ( int i = 0; i < _NbThreads; ++i )
  {
    for ( int j = 0; j < _NbRasterTriPerBuf[i]; ++j )
    {
      RasterTriangle & tri = _RasterTrianglesBuf[i][j];

      // Backface culling
      if ( 0 )
      {
        Vec3 AB(tri._V[1] - tri._V[0]);
        Vec3 AC(tri._V[2] - tri._V[0]);
        Vec3 crossProd = glm::cross(AB,AC);
        if ( crossProd.z < 0 )
          continue;
      }

      int xMin = std::max(0,       std::min((int)std::floorf(tri._BBox._Low.x),  width - 1));
      int yMin = std::max(iStartY, std::min((int)std::floorf(tri._BBox._Low.y),  iEndY - 1 ));
      int xMax = std::max(0,       std::min((int)std::floorf(tri._BBox._High.x), width - 1));
      int yMax = std::max(iStartY, std::min((int)std::floorf(tri._BBox._High.y), iEndY - 1 ));

      for ( int y = yMin; y <= yMax; ++y )
      {
        for ( int x = xMin; x <= xMax; ++x )
        {
          Vec3 coord(x + .5f, y + .5f, 0.f);

          Vec3 W;
          W.x = MathUtil::EdgeFunction(tri._V[1], tri._V[2], coord);
          W.y = MathUtil::EdgeFunction(tri._V[2], tri._V[0], coord);
          W.z = MathUtil::EdgeFunction(tri._V[0], tri._V[1], coord);
          if ( ( W.x < 0.f )
            || ( W.y < 0.f )
            || ( W.z < 0.f ) )
            continue;

          // Perspective correction
          W *= tri._InvArea;

          W.x *= tri._InvW[0]; // W0 / -z0
          W.y *= tri._InvW[1]; // W1 / -z1
          W.z *= tri._InvW[2]; // W2 / -z2

          float Z = 1.f / (W.x + W.y + W.z);
          W *= Z;

          coord.z = W.x * tri._V[0].z + W.y * tri._V[1].z + W.z * tri._V[2].z;
          if ( _WBuffer )
          {
            if ( Z > _ImageBuffer._DepthBuffer[x + width * y] || ( Z < zNear ) )
              continue;
          }
          else
          {
            if ( coord.z > _ImageBuffer._DepthBuffer[x + width * y] || ( coord.z < -1.f ) )
              continue;
          }

          Varying Attrib[3];
          Attrib[0] = _ProjVerticesBuf[tri._Indices[0]]._Attrib;
          Attrib[1] = _ProjVerticesBuf[tri._Indices[1]]._Attrib;
          Attrib[2] = _ProjVerticesBuf[tri._Indices[2]]._Attrib;

          Fragment frag;
          frag._FragCoords = coord;
          frag._MatID      = tri._MatID;
          frag._Attrib     = Attrib[0] * W.x + Attrib[1] * W.y + Attrib[2] * W.z;

         if ( ShadingType::Phong == _ShadingType )
            frag._Attrib._Normal = glm::normalize(frag._Attrib._Normal);
          else
            frag._Attrib._Normal = tri._Normal;

          Vec4 fragColor(1.f);

          if ( 1 == _ColorDepthOrNormalsBuffer )
            FragmentShader_Depth(frag, uniforms, fragColor);
          else if ( 2 == _ColorDepthOrNormalsBuffer )
            FragmentShader_Normal(frag, uniforms, fragColor);
          else
            FragmentShader_Color(frag, uniforms, fragColor);

          if ( _ShowWires )
          {
            Vec4 wireColor(1.f);
            FragmentShader_Wires(frag, tri._V, uniforms, wireColor);
            fragColor.x = glm::mix(fragColor.x, wireColor.x, wireColor.w);
            fragColor.y = glm::mix(fragColor.y, wireColor.y, wireColor.w);
            fragColor.z = glm::mix(fragColor.z, wireColor.z, wireColor.w);
          }

          _ImageBuffer._ColorBuffer[x + width * y] = fragColor;
          if ( _WBuffer )
            _ImageBuffer._DepthBuffer[x + width * y] = Z;
          else
            _ImageBuffer._DepthBuffer[x + width * y] = coord.z;

        }
      }
    }
  }
}

// ----------------------------------------------------------------------------
// FragmentShader_Color
// ----------------------------------------------------------------------------
void Test4::FragmentShader_Color( const Fragment & iFrag, Uniform & iUniforms, Vec4 & oColor )
{
  Vec4 albedo;
  if ( iFrag._MatID >= 0 )
  {
    const Material & mat = (*iUniforms._Materials)[iFrag._MatID];
    if ( mat._BaseColorTexId >= 0 )
    {
      const Texture * tex = (*iUniforms._Textures)[mat._BaseColorTexId];
      if ( iUniforms._BilinearSampling )
        albedo = tex -> BiLinearSample(iFrag._Attrib._UV);
      else
        albedo = tex -> Sample(iFrag._Attrib._UV);
    }
    else
      albedo = Vec4(mat._Albedo, 1.f);
  }

  // Shading
  Vec4 alpha(0.f, 0.f, 0.f, 0.f);
  for ( const auto & light : iUniforms._Lights )
  {
    float ambientStrength = .1f;
    float diffuse = 0.f;
    float specular = 0.f;

    Vec3 dirToLight = glm::normalize(light._Pos - iFrag._Attrib._WorldPos);
    diffuse = std::max(0.f, glm::dot(iFrag._Attrib._Normal, dirToLight));

    Vec3 viewDir =  glm::normalize(iUniforms._CameraPos - iFrag._Attrib._WorldPos);
    Vec3 reflectDir = glm::reflect(-dirToLight, iFrag._Attrib._Normal);

    static float specularStrength = 0.5f;
    specular = pow(std::max(glm::dot(viewDir, reflectDir), 0.f), 32) * specularStrength;

    alpha += std::min(diffuse+ambientStrength+specular, 1.f) * Vec4(glm::normalize(light._Emission), 1.f);
  }

  oColor = MathUtil::Min(albedo * alpha, Vec4(1.f));
}

// ----------------------------------------------------------------------------
// FragmentShader_Depth
// ----------------------------------------------------------------------------
void Test4::FragmentShader_Depth( const Fragment  & iFrag, Uniform & iUniforms, Vec4 & oColor )
{
  oColor = Vec4(Vec3(iFrag._FragCoords.z + 1.f) * .5f, 1.f);
  return;
}

// ----------------------------------------------------------------------------
// FragmentShader_Normal
// ----------------------------------------------------------------------------
void Test4::FragmentShader_Normal( const Fragment  & iFrag, Uniform & iUniforms, Vec4 & oColor )
{
  oColor = Vec4(glm::abs(iFrag._Attrib._Normal),1.f);
  return;
}

// ----------------------------------------------------------------------------
// FragmentShader_Wires
// ----------------------------------------------------------------------------
void Test4::FragmentShader_Wires( const Fragment & iFrag, const Vec3 iVertCoord[3], Uniform & iUniforms, Vec4 & oColor )
{
  Vec2 P(iFrag._FragCoords);
  if ( ( MathUtil::DistanceToSegment(iVertCoord[0], iVertCoord[1], P) <= 1.f )
    || ( MathUtil::DistanceToSegment(iVertCoord[1], iVertCoord[2], P) <= 1.f )
    || ( MathUtil::DistanceToSegment(iVertCoord[2], iVertCoord[0], P) <= 1.f ) )
  {
    oColor = Vec4(1.f, 0.f, 0.f, 1.f);
  }
  else
    oColor = Vec4(0.f, 0.f, 0.f, 0.f);
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

  Mat4x4 RasterM;
  _Scene -> GetCamera().ComputeRasterMatrix(width, height, RasterM);

  Mat4x4 MVP = P * MV;

  RenderBackground(top, right);

  RenderScene(MV, P, RasterM);

  _RenderImgElapsed = glfwGetTime() - startTime;

  return 0;
}

// ----------------------------------------------------------------------------
// InitializeScene
// ----------------------------------------------------------------------------
int Test4::InitializeScene()
{
  Scene * newScene = new Scene;
  if ( !newScene || !Loader::LoadScene(_SceneFiles[_CurSceneId], *newScene, _Settings) )
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

  // Compile mesh data
  _Scene -> CompileMeshData(Vec2i(0), false, false);

  // Load _Triangles
  _Vertices.clear();
  _Triangles.clear();
  _ProjVerticesBuf.clear();
  DeleteTab(_RasterTrianglesBuf);
  DeleteTab(_NbRasterTriPerBuf);

  const std::vector<Vec3i>    & Indices   = _Scene -> GetIndices();
  const std::vector<Vec3>     & Vertices  = _Scene -> GetVertices();
  const std::vector<Vec3>     & Normals   = _Scene -> GetNormals();
  const std::vector<Vec3>     & UVMatIDs  = _Scene -> GetUVMatID();
  const std::vector<Material> & Materials = _Scene -> GetMaterials();
  const std::vector<Texture*> & Textures  = _Scene -> GetTextures();
  const int nbTris = Indices.size() / 3;

  std::unordered_map<Vertex, int> VertexIDs;
  VertexIDs.reserve(Vertices.size());

  _Triangles.resize(nbTris);
  for ( int i = 0; i < nbTris; ++i )
  {
    Triangle & tri = _Triangles[i];

    Vec3i Index[3];
    Index[0] = Indices[i*3];
    Index[1] = Indices[i*3+1];
    Index[2] = Indices[i*3+2];

    Vertex Vert[3];
    for ( int j = 0; j < 3; ++j )
    {
      Vert[j]._WorldPos = Vertices[Index[j].x];
      Vert[j]._UV       = Vec2(0.f);
      Vert[j]._Normal   = Vec3(0.f);
    }

    Vec3 vec1(Vert[1]._WorldPos.x-Vert[0]._WorldPos.x, Vert[1]._WorldPos.y-Vert[0]._WorldPos.y, Vert[1]._WorldPos.z-Vert[0]._WorldPos.z);
    Vec3 vec2(Vert[2]._WorldPos.x-Vert[0]._WorldPos.x, Vert[2]._WorldPos.y-Vert[0]._WorldPos.y, Vert[2]._WorldPos.z-Vert[0]._WorldPos.z);
    tri._Normal = glm::normalize(glm::cross(vec1, vec2));

    for ( int j = 0; j < 3; ++j )
    {
      if ( Index[j].y >= 0 )
        Vert[j]._Normal = Normals[Index[j].y];
      else
        Vert[j]._Normal = tri._Normal;

      if ( Index[j].z >= 0 )
         Vert[j]._UV = Vec2(UVMatIDs[Index[j].z].x, UVMatIDs[Index[j].z].y);
    }

    tri._MatID = (int)UVMatIDs[Index[0].z].z;

    for ( int j = 0; j < 3; ++j )
    {
      int idx = 0;
      if ( 0 == VertexIDs.count(Vert[j]) )
      {
        idx = (int)_Vertices.size();
        VertexIDs[Vert[j]] = idx;
        _Vertices.push_back(Vert[j]);
      }
      else
        idx = VertexIDs[Vert[j]];

      tri._Indices[j] = idx;
    }
  }

  // Resize Image Buffer
  this -> ResizeImageBuffers();

  _Settings._Gamma = 1.5f;
  _Settings._Exposure = 1.f;

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

  do
  {
    if ( !_MainWindow )
    {
      ret = 1;
      break;
    }

    glfwSetWindowTitle( _MainWindow.get(), GetTestHeader() );
    glfwSetWindowUserPointer( _MainWindow.get(), this );

    glfwSetFramebufferSizeCallback( _MainWindow.get(), Test4::FramebufferSizeCallback );
    glfwSetMouseButtonCallback( _MainWindow.get(), Test4::MousebuttonCallback );
    glfwSetKeyCallback( _MainWindow.get(), Test4::KeyCallback );

    glfwMakeContextCurrent( _MainWindow.get() );
    glfwSwapInterval( 1 ); // Enable vsync

    // Setup Dear ImGui context
    if ( 0 != InitializeUI() )
    {
      std::cout << "Failed to initialize ImGui!" << std::endl;
      ret = 1;
      break;
    }

    // Init openGL scene
    glewExperimental = GL_TRUE;
    if ( glewInit() != GLEW_OK )
    {
      std::cout << "Failed to initialize GLEW!" << std::endl;
      ret = 1;
      break;
    }

    // Initialize the scene
    if ( ( 0 != InitializeSceneFiles() ) || ( 0 != InitializeScene() ) )
    {
      std::cout << "ERROR: Scene initialization failed!" << std::endl;
      ret = 1;
      break;
    }

    // Initialize the Background
    if ( ( 0 != InitializeBackgroundFiles() ) || ( 0 != InitializeBackground() ) )
    {
      std::cout << "ERROR: Background initialization failed!" << std::endl;
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
    _Quad = std::make_unique<QuadMesh>();

    // Frame buffer
    if ( 0 != InitializeFrameBuffer() )
    {
      std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
      ret = 1;
      break;
    }

    // Main loop
    glfwSetWindowSize( _MainWindow.get(), _Settings._WindowResolution.x, _Settings._WindowResolution.y );
    glViewport( 0, 0, _Settings._WindowResolution.x, _Settings._WindowResolution.y );
    glDisable( GL_DEPTH_TEST );

    glBindTexture( GL_TEXTURE_2D, _ScreenTextureID );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, _Settings._RenderResolution.x, _Settings._RenderResolution.y, 0, GL_RGBA, GL_FLOAT, NULL );
    glBindTexture( GL_TEXTURE_2D, 0 );

    //ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    _CPULoopTime = glfwGetTime();
    while ( !glfwWindowShouldClose( _MainWindow.get() ) && !_KeyState._KeyEsc )
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

      // Screenshot
      if ( g_RenderToFile )
      {
        RenderToFile( g_FilePath );
        g_RenderToFile = false;
      }

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
