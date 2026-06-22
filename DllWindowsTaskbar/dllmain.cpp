/* 
    ProcessCommand - Processing command from pipe (atm only checks if piping works)
    PipeServer - connecting pipe to dll
    FindTaskbar - looking for a taskbar HWND item inside an explorer - when found -> assign it to hTaskbar
    LoadLogic - loading logic of nonSystem used dll
*/
#include "pch.h"
#include <dwmapi.h>
#include <cstdio>
#include <thread>
#include <sddl.h>
#include <mutex>
#include <string>

#pragma comment(lib, "dwmapi.lib")

#define PIPE_NAME L"\\\\.\\pipe\\WSM"

using namespace std;

HMODULE hLogicModule = NULL;
mutex logicMutex;
SECURITY_ATTRIBUTES sa;
bool saInitialized = false;
HWND hTaskbar = NULL;
HWND hIsland = NULL;

struct IRenderer {
    virtual void OnPaint(HDC hdc, int width, int height) = 0;
    virtual void OnCommand(const char* cmd) = 0;
};
IRenderer* currentRenderer = nullptr;

wstring GetLogicPath() {
    WCHAR path[MAX_PATH];
    HMODULE hm = NULL;

    if (GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&GetLogicPath,
        &hm))
    {
        GetModuleFileName(hm, path, MAX_PATH);
    }

    wstring strPath = path;
    size_t pos = strPath.find_last_of(L"\\/");
    return strPath.substr(0, pos + 1) + L"taskbarLogic.dll";
}



/* Spliting logic and process of injecting changes
    allows to avoid a need to recompiling system used dll(master dll) on every code change(which means a need to kill / reload the explorer process every time) - we
    keep our dll orchestrator alive and recompile logic.file which is only used by master dll, not a system process
*/
void LoadLogic() {
    lock_guard<mutex> lock(logicMutex);
    /* we are using guard to avoid race condition -
        we use pipe server to reload logic wcich is an independent service.
        if master dll is curently Freeing libary and pipe server is loading new logic it will
        prevent a crash caused by a modifing a delated memory
    */
    if (hLogicModule) {
        FreeLibrary(hLogicModule);
        hLogicModule = NULL;
    }
    wstring originalPath = GetLogicPath();
    wstring tempPath = originalPath + L".tmp";

    if (CopyFile(originalPath.c_str(), tempPath.c_str(), FALSE)) {
        WSM_Log("Core: Logic.dll copied to temp successfully.");
    }
    else {
        WSM_Log("Core: Warning - Could not copy Logic.dll, trying existing temp file.");
    }
    string pathStr(tempPath.begin(), tempPath.end());
    WSM_Log(("Core: Trying to load from temp: " + pathStr).c_str());

    hLogicModule = LoadLibrary(tempPath.c_str());

    if (!hLogicModule) {
        hLogicModule = LoadLibrary(originalPath.c_str());
    }

    if (!hLogicModule) {
        WSM_Log("Core: Failed to load Logic.dll!");
    }
    else {
        WSM_Log("Core: Logic.dll loaded successfully!");
    }
}

BOOL CALLBACK FindTaskbar(HWND hwnd, LPARAM lParam) {
    WCHAR className[256];
    GetClassName(hwnd, className, 256);
    /* according to MSDN maximal length of classNames inside windows architeture is 256 in most cases(*2 caused by WCHAR size = 512 bytes in memory)
        we want to use as little amounts of memory as possible to keep system unaffected
    */
    if (wcscmp(className, L"Shell_TrayWnd") == 0) {
        hTaskbar = hwnd;
        return FALSE;
    }
    return TRUE;
}

void ProcessCommand(const char* command) {
    if (strcmp(command, "RELOAD") == 0) {
        LoadLogic();
        return;
    }
    lock_guard<mutex> lock(logicMutex);
    if (hLogicModule) {
        typedef void (*LogicFunc)(const char*, HWND);
        LogicFunc func = (LogicFunc)GetProcAddress(hLogicModule, "ExecuteLogic");
        if (func) {
            HWND target = hIsland ? hIsland : hTaskbar;
            func(command, target);
        }
    }
}
LRESULT CALLBACK IslandProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (currentRenderer) {
            currentRenderer->OnPaint(hdc, 400, 60);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_USER + 100: {}
   
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
HWND CreateIslandWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = IslandProc; //simple functions handler attribute
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TaskbarIslandClass";
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        //attributes
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"TaskbarIslandClass", L"Island",
        WS_POPUP | WS_VISIBLE,
        500, 1000, 400, 60, //position and size (x, y, width, height)
        NULL, NULL, hInstance, NULL
    );

    //DWM styles and prefetences
    int preference = 2; //Round 33
    DwmSetWindowAttribute(hwnd, 33, &preference, sizeof(preference));

    int backdrop = 2; //Mica acrylic 19
    DwmSetWindowAttribute(hwnd, 19, &backdrop, sizeof(backdrop));

    return hwnd;
}

DWORD WINAPI PipeServer(LPVOID lpParam) {
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
    return 0;
}

DWORD WINAPI Initialize(LPVOID lpParam) {
    Sleep(1000);
    EnumWindows(FindTaskbar, 0);
    HINSTANCE hInstance = (HINSTANCE)lpParam;
    if (hTaskbar) {
        ShowWindow(hTaskbar, SW_HIDE);
        WSM_Log("Core: System Taskbar hidden.");
        
        hIsland = CreateIslandWindow(hInstance);
        WSM_Log("Core: Island Window created.");
    }
    LoadLogic();
    HANDLE hPipeThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PipeServer, NULL, 0, NULL);
    if (hPipeThread) CloseHandle(hPipeThread);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Initialize, hModule, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (saInitialized && sa.lpSecurityDescriptor) {
            LocalFree(sa.lpSecurityDescriptor);
        }
    }
    return TRUE;
}