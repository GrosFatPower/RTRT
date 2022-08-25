#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <vector>
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>

#define RTRT_DISPLAY_GUI
#define RTRT_RENDER_TO_TEXTURE

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
int g_ScreenWidth  = 1920;
int g_ScreenHeight = 1080;

double g_MouseX     = 0.;
double g_MouseY     = 0.;
bool   g_LeftClick  = false;
bool   g_RightClick = false;

long  g_Frame     = 0;
float g_FrameRate = 60.;

short g_VtxShaderNum  = 0;
short g_FragShaderNum = 0;

#ifdef RTRT_RENDER_TO_TEXTURE
GLuint g_ShaderProgramID   = 0;
GLuint g_ScreenTextureID   = 0;
GLuint g_u_DirectOutPassID = 0;
GLuint g_u_ResolutionID    = 0;
GLuint g_u_MouseID         = 0;
GLuint g_u_TimeID          = 0;
GLuint g_u_TimeDeltaID     = 0;
GLuint g_u_FrameID         = 0;
GLuint g_u_FrameRateID     = 0;

const GLfloat g_QuadVtx[] =
{
  -1.0f, -1.0f,  0.0f,
   1.0f, -1.0f,  0.0f,
   1.0f,  1.0f,  0.0f,
   1.0f,  1.0f,  0.0f,
  -1.0f,  1.0f,  0.0f,
  -1.0f, -1.0f,  0.0f
};

const GLfloat g_QuadUVs[] =
{
  0.0f, 0.0f,
  1.0f, 0.0f,
  1.0f, 1.0f,
  1.0f, 1.0f,
  0.0f, 1.0f,
  0.0f, 0.0f,
};
#endif

// ----------------------------------------------------------------------------
// Global functions
// ----------------------------------------------------------------------------
static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

#ifdef RTRT_DISPLAY_GUI
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (action == GLFW_PRESS)
    std::cout << "EVENT : KEY PRESSED" << std::endl;
}

void MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
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

void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  std::cout << "EVENT : FRAME BUFFER RESIZED" << std::endl;

  g_ScreenWidth  = width;
  g_ScreenHeight = height;

  glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);

  glBindTexture(GL_TEXTURE_2D, g_ScreenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, g_ScreenWidth, g_ScreenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
}
#endif

#ifdef RTRT_RENDER_TO_TEXTURE
GLuint CreateShaderProgram(const char * vertexShaderPath, const char * fragmentShaderPath)
{
  // Create the shaders
  GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

  // Read the Vertex Shader code from the file
  std::string vertexShaderCode;
  std::ifstream vertexShaderStream(vertexShaderPath, std::ios::in);
  if ( vertexShaderStream.is_open() )
  {
    std::stringstream sstr;
    sstr << vertexShaderStream.rdbuf();
    vertexShaderCode = sstr.str();
    vertexShaderStream.close();
  }
  else
  {
    printf("Unable to open %s.\n", vertexShaderPath);
    return 0;
  }

  // Read the Fragment Shader code from the file
  std::string fragmentShaderCode;
  std::ifstream fragmentShaderStream(fragmentShaderPath, std::ios::in);
  if ( fragmentShaderStream.is_open() ) {
    std::stringstream sstr;
    sstr << fragmentShaderStream.rdbuf();
    fragmentShaderCode = sstr.str();
    fragmentShaderStream.close();
  }
  else
  {
    printf("Unable to open %s.\n", fragmentShaderPath);
    return 0;
  }

  GLint glResult = GL_FALSE;
  int glInfoLogLength = 0;

  // Compile Vertex Shader
  printf("Compiling shader : %s\n", vertexShaderPath);
  char const * vertexSourcePointer = vertexShaderCode.c_str();
  glShaderSource(vertexShaderID, 1, &vertexSourcePointer, NULL);
  glCompileShader(vertexShaderID);

  // Check Vertex Shader
  glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &glResult);
  glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, &glInfoLogLength);
  if ( glInfoLogLength > 0 )
  {
    std::vector<char> vertexShaderErrorMessage(glInfoLogLength + 1);
    glGetShaderInfoLog(vertexShaderID, glInfoLogLength, NULL, &vertexShaderErrorMessage[0]);
    printf("%s\n", &vertexShaderErrorMessage[0]);
  }

  // Compile Fragment Shader
  printf("Compiling shader : %s\n", fragmentShaderPath);
  char const* FragmentSourcePointer = fragmentShaderCode.c_str();
  glShaderSource(fragmentShaderID, 1, &FragmentSourcePointer, NULL);
  glCompileShader(fragmentShaderID);

  // Check Fragment Shader
  glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &glResult);
  glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, &glInfoLogLength);
  if ( glInfoLogLength > 0 )
  {
    std::vector<char> fragmentShaderErrorMessage(glInfoLogLength + 1);
    glGetShaderInfoLog(fragmentShaderID, glInfoLogLength, NULL, &fragmentShaderErrorMessage[0]);
    printf("%s\n", &fragmentShaderErrorMessage[0]);
  }

  // Link the program
  printf("Linking program\n");
  GLuint programID = glCreateProgram();
  glAttachShader(programID, vertexShaderID);
  glAttachShader(programID, fragmentShaderID);
  glLinkProgram(programID);

  // Check the program
  glGetProgramiv(programID, GL_LINK_STATUS, &glResult);
  glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &glInfoLogLength);
  if ( glInfoLogLength > 0)
  {
    std::vector<char> programErrorMessage(glInfoLogLength + 1);
    glGetProgramInfoLog(programID, glInfoLogLength, NULL, &programErrorMessage[0]);
    printf("%s\n", &programErrorMessage[0]);
  }

  glDetachShader(programID, vertexShaderID);
  glDetachShader(programID, fragmentShaderID);

  glDeleteShader(vertexShaderID);
  glDeleteShader(fragmentShaderID);

  return programID;
}

int RecompileShaders()
{
  if ( g_ShaderProgramID )
    glDeleteProgram(g_ShaderProgramID);

  const char * vertexShaderFileName = NULL;
  switch ( g_VtxShaderNum )
  {
  case 0 :
  default :
    vertexShaderFileName = "..\\..\\shaders\\vertex_Default.glsl";
  }

  const char * fragmentShaderFileName = NULL;
  switch ( g_FragShaderNum )
  {
  case 1 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_Drawtexture.glsl";
    break;
  case 2 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_RayMarching.glsl";
    break;
  case 3 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_ScreenSaver.glsl";
    break;
  case 4 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_Mandelbrot.glsl";
    break;
  case 5 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_Whitenoise.glsl";
    break;
  case 6 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_Octagrams.glsl";
    break;
  case 7 :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_FantasyEscape.glsl";
    break;
  default :
    fragmentShaderFileName = "..\\..\\shaders\\fragment_Default.glsl";
  }

  g_ShaderProgramID = CreateShaderProgram(vertexShaderFileName, fragmentShaderFileName);
  if ( !g_ShaderProgramID )
    return 0;

  glUseProgram(g_ShaderProgramID);

  g_u_DirectOutPassID = glGetUniformLocation(g_ShaderProgramID, "u_DirectOutputPass");
  g_u_ResolutionID    = glGetUniformLocation(g_ShaderProgramID, "u_Resolution");
  g_u_MouseID         = glGetUniformLocation(g_ShaderProgramID, "u_Mouse");
  g_u_TimeID          = glGetUniformLocation(g_ShaderProgramID, "u_Time");
  g_u_TimeDeltaID     = glGetUniformLocation(g_ShaderProgramID, "u_TimeDelta");
  g_u_FrameID         = glGetUniformLocation(g_ShaderProgramID, "u_Frame");
  g_u_FrameRateID     = glGetUniformLocation(g_ShaderProgramID, "u_FrameRate");
  glUniform1i(glGetUniformLocation(g_ShaderProgramID, "u_ScreenTexture"), 0);
  glUniform1i(glGetUniformLocation(g_ShaderProgramID, "u_Texture"), 1);

  return 1;
}
#endif

// ----------------------------------------------------------------------------
// main
// ----------------------------------------------------------------------------
int main(int, char**)
{
  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if ( !glfwInit() )
  {
    std::cout << "Failed to initialize GLFW!" << std::endl;
    exit(EXIT_FAILURE);
  }

  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
  //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

  // Create window with graphics context
  GLFWwindow * mainWindow = glfwCreateWindow(g_ScreenWidth, g_ScreenHeight, "RTRT Hello world", NULL, NULL);
  if ( !mainWindow )
  {
    std::cout << "Failed to create a window!" << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetFramebufferSizeCallback(mainWindow, FramebufferSizeCallback);
  glfwSetMouseButtonCallback(mainWindow, MousebuttonCallback);
  //glfwSetKeyCallback(mainWindow, keyCallback);

  glfwMakeContextCurrent(mainWindow);
  glfwSwapInterval(1); // Enable vsync

#ifdef RTRT_DISPLAY_GUI
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
  ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
#endif

  // Init openGL scene

#ifdef RTRT_RENDER_TO_TEXTURE
  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  if ( !RecompileShaders() )
  {
    std::cout << "Shader compilation failed!" << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  int textWidth = 0, textHeight = 0, textNbChan = 0;
  float * textureData = stbi_loadf("..\\..\\Resources\\Img\\nature.png", &textWidth, &textHeight, &textNbChan, 0);
  if ( !textureData || ( ( textNbChan != 3 ) && ( textNbChan != 4 ) ) )
  {
    std::cout << "unable to load image!" << std::endl;
    glfwTerminate();
    if ( textureData )
      stbi_image_free(textureData);
    exit(EXIT_FAILURE);
  }

  // Quad
  GLuint vertexArrayID;
  glGenVertexArrays(1, &vertexArrayID);
  glBindVertexArray(vertexArrayID);

  GLuint vertexBufferID;
  glGenBuffers(1, &vertexBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_QuadVtx), g_QuadVtx, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(0);

  GLuint uvBufferID;
  glGenBuffers(1, &uvBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_QuadUVs), g_QuadUVs, GL_STATIC_DRAW);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(1);

  glBindVertexArray(vertexArrayID);

  // Texture
  GLuint textureID;
  glGenTextures(1, &textureID);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textureID);
  if ( 4 == textNbChan )
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textWidth, textHeight, 0, GL_RGBA, GL_FLOAT, textureData);
  else
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, textWidth, textHeight, 0, GL_RGB, GL_FLOAT, textureData);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_image_free(textureData);

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
    exit(EXIT_FAILURE);
  }

  glUniform1i(glGetUniformLocation(g_ShaderProgramID, "u_ScreenTexture"), 0);
  glUniform1i(glGetUniformLocation(g_ShaderProgramID, "u_Texture"), 1);
#endif

  glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);
  glDisable(GL_DEPTH_TEST);


  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  double averageDelta = 0.;
  double LastDeltas[30] = { 0. };
  int nbStoredDeltas = 0;

  double oldCpuTime = glfwGetTime();
  while (!glfwWindowShouldClose(mainWindow))
  {
    g_Frame++;

    double curLoopTime = glfwGetTime();

    double timeDelta = curLoopTime - oldCpuTime;
    oldCpuTime = curLoopTime;

    if ( 30 == nbStoredDeltas )
    {
      double totalDelta = 0.;
      for ( int i = 0; i < 30; ++i )
        totalDelta += LastDeltas[i];
      averageDelta = totalDelta / 30;
      nbStoredDeltas = 0;
    }
    else if ( nbStoredDeltas < 30 )
      LastDeltas[nbStoredDeltas++] = timeDelta;
    else
      nbStoredDeltas = 0;

    if ( averageDelta > 0. )
      g_FrameRate = 1. / averageDelta;
    else if ( timeDelta > 0. )
      g_FrameRate = 1. / timeDelta;

    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
    // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
    glfwPollEvents();

#ifdef RTRT_DISPLAY_GUI
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
#ifdef RTRT_DISPLAY_GUI
        RecompileShaders();
#endif
      }
      ImGui::SameLine();
      ImGui::Text("Shader num = %d", g_FragShaderNum);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::Text("RenderToTexture average %.3f ms/frame (%.1f FPS)", averageDelta * 1000.f, g_FrameRate);

      ImGui::End();
    }
#endif

    // Rendering
#ifdef RTRT_DISPLAY_GUI
    ImGui::Render();
#endif

    glfwGetFramebufferSize(mainWindow, &g_ScreenWidth, &g_ScreenHeight);
    glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

#ifdef RTRT_RENDER_TO_TEXTURE
    // Render to frame buffer
   glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
   glUniform1i(g_u_DirectOutPassID, 0);
   glUniform3f(g_u_ResolutionID, g_ScreenWidth, g_ScreenHeight, 0.f);
   glUniform4f(g_u_MouseID, g_MouseX, g_MouseY, (float) g_LeftClick, (float) g_RightClick);
   glUniform1f(g_u_TimeID, (float)curLoopTime);
   glUniform1f(g_u_TimeDeltaID, (float)timeDelta);
   glUniform1i(g_u_FrameID, (int)g_Frame);
   glUniform1f(g_u_FrameRateID, g_FrameRate);
   glDrawArrays(GL_TRIANGLES, 0, 6);

   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   glUniform1i(g_u_DirectOutPassID, 1);
   glDrawArrays(GL_TRIANGLES, 0, 6);
#endif

#ifdef RTRT_DISPLAY_GUI
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif

    glfwSwapBuffers(mainWindow);
  }

  // Cleanup
#ifdef RTRT_DISPLAY_GUI
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
#endif

#ifdef RTRT_RENDER_TO_TEXTURE
  glDeleteBuffers(1, &vertexBufferID);
  glDeleteBuffers(1, &uvBufferID);
  glDeleteVertexArrays(1, &vertexArrayID);
  glDeleteProgram(g_ShaderProgramID);
  glDeleteFramebuffers(1, &frameBufferID);
  glDeleteTextures(1, &g_ScreenTextureID);
#endif

  glfwDestroyWindow(mainWindow);
  glfwTerminate();

  // Exit program
  exit(EXIT_SUCCESS);
}
