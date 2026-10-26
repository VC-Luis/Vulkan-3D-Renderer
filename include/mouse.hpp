#ifndef MOUSE_H
#define MOUSE_H

#include "window.hpp"

struct mousePosition
{
    double xPos;
    double yPos;
};

struct mouseVelocity
{
    double xVelocity;
    double yVelocity;
};

enum MouseButton
{
    Left = 0,
    Right = 1,
    Middle = 2
};

enum MouseMode
{
    Normal = 0x00034001, //
    Hidden = 0x00034002,
    Disabled = 0x00034003,
    Captured = 0x00034004
};

class Mouse
{
private:
    mousePosition position;

    mouseVelocity velocity;
public:
    static mousePosition getPosition(Window& window);
    static mouseVelocity getVelocity(Window& window); //TODO
    static void setMode(Window& window, MouseMode mode);
};

#endif