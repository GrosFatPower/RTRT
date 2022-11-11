#include "Test2.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "tinydir.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "Scene.h"
#include "RenderSettings.h"
#include "Loader.h"

namespace RTRT
{

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static double g_MouseX       = 0.;
static double g_MouseY       = 0.;
static bool   g_LeftClick    = false;
static bool   g_RightClick   = false;

static long  g_Frame         = 0;
static float g_FrameRate     = 60.;

static std::string g_AssetsDir = "..\\..\\Assets\\";
static int         g_CurSceneIndex = 0;
static int         g_LoadingState = 0;

// ----------------------------------------------------------------------------
// Global functions
// ----------------------------------------------------------------------------
static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

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

  glViewport(0, 0, width, height);
}

static int LoadScene( const std::string & iFilename, Scene *& ioScene, RenderSettings & oSettings )
{
  if ( ioScene )
    delete ioScene;
  ioScene = nullptr;

  if ( !Loader::LoadScene(g_AssetsDir + iFilename, ioScene, oSettings) || !ioScene )
    return 1;

  return 0;
}

Test2::Test2( int iScreenWidth, int iScreenHeight )
{
  _ScreenWitdh  = iScreenWidth;
  _ScreenHeight = iScreenHeight;

  InitializeSceneFile();
}

Test2::~Test2()
{
  if ( _Scene )
    delete _Scene;
  _Scene = nullptr;
}

void Test2::InitializeSceneFile()
{
  tinydir_dir dir;
  tinydir_open_sorted(&dir, g_AssetsDir.c_str());

  for ( int i = 0; i < dir.n_files; ++i )
  {
    tinydir_file file;
    tinydir_readfile_n(&dir, &file, i);

    std::string extension(file.extension);
    if ( "scene" == extension )
      _SceneFiles.push_back(std::string(file.name));
  }

  tinydir_close(&dir);
}

int Test2::Run()
{
  int ret = 0;

  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if ( !glfwInit() )
  {
    std::cout << "Failed to initialize GLFW!" << std::endl;
    return 1;
  }

  const char* glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  // Create window with graphics context
  GLFWwindow * mainWindow = glfwCreateWindow(_ScreenWitdh, _ScreenHeight, "RTRT Hello world", NULL, NULL);
  if ( !mainWindow )
  {
    std::cout << "Failed to create a window!" << std::endl;
    glfwTerminate();
    return 1;
  }

  glfwSetFramebufferSizeCallback(mainWindow, FramebufferSizeCallback);
  glfwSetMouseButtonCallback(mainWindow, MousebuttonCallback);
  //glfwSetKeyCallback(mainWindow, keyCallback);

  glfwMakeContextCurrent(mainWindow);
  glfwSwapInterval(1); // Enable vsync

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
  ImGui_ImplGlfw_InitForOpenGL(mainWindow, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Init openGL scene

  glewExperimental = GL_TRUE;
  if ( glewInit() != GLEW_OK )
  {
    std::cout << "Failed to initialize GLEW!" << std::endl;
    glfwTerminate();
    return 1;
  }

  glViewport(0, 0, _ScreenWitdh, _ScreenHeight);
  glDisable(GL_DEPTH_TEST);

  // Our state
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  bool forceResize = false;
  double oldCpuTime = glfwGetTime();
  while (!glfwWindowShouldClose(mainWindow))
  {
    g_Frame++;

    double curLoopTime = glfwGetTime();

    double timeDelta = curLoopTime - oldCpuTime;
    oldCpuTime = curLoopTime;

    if ( forceResize )
    {
      _ScreenWitdh  = _Settings._WindowResolution.x;
      _ScreenHeight = _Settings._WindowResolution.y;
      glfwSetWindowSize(mainWindow, _ScreenWitdh, _ScreenHeight);
      forceResize = false;
    }

    glfwPollEvents();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
      ImGui::Begin("Scene loader");

      std::vector<const char*> sceneNames;
      for (int i = 0; i < _SceneFiles.size(); ++i)
          sceneNames.push_back(_SceneFiles[i].c_str());

      if ( ImGui::Combo("Scene selection", &g_CurSceneIndex, sceneNames.data(), sceneNames.size()) )
      {
        int failed = LoadScene(_SceneFiles[g_CurSceneIndex], _Scene, _Settings);
        if ( !failed )
        {
          g_LoadingState = 1;
          forceResize = true;
        }
        else
        {
          g_LoadingState = 0;
          std::cout << "Failed to load scene " << _SceneFiles[g_CurSceneIndex] << std::endl;
        }
      }

      if ( g_LoadingState )
      {
        ImGui::BulletText("Settings");
        ImGui::Text("Window resolution : w=%d h=%d", _Settings._WindowResolution.x, _Settings._WindowResolution.y);
        ImGui::Text("Render resolution : w=%d h=%d", _Settings._RenderResolution.x, _Settings._RenderResolution.y);

        ImGui::BulletText("Scene : %s", _SceneFiles[g_CurSceneIndex].c_str());
        ImGui::Text("Nb lights    : %d",    _Scene -> GetNbLights());
        ImGui::Text("Nb materials : %d", _Scene -> GetNbMaterials());
        ImGui::Text("Nb textures  : %d",  _Scene -> GetNbTextures());
        ImGui::Text("Nb meshes    : %d",    _Scene -> GetNbMeshes());
      }
      else
      {
        ImGui::Text("No loaded scene");
      }

      ImGui::End();
    }

    // Rendering
    ImGui::Render();

    glfwGetFramebufferSize(mainWindow, &_ScreenWitdh, &_ScreenHeight);
    glViewport(0, 0, _ScreenWitdh, _ScreenHeight);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(mainWindow);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(mainWindow);
  glfwTerminate();

  return ret;
}

  // Load scene
  //if ( ( 0 != LoadScene("..\\..\\Assets\\diningroom.scene", _Scene, _Settings) ) || !_Scene )
  //{
  //  return 1;
  //}

}
