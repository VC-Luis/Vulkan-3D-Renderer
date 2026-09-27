#include "keyboard.hpp"

#include <GLFW/glfw3.h>

KeyStatus getKeyStatus(Key key, Window& targetWindow)
{
    return (KeyStatus)glfwGetKey(targetWindow.getHandle(), key);
}