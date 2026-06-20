#include "pch.h"
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

extern "C" __declspec(dllexport) void __stdcall ExecuteLogic(const char* command, HWND hwnd) {
    // at the moment runing test command to se if on go injection work
    if (!hwnd) {
        return;
    }
    RECT rect;
    GetWindowRect(hwnd, &rect);
    int width = rect.right - rect.left;
    /*
    we need to oparate on right - left instead of only right like in normal graph libary
        - in case of user having 2 monitors or more starting
        for example 2x 1920x1080 points are both starting - 0 and 1920 ending -  1920 and 3840 - using only right would cause incorrect rendering
    */
    int height = rect.bottom - rect.top;

    HRGN hRgn = CreateRoundRectRgn(0, 0, width, height, 40, 40); // 40,40 - radius
    SetWindowRgn(hwnd, hRgn, TRUE);
    DeleteObject(hRgn);
}