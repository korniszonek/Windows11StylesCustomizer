#include "pch.h"
#include <windows.h>
#include <dwmapi.h>
#include <iostream>
#include <string>
#include <unordered_map>
#pragma comment(lib, "dwmapi.lib")
using namespace std;
enum class CommandType {
    RELOAD,
    UNKNOWN,
    ALPHA,
    COLOR,
    BLUR,
    HIDE,
    CLEAN
};
static const unordered_map<string, CommandType> commandMap = {
    {"RELOAD",CommandType::RELOAD},
    {"ALPHA", CommandType::ALPHA},
    {"COLOR", CommandType::COLOR},
    {"BLUR",  CommandType::BLUR},
    {"HIDE",  CommandType::HIDE},
    {"CLEAN", CommandType::CLEAN}
};
enum WindowCompositionAttribute {
    WCA_ACCENT_POLICY = 19
};

struct ACCENT_POLICY {
    int nAccentState;   // 0=None, 1=Blur, 2=Acrylic, 3=Transparent
    int nFlags;         // 2 = Blur, 0 = Inactive
    int nColor;         // ARGB format
    int nAnimationId;
};

struct WINDOWCOMPOSITIONATTRIBUTEDATA {
    WindowCompositionAttribute nAttribute;
    PVOID pData;
    ULONG ulDataSize;
};

void CleanTaskbar(HWND hTaskbar) {
    if (hTaskbar == NULL) {
        return;
    }
    HWND hTray = FindWindowEx(hTaskbar, NULL, L"TrayNotifyWnd", NULL);
    if (hTray) {
        ShowWindow(hTray, SW_HIDE);
    }
    HWND hClock = FindWindowEx(hTray, NULL, L"TrayClockWClass", NULL);
    if (hClock) {
        ShowWindow(hClock, SW_HIDE);
    }
    HWND hShowDesktop = FindWindowEx(hTaskbar, NULL, L"TrayShowDesktopButtonWClass", NULL);
    if (hShowDesktop) { 
        ShowWindow(hShowDesktop, SW_HIDE);
    }
    WSM_Log("Logic: Tray is hidden now");
}

typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBUTEDATA*);

extern "C" __declspec(dllexport) void __stdcall ExecuteLogic(const char* command, HWND hwnd) {
    string fullCmd(command);
    size_t colonPos = fullCmd.find(':');

    std::string action = (colonPos != std::string::npos) ? fullCmd.substr(0, colonPos) : fullCmd;
    std::string value = (colonPos != std::string::npos) ? fullCmd.substr(colonPos + 1) : "";

    CommandType type = CommandType::UNKNOWN;
    if (commandMap.find(action) != commandMap.end()) {
        type = commandMap.at(action);
    }

    switch (type) {
        case CommandType::ALPHA: { 
            WSM_Log(("Logic: Setting Alpha to " + value).c_str());
            int alphaValue = stoi(value);
            if (alphaValue < 0) alphaValue = 0;
            if (alphaValue > 255) alphaValue = 255;

            LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

            if (!(exStyle & WS_EX_LAYERED)) {
                SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
            }

            if (SetLayeredWindowAttributes(hwnd, 0, (BYTE)alphaValue, LWA_ALPHA)) {
                WSM_Log(("Logic: Alpha set to" + to_string(alphaValue)).c_str());
            }
            else {
                WSM_Log("Logic: Failed to set Alpha");
            }
        } break; 

        case CommandType::COLOR: {
            WSM_Log(("Logic: Changing Color to " + value).c_str());
        } break;

        case CommandType::BLUR: {
            
            int blurType = std::stoi(value); // 0=None, 1=Blur, 2=Acrylic
            if (blurType < 0) blurType = 0;
            if (blurType > 2) blurType = 2;

            HMODULE hUser32 = GetModuleHandle(L"user32.dll");
            if (hUser32) {
                auto SetWindowCompositionAttribute = (pSetWindowCompositionAttribute)GetProcAddress(hUser32, "SetWindowCompositionAttribute");

                if (SetWindowCompositionAttribute) {
                    ACCENT_POLICY policy = { 2 };

                    policy.nAccentState = blurType; // 1 = Blur, 2 = Acrylic
                    policy.nFlags = 0; // Włączony efekt
                    // 0xAABBGGRR
                    // AA = 80 (50% blur)
                    // BB = 00 (Blue)
                    // GG = 00 (Green)
                    // RR = 00 (Red)
                    policy.nColor = 0x99FFFFFF; 

                    WINDOWCOMPOSITIONATTRIBUTEDATA data = { WCA_ACCENT_POLICY, &policy, sizeof(policy) };

                    if (SetWindowCompositionAttribute(hwnd, &data)) {
                        WSM_Log("Logic: SCA Blur applied successfully!");
                    }
                    else {
                        WSM_Log("Logic: SCA failed!");
                    }
                }
            }
        } break;

        case CommandType::HIDE: {
            if (value == "1") {
                ShowWindow(hwnd, SW_HIDE);
                WSM_Log("Logic: Taskbar hidden");
            }
            else {
                ShowWindow(hwnd, SW_SHOW);
                WSM_Log("Logic: Taskbar shown");
            }
        } break;
        
        case CommandType::CLEAN: {
            if (hwnd) {
                CleanTaskbar(hwnd);
                WSM_Log("Logic: CleanTaskbar executed.");
            }
            else {
                WSM_Log("Logic: hTaskbar is NULL, cannot clean!");
            }
        }break;

        default:
            WSM_Log("Logic: Unknown command");
            break;
    }
    
}