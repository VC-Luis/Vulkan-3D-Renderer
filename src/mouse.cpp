#include "mouse.hpp"

#include <GLFW/glfw3.h>

mousePosition Mouse::getPosition(Window& window)
{
    mousePosition pos;
    glfwGetCursorPos(window.getHandle(), &pos.xPos, &pos.yPos);

    return pos;
}

void Mouse::setMode(Window& window, MouseMode mode)
{
    glfwSetInputMode(window.getHandle(), GLFW_CURSOR, (int)mode);
}