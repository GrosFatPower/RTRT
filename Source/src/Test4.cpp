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
      this_ -> _Settings._RenderScale -= 5; updateFrameBuffer = true; this_ -> _UpdateImageTex = true; break;
    case GLFW_KEY_PAGE_UP:
      this_ -> _Settings._RenderScale += 5; updateFrameBuffer = true; this_ -> _UpdateImageTex = true; break;
    default :
      break;
    }

    if ( this_ -> _Settings._RenderScale <= 0 )
      this_ -> _Settings._RenderScale = 5;

    std::cout << "SCALE = " << this_ -> _Settings._RenderScale << std::endl;

    if ( updateFrameBuffer )
    {
      this_ -> _Settings._RenderResolution.x = this_ -> _Settings._WindowResolution.x * ( this_ -> _Settings._RenderScale * 0.01f );
      this_ -> _Settings._RenderResolution.y = this_ -> _Settings._WindowResolution.y * ( this_ -> _Settings._RenderScale * 0.01f );

      this_ -> _Image.resize(this_ -> _Settings._RenderResolution.x  * this_ -> _Settings._RenderResolution.y);
      Vec4 backgroundColor(this_ -> _Settings._BackgroundColor.x, this_ -> _Settings._BackgroundColor.y, this_ -> _Settings._BackgroundColor.z, 1.f);
      fill(this_ -> _Image.begin(), this_ -> _Image.end(), backgroundColor);

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

  this_ -> _Image.resize(this_ -> _Settings._RenderResolution.x  * this_ -> _Settings._RenderResolution.y);
  Vec4 backgroundColor(this_ -> _Settings._BackgroundColor.x, this_ -> _Settings._BackgroundColor.y, this_ -> _Settings._BackgroundColor.z, 1.f);
  fill(this_ -> _Image.begin(), this_ -> _Image.end(), backgroundColor);

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

  _Image.resize(_Settings._RenderResolution.x  * _Settings._RenderResolution.y);

  //Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
  Vec4 backgroundColor(1.f, 0.f, 0.f, 1.f);
  fill(_Image.begin(), _Image.end(), backgroundColor);
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
    _Scene -> GetCamera().ComputePerspectiveProjMatrix(ratio, 1.f, 10000.f, P);

    Mat4x4 MVP = P * MV;

    const std::vector<Vec3> & vertices = _Scene -> GetVertices();

    const Vec4 backgroundColor(_Settings._BackgroundColor.x, _Settings._BackgroundColor.y, _Settings._BackgroundColor.z, 1.f);
    fill(_Image.begin(), _Image.end(), backgroundColor);

    for ( auto vertex : vertices )
    {
      Vec4 projVect = MVP * Vec4(vertex, 1.f);
      projVect /= projVect.w;

      if ( ( projVect.x < -ratio )
        || ( projVect.x >  ratio )
        || ( projVect.y < -1.f )
        || ( projVect.y >  1.f ) )
        //|| ( projVect.z < -1.f )
        //|| ( projVect.z >  1.f ) )
        continue;

      // Convert to raster space
      int x = std::min(width - 1, (int)((projVect.x + 1) * 0.5 * width));
      int y = std::min(height - 1, (int)((projVect.y + 1) * 0.5 * height));

      for ( int i = -1; i <=1; ++i )
      {
        for ( int j = -1; j <=1; ++j )
        {
          int indX = x + i;
          int indY = y + j;
          if ( ( indX >= 0 ) && ( indX < width )
            && ( indY >= 0 ) && ( indY < height ) )
            _Image[indX + width * indY] = Vec4(1.f, 1.f, 1.f, 1.f);
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

  // Initialize the scene
  if ( 0 != InitializeScene() )
  {
    std::cout << "ERROR: Scene initialization failed!" << std::endl;
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
