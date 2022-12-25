#include "Test1.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "Shader.h"
#include "ShaderProgram.h"
#include "QuadMesh.h"
#include "Texture.h"

#include <vector>
#include <deque>
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>

namespace RTRT
{

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static int    g_ScreenWidth  = 1920;
static int    g_ScreenHeight = 1080;

static double g_MouseX       = 0.;
static double g_MouseY       = 0.;
static bool   g_LeftClick    = false;
static bool   g_RightClick   = false;

static long  g_Frame         = 0;
static float g_FrameRate     = 60.f;
static float g_CurLoopTime   = 0.f;
static float g_TimeDelta     = 0.f;

static short g_VtxShaderNum  = 0;
static short g_FragShaderNum = 0;

static ShaderProgram * g_RTTShader = NULL;
static ShaderProgram * g_OutputShader = NULL;

static GLuint g_ScreenTextureID = 0;

static const GLfloat g_QuadVtx[] =
{
  -1.0f, -1.0f,  0.0f,
   1.0f, -1.0f,  0.0f,
   1.0f,  1.0f,  0.0f,
   1.0f,  1.0f,  0.0f,
  -1.0f,  1.0f,  0.0f,
  -1.0f, -1.0f,  0.0f
};

static const GLfloat g_QuadUVs[] =
{
  0.0f, 0.0f,
  1.0f, 0.0f,
  1.0f, 1.0f,
  1.0f, 1.0f,
  0.0f, 1.0f,
  0.0f, 0.0f,
};

// ----------------------------------------------------------------------------
// Global functions
// ----------------------------------------------------------------------------
static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (action == GLFW_PRESS)
    std::cout << "EVENT : KEY PRESSED" << std::endl;
}

static void MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    glfwGetCursorPos(window, &g_MouseX, &g_MouseY);

    if ( ( GLFW_MOUSE_BUTTON_1 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : LEFT CLICK (" << g_MouseX << "," << g_MouseY << ")" << std::endl;
      g_LeftClick = true;
    }
    else
      g_LeftClick = false;

    if ( ( GLFW_MOUSE_BUTTON_2 == button ) && ( GLFW_PRESS == action ) )
    {
      std::cout << "EVENT : RIGHT CLICK (" << g_MouseX << "," << g_MouseY << ")" << std::endl;
      g_RightClick = true;
    }
    else
      g_RightClick = false;
  }
}

static void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;

  g_ScreenWidth  = width;
  g_ScreenHeight = height;

  glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);

  glBindTexture(GL_TEXTURE_2D, g_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, g_ScreenWidth, g_ScreenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
}

static int RecompileShaders()
{
  if ( g_RTTShader )
  {
    g_RTTShader -> StopUsing();
    delete g_RTTShader;
  }
  g_RTTShader = NULL;

  if ( g_OutputShader )
  {
    g_OutputShader -> StopUsing();
    delete g_OutputShader;
  }
  g_OutputShader = NULL;

  ShaderSource vertexShaderSrc;
  switch ( g_VtxShaderNum )
  {
  case 0 :
  default :
    vertexShaderSrc = Shader::LoadShader("..\\..\\shaders\\vertex_Default.glsl");
  }

  ShaderSource fragmentShaderSrc;
  switch ( g_FragShaderNum )
  {
  case 1 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Drawtexture.glsl");
    break;
  case 2 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_RayMarching.glsl");
    break;
  case 3 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_ScreenSaver.glsl");
    break;
  case 4 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Mandelbrot.glsl");
    break;
  case 5 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Whitenoise.glsl");
    break;
  case 6 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Octagrams.glsl");
    break;
  case 7 :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_FantasyEscape.glsl");
    break;
  default :
    fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Default.glsl");
  }

  g_RTTShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !g_RTTShader )
    return 0;

  fragmentShaderSrc = Shader::LoadShader("..\\..\\shaders\\fragment_Output.glsl");
  g_OutputShader = ShaderProgram::LoadShaders(vertexShaderSrc, fragmentShaderSrc);
  if ( !g_OutputShader )
    return 0;

  return 1;
}

void UpdateUniforms()
{
  if ( g_RTTShader )
  {
    g_RTTShader -> Use();
    GLuint shaderProgramID = g_RTTShader -> GetShaderProgramID();
    glUniform3f(glGetUniformLocation(shaderProgramID, "u_Resolution"), g_ScreenWidth, g_ScreenHeight, 0.f);
    glUniform4f(glGetUniformLocation(shaderProgramID, "u_Mouse"), g_MouseX, g_MouseY, (float) g_LeftClick, (float) g_RightClick);
    glUniform1f(glGetUniformLocation(shaderProgramID, "u_Time"), g_CurLoopTime);
    glUniform1f(glGetUniformLocation(shaderProgramID, "u_TimeDelta"), g_TimeDelta);
    glUniform1i(glGetUniformLocation(shaderProgramID, "u_Frame"), (int)g_Frame);
    glUniform1f(glGetUniformLocation(shaderProgramID, "u_FrameRate"), g_FrameRate);
    glUniform1i(glGetUniformLocation(shaderProgramID, "u_ScreenTexture"), 0);
    glUniform1i(glGetUniformLocation(shaderProgramID, "u_Texture"), 1);
    g_RTTShader -> StopUsing();
  }

  if ( g_OutputShader )
  {
    g_OutputShader -> Use();
    GLuint outputProgramID = g_OutputShader -> GetShaderProgramID();
    glUniform1i(glGetUniformLocation(outputProgramID, "u_ScreenTexture"), 0);
    g_OutputShader -> StopUsing();
  }
}

Test1::Test1( GLFWwindow * iMainWindow, int iScreenWidth, int iScreenHeight )
: _MainWindow(iMainWindow)
{
  g_ScreenWidth = iScreenWidth;
  g_ScreenHeight = iScreenHeight;
}

Test1::~Test1()
{
}

int Test1::Run()
{
  int ret = 0;

  if ( !_MainWindow )
    return 1;

  glfwSetWindowTitle(_MainWindow, "Test 1 : Render to texture");

  glfwSetFramebufferSizeCallback(_MainWindow, FramebufferSizeCallback);
  glfwSetMouseButtonCallback(_MainWindow, MousebuttonCallback);
  //glfwSetKeyCallback(_MainWindow, keyCallback);

  glfwMakeContextCurrent(_MainWindow);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsLight();

  // Load Fonts
  io.Fonts->AddFontDefault();

  // Setup Platform/Renderer backends
  const char* glsl_version = "#version 130";
  ImGui_ImplGlfw_InitForOpenGL(_MainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Init openGL scene

  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    glfwTerminate();
    return 1;
  }

  if ( !RecompileShaders() || !g_RTTShader || !g_OutputShader )
  {
    std::cout << "Shader compilation failed!" << std::endl;
    glfwTerminate();
    return 1;
  }

  GLuint shaderProgramID = g_RTTShader -> GetShaderProgramID();

  Texture backgroundTex;
  if ( !backgroundTex.Load("..\\..\\Resources\\Img\\nature.png", 4) )
  {
    glfwTerminate();
    return 1;
  }

  // Quad
  QuadMesh * quad = new QuadMesh();

  // Texture
  GLuint textureID;
  glGenTextures(1, &textureID);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, backgroundTex.GetWidth(), backgroundTex.GetHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, backgroundTex.GetUCData());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Screen texture
  glGenTextures(1, &g_ScreenTextureID);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, g_ScreenWidth, g_ScreenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // FrameBuffer
  GLuint frameBufferID;
  glGenFramebuffers(1, &frameBufferID);
  glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_ScreenTextureID, 0);
  if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  {
    std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
    glfwTerminate();
    return 1;
  }

  glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);
  glDisable(GL_DEPTH_TEST);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  std::deque<float> lastDeltas;

  double oldCpuTime = glfwGetTime();
  while (!glfwWindowShouldClose(_MainWindow))
  {
    g_Frame++;

    g_CurLoopTime = glfwGetTime();

    g_TimeDelta = g_CurLoopTime - oldCpuTime;
    oldCpuTime = g_CurLoopTime;

    lastDeltas.push_back(g_TimeDelta);
    while ( lastDeltas.size() > 60 )
      lastDeltas.pop_front();
    
    double totalDelta = 0.;
    for ( auto delta : lastDeltas )
      totalDelta += delta;
    double averageDelta = totalDelta / lastDeltas.size();

    if ( averageDelta > 0. )
      g_FrameRate = 1. / (float)averageDelta;

    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::Begin("Shader selection");

      if (ImGui::Button("Next Fragment Shader >"))
      {
        g_FragShaderNum++;
        if ( g_FragShaderNum > 7 )
          g_FragShaderNum = 0;
        RecompileShaders();
        if ( !g_RTTShader )
          break;
        shaderProgramID = g_RTTShader -> GetShaderProgramID();

        lastDeltas.clear();
        lastDeltas.push_back(g_TimeDelta);
      }
      ImGui::SameLine();
      ImGui::Text("Shader num = %d", g_FragShaderNum);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::Text("RenderToTexture average %.3f ms/frame (%.1f FPS)", averageDelta * 1000.f, g_FrameRate);

      ImGui::End();
    }

    // Rendering
    ImGui::Render();

    glfwGetFramebufferSize(_MainWindow, &g_ScreenWidth, &g_ScreenHeight);
    glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    UpdateUniforms();

    // Render to frame buffer
    glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
    quad -> Render(*g_RTTShader);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    quad -> Render(*g_OutputShader);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(_MainWindow);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glDeleteFramebuffers(1, &frameBufferID);
  glDeleteTextures(1, &g_ScreenTextureID);

  if ( quad )
    delete quad;
  quad = NULL;
  if ( g_RTTShader )
    delete g_RTTShader;
  g_RTTShader = NULL;
  if ( g_OutputShader )
    delete g_OutputShader;
  g_OutputShader = NULL;

  return ret;
}

}
