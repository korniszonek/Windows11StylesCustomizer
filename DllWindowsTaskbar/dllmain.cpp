#include "pch.h"
#include <dwmapi.h>
#include <cstdio>

#pragma comment(lib, "dwmapi.lib")
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WCHAR className[256];
    GetClassName(hwnd, className, 256);

    if (wcsstr(className, L"Tray") || wcsstr(className, L"Taskbar")) {
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        SetLayeredWindowAttributes(hwnd, 0, 200, LWA_ALPHA);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
    return TRUE;
}
void MainThread() {
    Sleep(3000);

    EnumWindows(EnumWindowsProc, 0);
}
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        CreateThread(0, 0, (LPTHREAD_START_ROUTINE)MainThread, 0, 0, 0);
    }
    return TRUE;
}

