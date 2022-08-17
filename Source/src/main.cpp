#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <vector>
#include <string>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>

#define RTRT_DISPLAY_GUI
#define RTRT_RENDER_TO_TEXTURE

int g_ScreenWidth  = 1920;
int g_ScreenHeight = 1080;

#ifdef RTRT_RENDER_TO_TEXTURE
GLuint g_ShaderProgramID;

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

static void glfw_error_callback(int error, const char* description)
{
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

#ifdef RTRT_DISPLAY_GUI
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  if (action == GLFW_PRESS)
    std::cout << "EVENT : KEY PRESSED" << std::endl;

  ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

void mousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( ( ( button == GLFW_MOUSE_BUTTON_1 ) || ( button == GLFW_MOUSE_BUTTON_2 ) ) && ( action == GLFW_PRESS ) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
  {
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    std::cout << "EVENT : MOUSE CLICK (" << mouseX << "," << mouseY << ")" << std::endl;
  }

  ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  std::cout << "EVENT : FRAME BUFFER RESIZED" << std::endl;
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

  g_ShaderProgramID = CreateShaderProgram("..\\..\\shaders\\vertex.glsl", "..\\..\\shaders\\fragment.glsl");
  if ( !g_ShaderProgramID )
    return 0;

  glUseProgram(g_ShaderProgramID);

  return 1;
}
#endif

int main(int, char**)
{
  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
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
  if (!mainWindow)
  {
    std::cout << "Failed to create a window!" << std::endl;
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  //glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
  //glfwSetMouseButtonCallback(mainWindow, mousebuttonCallback);
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
  //glBindBuffer(GL_ARRAY_BUFFER, 0);

  GLuint uvBufferID;
  glGenBuffers(1, &uvBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
  glBufferData(GL_ARRAY_BUFFER, sizeof(g_QuadUVs), g_QuadUVs, GL_STATIC_DRAW);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(1);
  //glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(vertexArrayID);

  GLuint screenTextureID;
  glGenTextures(1, &screenTextureID);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, screenTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, g_ScreenWidth, g_ScreenHeight, 0, GL_RGBA, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  //GLuint frameBufferID;
  //glGenFramebuffers(1, &frameBufferID);
  //glBindFramebuffer(GL_FRAMEBUFFER, frameBufferID);
  //glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTextureID, 0);
  //if ( glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE )
  //{
  //  std::cout << "ERROR: Framebuffer is not complete!" << std::endl;
  //  glfwTerminate();
  //  exit(EXIT_FAILURE);
  //}

  //glUniform1i(glGetUniformLocation(shaderProgram, "u_screenTexture"), 0);
#endif

  glViewport(0, 0, g_ScreenWidth, g_ScreenHeight);
  glDisable(GL_DEPTH_TEST);


  // Our state
  bool show_demo_window = true;
  bool show_another_window = true;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  while (!glfwWindowShouldClose(mainWindow))
  {
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

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
      ImGui::ShowDemoWindow(&show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

      ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
      ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
      ImGui::Checkbox("Another Window", &show_another_window);

      ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
      ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me"))
        show_another_window = false;
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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
  //glDeleteFramebuffers(1, &frameBufferID);
#endif

  glfwDestroyWindow(mainWindow);
  glfwTerminate();

  // Exit program
  exit(EXIT_SUCCESS);
}
