#pragma once
#include <windows.h>

class IRenderer {
public:
    virtual ~IRenderer() {}
    virtual void OnPaint(HDC hdc, int width, int height) = 0;
    virtual void OnCommand(const char* cmd) = 0;
};