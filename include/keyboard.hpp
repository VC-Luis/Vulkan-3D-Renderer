#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "window.hpp"

enum Key
{
    Space = 32,

    Apostrophe = 39,

    Comma = 44,
    Dash = 45,
    Period = 46,
    Slash = 47,
    Zero = 48,
    One = 49,
    Two = 50,
    Three = 51,
    Four = 52,
    Five = 53,
    Six = 54,
    Seven = 55,
    Eight = 56,
    Nine = 57,


    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,

    LeftShift = 340,
    LeftCtrl = 341,
    LeftAlt = 342,
    
    RightShift = 344,
    RightCtrl = 345,
    RightAlt = 346
};

enum KeyStatus
{
    Release,
    Press,
    Hold
};

KeyStatus getKeyStatus(Key key, Window& targetWindow);

#endif