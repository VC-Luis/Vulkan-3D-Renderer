#include "../include/window.hpp"

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define BOLD    "\033[1m"
#define UNDERLINE "\033[4m"
#define BRIGHT_RED "\033[91m"
#define BRIGHT_YELLOW "\33[93m"
#define BRIGHT_WHITE "\033[97m"

Window::Window(uint32_t windowWidth, uint32_t windowHeight, std::string name)
{
    //Initialise the GLFW library and set the hints such that no OpenGL context is created
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLWindow = glfwCreateWindow(windowWidth, windowHeight, name.c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(GLWindow, this);
    
    if(GLWindow == nullptr || GLWindow == NULL)
    {
        std::cerr << RED << "ERROR: The window could not be initialised properly" << RESET << std::endl;
    }
    else
    {
        std::cout << GREEN << "GLFW window was successfully created!" << RESET << std::endl;
    }

    width = windowWidth;
    height = windowHeight;
    windowName = name;
}

Window::Window()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    width = 1;
    height = 1;
    windowName = "Undefined";
    GLWindow = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    
    if(GLWindow == nullptr || GLWindow == NULL)
    {
        std::cerr << RED << "ERROR: The window could not be initialised properly" << RESET << std::endl;
    }
    else
    {
        std::cout << GREEN << "GLFW window was successfully created!" << RESET << std::endl;
    }
}

Window::~Window()
{
    std::cout << BRIGHT_WHITE << "Destrying window" << std::endl;
    glfwDestroyWindow(GLWindow);
    glfwTerminate();
}

bool Window::windowShouldClose()
{
    glfwPollEvents();
    return glfwWindowShouldClose(GLWindow);
}