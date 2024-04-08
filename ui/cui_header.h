#pragma once

#include <vector>
#include <windows.h>
#include <iostream>
#include <conio.h>
#include <algorithm>

#include "../netlib.h"

namespace cui {
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);


    void goToXY(short x, short y)
    {
        SetConsoleCursorPosition(hStdOut, {x, y});
    }

    void consoleCursorVisible(bool show, short size)
    {
        CONSOLE_CURSOR_INFO structCursorInfo;
        GetConsoleCursorInfo(hStdOut, &structCursorInfo);
        structCursorInfo.bVisible = show;
        structCursorInfo.dwSize = size;
        SetConsoleCursorInfo(hStdOut, &structCursorInfo);
    }

    void printText(std::string text, int flags = FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN) {
        SetConsoleTextAttribute(hStdOut, flags);
        std::cout << text;
    }

    const char UP = 72;
    const char DOWN = 80;
    const char LEFT = 75;
    const char RIGHT = 77;
    const char ENTER = 13;

    void beep() {
        Beep(523,50);
    }
}
