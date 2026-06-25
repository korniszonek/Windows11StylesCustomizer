/* ProcessCommand - Processing command from pipe (atm only checks if piping works)
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
#include <vector>
#include <algorithm>
#include <string>
#pragma comment(lib, "dwmapi.lib")
#include "../IRenderer.h"
#include "../ConfigModel.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
HMODULE hLogicModule = NULL;
mutex logicMutex;
SECURITY_ATTRIBUTES sa;
bool saInitialized = false;
HWND hTaskbar = NULL;
HWND hIsland = NULL;

HMODULE hStyleModule = NULL;
IRenderer* g_Renderer = nullptr;

UINT g_ShellHookMsg = 0;

DynamicConfig g_Config;
mutex g_ConfigMutex;

void UpdateIslandPosition();

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
        we use pipe server to reload logic which is an independent service.
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

void UpdateIslandPosition() {
    if (!hIsland || !g_Renderer) return;

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int islandWidth = g_Renderer->GetRequiredWidth(g_Config);
    int islandHeight = 60;

    int posX = (screenWidth / 2) - (islandWidth / 2);
    int posY = screenHeight - islandHeight - 10;

    SetWindowPos(hIsland, HWND_TOPMOST, posX, posY, islandWidth, islandHeight, SWP_NOACTIVATE | SWP_NOCOPYBITS);
    InvalidateRect(hIsland, NULL, TRUE);
}

void LoadStyle(wstring styleName) {
    if (g_Renderer) {
        delete g_Renderer;
        g_Renderer = nullptr;
    }
    if (hStyleModule) {
        FreeLibrary(hStyleModule);
        hStyleModule = NULL;
    }

    WCHAR dllPath[MAX_PATH];
    GetModuleFileName((HMODULE)&__ImageBase, dllPath, MAX_PATH);
    wstring strPath = dllPath;
    size_t pos = strPath.find_last_of(L"\\/");
    wstring baseDir = strPath.substr(0, pos + 1);

    wstring fullPath = baseDir + L"Styles\\" + styleName + L".dll";

    HMODULE hStyle = LoadLibrary(fullPath.c_str());
    if (!hStyle) {
        WSM_Log("Core: Failed to load style DLL!");
        return;
    }

    typedef IRenderer* (*CreateRendererFunc)();
    auto CreateRenderer = (CreateRendererFunc)GetProcAddress(hStyle, "CreateRenderer");

    if (CreateRenderer) {
        g_Renderer = CreateRenderer();
        hStyleModule = hStyle;
        UpdateIslandPosition();
        WSM_Log("Core: Style loaded successfully.");
    }
    else {
        WSM_Log("Core: Failed to find CreateRenderer in style DLL!");
        FreeLibrary(hStyle);
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
    string cmdStr(command);

    // Handling File Reload 
    if (cmdStr == "RELOAD") {
        LoadLogic();
        return;
    }

    // Handling Change of styleFile
    if (cmdStr.find("STYLE:") == 0) {
        string styleNameStr = cmdStr.substr(6); //without STYLE:
        wstring styleNameW(styleNameStr.begin(), styleNameStr.end());

        LoadStyle(styleNameW);
        InvalidateRect(hIsland, NULL, TRUE);
        return;
    }

    // Handling rest of commands hide, alpha etc.
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
    if (msg == g_ShellHookMsg && g_ShellHookMsg != 0) {
        // 1 = Window Created, 2 = Window Destroyed, 6 = Redraw 
        if (wParam == 1 || wParam == 2 || wParam == 6) {
            if (g_Renderer) {
                g_Renderer->OnCommand("REFRESH");
                UpdateIslandPosition();
            }
        }
        return 0;
    }
    switch (msg) {
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        DynamicConfig configCopy;
        {
            lock_guard<mutex> lock(g_ConfigMutex);
            configCopy = g_Config;
            g_Config.isDirty = false;
        }

        if (g_Renderer) {
            g_Renderer->OnPaint(hdc, width, height, configCopy);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_LBUTTONDOWN: {
        int mouseX = LOWORD(lParam);
        int mouseY = HIWORD(lParam);
        if (g_Renderer) {
            g_Renderer->OnMouseClick(mouseX, mouseY);
        }
        return 0;
    }
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

    //DWM styles and preferences
    int preference = 2; //Round 33
    DwmSetWindowAttribute(hwnd, 33, &preference, sizeof(preference));

    int backdrop = 2; //Mica acrylic 19
    DwmSetWindowAttribute(hwnd, 19, &backdrop, sizeof(backdrop));

    BOOL disableTransitions = TRUE;
    DwmSetWindowAttribute(hwnd, 3, &disableTransitions, sizeof(disableTransitions)); // DWMWA_DISALLOW_PEEK

    return hwnd;
}

void PipeServerThread(HWND* phwndIsland) {
    LPCWSTR pipeName = L"\\\\.\\pipe\\WindowsTaskbarConfigPipe";

    while (true) {
        HANDLE hPipe = CreateNamedPipeW(
            pipeName,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1, 4096, 4096, 0, NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            vector<char> buffer(4096);
            DWORD bytesRead = 0;

            if (ReadFile(hPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, NULL)) {
                buffer[bytesRead] = '\0';

                string rawCmd(buffer.data());
                if (rawCmd == "RELOAD" || rawCmd.find("STYLE:") == 0) {
                    ProcessCommand(buffer.data());
                }
                else {
                    try {
                        auto data = json::parse(buffer.data());

                        if (data.contains("command") && data["command"] == "UPDATE_STYLE") {
                            {
                                lock_guard<mutex> lock(g_ConfigMutex);

                                // if some value is not declared it remains unchanged
                                // we use clamp to prevent incorect value - instead we round them to the clostest, by using clamp
                                g_Config.radius = clamp((int)data.value("radius", g_Config.radius), 0, 50);
                                g_Config.padding = clamp((int)data.value("padding", g_Config.padding), 2, 40);
                                g_Config.iconSize = clamp((int)data.value("iconSize", g_Config.iconSize), 16, 64);

                                if (data.contains("bg_color")) {
                                    auto bg = data["bg_color"];
                                    g_Config.bgA = bg.value("a", g_Config.bgA);
                                    g_Config.bgR = bg.value("r", g_Config.bgR);
                                    g_Config.bgG = bg.value("g", g_Config.bgG);
                                    g_Config.bgB = bg.value("b", g_Config.bgB);
                                }

                                if (data.contains("border_color")) {
                                    auto bc = data["border_color"];
                                    g_Config.borderA = bc.value("a", g_Config.borderA);
                                    g_Config.borderR = bc.value("r", g_Config.borderR);
                                    g_Config.borderG = bc.value("g", g_Config.borderG);
                                    g_Config.borderB = bc.value("b", g_Config.borderB);
                                }

                                g_Config.isDirty = true;
                            }

                            UpdateIslandPosition();
                        }
                    }
                    catch (const exception&) {
                        WSM_Log("Incorrect Json or Command Format");
                    }
                }
            }
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
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

        typedef BOOL(WINAPI* RegisterShellHookWindowFunc)(HWND);
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            auto RegisterShellHookWindow = (RegisterShellHookWindowFunc)GetProcAddress(hUser32, "RegisterShellHookWindow");
            if (RegisterShellHookWindow) {
                RegisterShellHookWindow(hIsland);
                g_ShellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");
            }
        }
    }
    LoadLogic();
    UpdateIslandPosition();

    thread(PipeServerThread, &hIsland).detach();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Initialize, hModule, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_Renderer) delete g_Renderer;
        if (hStyleModule) FreeLibrary(hStyleModule);

        if (saInitialized && sa.lpSecurityDescriptor) {
            LocalFree(sa.lpSecurityDescriptor);
        }
    }
    return TRUE;
}