#include "pch.h"
#include <dwmapi.h>
#include <cstdio>
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>
#include <string>
#pragma comment(lib, "dwmapi.lib")
#include "../IRenderer.h"
#include "../ConfigModel.h"
#include <nlohmann/json.hpp>
#include "../Common.h"

using namespace std;
using json = nlohmann::json;

EXTERN_C IMAGE_DOS_HEADER __ImageBase;
HMODULE hLogicModule = NULL;
mutex logicMutex;
HWND hTaskbar = NULL;
HWND hIsland = NULL;

HMODULE hStyleModule = NULL;
IRenderer* g_Renderer = nullptr;
UINT g_ShellHookMsg = 0;

DynamicConfig g_Config;
mutex g_ConfigMutex;
HINSTANCE g_hInstance = NULL;

void UpdateIslandPosition();
void LoadLogic();
void LoadStyle(wstring styleName);
HWND CreateIslandWindow(HINSTANCE hInstance);
LRESULT CALLBACK IslandProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

wstring GetLogicPath() {
    WCHAR path[MAX_PATH];
    HMODULE hm = NULL;
    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetLogicPath, &hm)) {
        GetModuleFileName(hm, path, MAX_PATH);
    }
    wstring strPath = path;
    size_t pos = strPath.find_last_of(L"\\/");
    return strPath.substr(0, pos + 1) + L"taskbarLogic.dll";
}

BOOL CALLBACK FindTaskbar(HWND hwnd, LPARAM lParam) {
    WCHAR className[256];
    GetClassName(hwnd, className, 256);
    if (wcscmp(className, L"Shell_TrayWnd") == 0) {
        hTaskbar = hwnd;
        return FALSE;
    }
    return TRUE;
}

void ProcessCommand(const char* command) {
    if (!command) return;
    string cmdStr(command);

    WSM_Log(("ProcessCommand received: " + cmdStr).c_str());

    if (cmdStr.find("STYLE:") == 0) {
        string styleNameStr = cmdStr.substr(6);
        wstring styleNameW(styleNameStr.begin(), styleNameStr.end());
        LoadStyle(styleNameW);
        if (hIsland) InvalidateRect(hIsland, NULL, TRUE);
        return;
    }

    if (cmdStr == "RELOAD") {
        LoadLogic();
        return;
    }

    if (cmdStr.find("{") != string::npos || cmdStr.find("UPDATE_STYLE") != string::npos) {
        try {
            auto data = json::parse(command);

            if (data.contains("command") && data["command"] == "UPDATE_STYLE") {
                WSM_Log("Logic: UPDATE_STYLE matched inside JSON!"); 
                {
                    lock_guard<mutex> lock(g_ConfigMutex);

                    g_Config.radius = clamp((int)data.value("radius", g_Config.radius), 0, 50);
                    g_Config.padding = clamp((int)data.value("padding", g_Config.padding), 2, 40);
                    g_Config.iconSize = clamp((int)data.value("iconSize", g_Config.iconSize), 16, 64);

                    g_Config.bgA = (BYTE)data.value("bgA", g_Config.bgA);
                    g_Config.bgR = (BYTE)data.value("bgR", g_Config.bgR);
                    g_Config.bgG = (BYTE)data.value("bgG", g_Config.bgG);
                    g_Config.bgB = (BYTE)data.value("bgB", g_Config.bgB);

                    g_Config.borderA = (BYTE)data.value("borderA", g_Config.borderA);
                    g_Config.borderR = (BYTE)data.value("borderR", g_Config.borderR);
                    g_Config.borderG = (BYTE)data.value("borderG", g_Config.borderG);
                    g_Config.borderB = (BYTE)data.value("borderB", g_Config.borderB);

                    g_Config.isDirty = true;
                }

                UpdateIslandPosition();

                if (hIsland) {
                    InvalidateRect(hIsland, NULL, TRUE);
                    UpdateWindow(hIsland);
                }
            }
            else {
                WSM_Log("Logic: JSON valid, but 'command' field is missing or not 'UPDATE_STYLE'");
            }
        }
        catch (const exception& e) {
            string errMessage = "Incorrect Json Format: ";
            errMessage += e.what();
            WSM_Log(errMessage.c_str());
        }
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
//koffi export
extern "C" __declspec(dllexport) const char* ExecuteTaskbarLogic(const char* command) {
    if (!command) return "Error: Command is null";

    if (!hTaskbar) {
        EnumWindows(FindTaskbar, 0);
        if (hTaskbar) {
            ShowWindow(hTaskbar, SW_HIDE);
            hIsland = CreateIslandWindow(g_hInstance);

            HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
            if (hUser32) {
                typedef BOOL(WINAPI* RegisterShellHookWindowFunc)(HWND);
                auto RegisterShellHookWindow = (RegisterShellHookWindowFunc)GetProcAddress(hUser32, "RegisterShellHookWindow");
                if (RegisterShellHookWindow) {
                    RegisterShellHookWindow(hIsland);
                    g_ShellHookMsg = RegisterWindowMessageW(L"SHELLHOOK");
                }
            }

            LoadStyle(L"PureMinimal");
            LoadLogic();
            UpdateIslandPosition();

            thread([]() {
                MSG msg;
                while (GetMessage(&msg, NULL, 0, 0)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                }).detach();
        }
    }

    std::string cmdStr(command);
    if (cmdStr == "HIDE:1") {
        if (hTaskbar) ShowWindow(hTaskbar, SW_HIDE);
        if (hIsland) ShowWindow(hIsland, SW_HIDE);
        return "Success: Hidden";
    }
    if (cmdStr == "HIDE:0") {
        if (hTaskbar) ShowWindow(hTaskbar, SW_SHOW);
        if (hIsland) ShowWindow(hIsland, SW_SHOW);
        return "Success: Shown";
    }

    ProcessCommand(command);
    return "Command forwarded to ProcessCommand";
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

void LoadLogic() {
    lock_guard<mutex> lock(logicMutex);
    if (hLogicModule) { FreeLibrary(hLogicModule); hLogicModule = NULL; }
    wstring originalPath = GetLogicPath();
    wstring tempPath = originalPath + L".tmp";
    CopyFile(originalPath.c_str(), tempPath.c_str(), FALSE);
    hLogicModule = LoadLibrary(tempPath.c_str());
    if (!hLogicModule) hLogicModule = LoadLibrary(originalPath.c_str());
}

void LoadStyle(wstring styleName) {
    if (g_Renderer) { delete g_Renderer; g_Renderer = nullptr; }
    if (hStyleModule) { FreeLibrary(hStyleModule); hStyleModule = NULL; }

    WCHAR dllPath[MAX_PATH];
    GetModuleFileName((HMODULE)&__ImageBase, dllPath, MAX_PATH);
    wstring strPath = dllPath;
    size_t pos = strPath.find_last_of(L"\\/");
    wstring baseDir = strPath.substr(0, pos + 1);
    wstring fullPath = baseDir + L"Styles\\" + styleName + L".dll";

    HMODULE hStyle = LoadLibrary(fullPath.c_str());
    if (!hStyle) return;

    typedef IRenderer* (*CreateRendererFunc)();
    auto CreateRenderer = (CreateRendererFunc)GetProcAddress(hStyle, "CreateRenderer");
    if (CreateRenderer) {
        g_Renderer = CreateRenderer();
        hStyleModule = hStyle;
        UpdateIslandPosition();
    }
    else {
        FreeLibrary(hStyle);
    }
}

LRESULT CALLBACK IslandProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_ShellHookMsg && g_ShellHookMsg != 0) {
        if (wParam == 1 || wParam == 2 || wParam == 6) {
            if (g_Renderer) {
                g_Renderer->OnCommand("REFRESH");
                UpdateIslandPosition();
            }
        }
        return 0;
    }
    switch (msg) {
    case WM_SETCURSOR: SetCursor(LoadCursor(NULL, IDC_ARROW)); return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect; GetClientRect(hwnd, &rect);
        DynamicConfig configCopy;
        {
            lock_guard<mutex> lock(g_ConfigMutex);
            configCopy = g_Config;
            g_Config.isDirty = false;
        }
        if (g_Renderer) g_Renderer->OnPaint(hdc, rect.right - rect.left, rect.bottom - rect.top, configCopy);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_LBUTTONDOWN: {
        if (g_Renderer) g_Renderer->OnMouseClick(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND CreateIslandWindow(HINSTANCE hInstance) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = IslandProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"TaskbarIslandClass";
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"TaskbarIslandClass", L"Island",
        WS_POPUP | WS_VISIBLE,
        500, 1000, 400, 60,
        NULL, NULL, hInstance, NULL
    );

    int preference = 2; DwmSetWindowAttribute(hwnd, 33, &preference, sizeof(preference));
    int backdrop = 2; DwmSetWindowAttribute(hwnd, 19, &backdrop, sizeof(backdrop));
    BOOL disableTransitions = TRUE; DwmSetWindowAttribute(hwnd, 3, &disableTransitions, sizeof(disableTransitions));
    return hwnd;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hInstance = hModule;
    }
    else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        if (g_Renderer) delete g_Renderer;
        if (hStyleModule) FreeLibrary(hStyleModule);
        if (hTaskbar) ShowWindow(hTaskbar, SW_SHOW);
    }
    return TRUE;
}