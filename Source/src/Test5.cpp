#include "Test5.h"

#include <string>
#include <iostream>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace RTRT
{

const char * Test5::GetTestHeader() { return "Test 5 : Renderers"; }

// ----------------------------------------------------------------------------
// Global variables
// ----------------------------------------------------------------------------
static std::string g_AssetsDir = "..\\..\\Assets\\";

// ----------------------------------------------------------------------------
// KeyCallback
// ----------------------------------------------------------------------------
void Test5::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  if ( action == GLFW_PRESS )
  {
    std::cout << "EVENT : KEY PRESS" << std::endl;

    switch ( key )
    {
    default :
      break;
    }
  }

  else if ( action == GLFW_RELEASE )
  {
    std::cout << "EVENT : KEY RELEASE" << std::endl;

    switch ( key )
    {
    default :
      break;
    }
  }
}

// ----------------------------------------------------------------------------
// MousebuttonCallback
// ----------------------------------------------------------------------------
void Test5::MousebuttonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if ( !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) )
  {
    auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
    if ( !this_ )
      return;

    double mouseX = 0.f, mouseY = 0.f;
    glfwGetCursorPos(window, &mouseX, &mouseY);
  }
}

// ----------------------------------------------------------------------------
// FramebufferSizeCallback
// ----------------------------------------------------------------------------
void Test5::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
  auto * const this_ = static_cast<Test5*>(glfwGetWindowUserPointer(window));
  if ( !this_ )
    return;

  std::cout << "EVENT : FRAME BUFFER RESIZED. WIDTH = " << width << " HEIGHT = " << height << std::endl;

  if ( !width || !height )
    return;
}

// ----------------------------------------------------------------------------
// CTOR
// ----------------------------------------------------------------------------
Test5::Test5( std::shared_ptr<GLFWwindow> iMainWindow, int iScreenWidth, int iScreenHeight )
: BaseTest(iMainWindow, iScreenWidth, iScreenHeight)
{
}

// ----------------------------------------------------------------------------
// DTOR
// ----------------------------------------------------------------------------
Test5::~Test5()
{
}

// ----------------------------------------------------------------------------
// Run
// ----------------------------------------------------------------------------
int Test5::Run()
{
  int ret = 0;

  return ret;
}

}
