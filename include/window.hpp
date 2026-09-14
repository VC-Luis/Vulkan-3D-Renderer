#include <stdint.h>
#include <string>
#include <iostream>
#include <GLFW/glfw3.h>

#ifndef WINDOW_H
#define WINDOW_H

class Window
{
private:
    uint32_t width;
    uint32_t height;
    std::string windowName;

    GLFWwindow* GLWindow;

public:
    std::string getWindowName();
    std::pair<uint32_t, uint32_t> getSize();
    GLFWwindow* getHandle();

    Window(uint32_t width, uint32_t height, std::string name);
    Window();
    ~Window();

    bool windowShouldClose();
};

#endif