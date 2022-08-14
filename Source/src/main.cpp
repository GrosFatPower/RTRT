#include <iostream>

#include <GLFW/glfw3.h>

int main() {
  std::cout << "Hello, world!\n";

  GLFWwindow* window;
  int width, height;

  if (!glfwInit())
  {
    fprintf(stderr, "Failed to initialize GLFW\n");
    exit(EXIT_FAILURE);
  }

  window = glfwCreateWindow(300, 300, "Hello world GLFW", NULL, NULL);
  if (!window)
  {
    fprintf(stderr, "Failed to open GLFW window\n");
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  // Main loop
  while (!glfwWindowShouldClose(window))
  {
    // Swap buffers
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  // Terminate GLFW
  glfwTerminate();

  // Exit program
  exit(EXIT_SUCCESS);
}
