/*
*  ProcessCommand - Processing command from pipe (atm only checks if piping works)
*  PipeServer - connecting pipe to dll  
*  FindTaskbar - looking for a taskbar HWND item inside an explorer - when found -> assign it to hTaskbar 
*  ApplyTestEffect - check to see how real changes work - a final exam to verify FindTaskbar method 
*/
#include "pch.h"
#include <dwmapi.h>
#include <cstdio>
#include <thread>
#include <sddl.h>

#pragma comment(lib, "dwmapi.lib")
#define PIPE_NAME L"\\\\.\\pipe\\WSM"


SECURITY_ATTRIBUTES sa;
bool saInitialized = false;

HWND hTaskbar = NULL;

BOOL CALLBACK FindTaskbar(HWND hwnd, LPARAM lParam) {
    WCHAR className[256];
    GetClassName(hwnd, className, 256); 
    /* 
    according to MSDN maximal length of classNames inside windows architeture is 256 in most cases(*2 caused by WCHAR size = 512 bytes in memory)
        we want to use as little amounts of memory as possible to keep system unaffected
    */
    if (wcscmp(className, L"Shell_TrayWnd") == 0) {
        hTaskbar = hwnd;
        return FALSE;
    }
    return TRUE;
}

void ApplyTestEffect(HWND hwnd) {
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
    SetWindowRgn(hTaskbar, hRgn, TRUE);
    DeleteObject(hRgn);
}

void ProcessCommand(const char* command) {
    //MessageBoxA(NULL, "PipeServer started inside explorer.exe!", "WinShark Status", MB_OK); -> check
    if (hTaskbar == NULL) {
        EnumWindows(FindTaskbar, 0); //stops when return false, otherwise - keep looking
    } 
    ApplyTestEffect(hTaskbar);
}

void PipeServer() {
    
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = FALSE;
    if (ConvertStringSecurityDescriptorToSecurityDescriptor(
        L"D:P(A;;GA;;;WD)", SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) {
        saInitialized = true;
    }
    while (true) {
        HANDLE hPipe = CreateNamedPipe(
            PIPE_NAME,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,
            1024, 1024, 0, 
            saInitialized ? &sa : NULL
        );
        if (ConnectNamedPipe(hPipe, NULL)) {
            char buffer[1024] = { 0 };
            DWORD bytesRead;
            if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
                ProcessCommand(buffer);
            }
        }
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        std::thread(PipeServer).detach(); // pipe working in background, to prevent freezing the process start-up
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (saInitialized && sa.lpSecurityDescriptor) {
            LocalFree(sa.lpSecurityDescriptor);
        }
    }
    return TRUE;
}

