#include "pch.h"

#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <vector>

#include <gdiplus.h>
#include <shellapi.h> 
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#include "../IRenderer.h"
using namespace Gdiplus;

struct AppIcon {
    HWND hwndTarget;      // real target icon
    HICON hIcon;          // winApi icon
    Gdiplus::Bitmap* bmp; // bitmap gdi+
    RECT hitBox;          // positon (x,y, width,height)
};

std::vector<AppIcon> g_AppIcons;

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    int length = GetWindowTextLengthW(hwnd);
    if (length == 0) return TRUE;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!(style & WS_CAPTION)) return TRUE;

    HICON hIcon = NULL;

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        wchar_t exePath[MAX_PATH];
        DWORD dwSize = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &dwSize)) {
            SHFILEINFOW sfi = { 0 };
            uintptr_t ret = SHGetFileInfoW(exePath, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);
            if (ret && sfi.hIcon) {
                hIcon = sfi.hIcon;
            }
        }
        CloseHandle(hProcess);
    }

    if (!hIcon) {
        hIcon = (HICON)SendMessage(hwnd, WM_GETICON, ICON_BIG, 0);
        if (!hIcon) hIcon = (HICON)GetClassLongPtr(hwnd, GCLP_HICON);
    }

    if (hIcon) {
        AppIcon app;
        app.hwndTarget = hwnd;
        app.hIcon = hIcon;
        app.bmp = Gdiplus::Bitmap::FromHICON(hIcon);
        app.hitBox = { 0, 0, 0, 0 };
        g_AppIcons.push_back(app);
    }
    return TRUE;
}

void RefreshApplications() {
    for (auto& app : g_AppIcons) {
        if (app.bmp) delete app.bmp;
    }
    g_AppIcons.clear();
    EnumWindows(EnumWindowsProc, 0);

    std::sort(g_AppIcons.begin(), g_AppIcons.end(), [](const AppIcon& a, const AppIcon& b) {
        DWORD pidA = 0, pidB = 0;
        GetWindowThreadProcessId(a.hwndTarget, &pidA);
        GetWindowThreadProcessId(b.hwndTarget, &pidB);
        return pidA < pidB;
        });
}

class PinkRenderer : public IRenderer {
    ULONG_PTR gdiplusToken;
    GraphicsPath* path = nullptr;
    PathGradientBrush* pgb = nullptr;
    int lastW = 0, lastH = 0;

    void EnsureResources(int w, int h) {
        if (w != lastW || h != lastH) {
            if (pgb) delete pgb;
            if (path) delete path;

            path = new GraphicsPath();
            path->AddRectangle(Rect(0, 0, w, h)); 

            pgb = new PathGradientBrush(path);
            pgb->SetCenterPoint(PointF(w / 2.0f, h / 2.0f));
            pgb->SetCenterColor(Color(240, 255, 255, 255)); 

            Color colors[] = { Color(120, 255, 182, 193) }; 
            int count = 1;
            pgb->SetSurroundColors(colors, &count);

            pgb->SetFocusScales(0.85f, 0.10f); 

            lastW = w; lastH = h;
        }
    }

public:
    PinkRenderer() {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        RefreshApplications();
    }

    ~PinkRenderer() {
        if (pgb) delete pgb;
        if (path) delete path;

        for (auto& app : g_AppIcons) {
            if (app.bmp) delete app.bmp;
        }
        g_AppIcons.clear();

        GdiplusShutdown(gdiplusToken);
    }

    void OnPaint(HDC hdc, int width, int height) override {
        EnsureResources(width, height);

        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);

        graphics.FillPath(pgb, path);

        int startX = 20;
        int iconSize = 32;
        int padding = 15;
        int posY = (height - iconSize) / 2;

        for (size_t i = 0; i < g_AppIcons.size(); ++i) {
            if (g_AppIcons[i].bmp) {
                graphics.DrawImage(g_AppIcons[i].bmp, startX, posY, iconSize, iconSize);

                g_AppIcons[i].hitBox = { startX, posY, startX + iconSize, posY + iconSize };

                startX += iconSize + padding;
            }
        }
    }
    
    void OnMouseClick(int x, int y) override {
        for (const auto& app : g_AppIcons) {
            if (x >= app.hitBox.left && x <= app.hitBox.right &&
                y >= app.hitBox.top && y <= app.hitBox.bottom) {

                HWND hwndTarget = app.hwndTarget;
                if (!IsWindow(hwndTarget)) continue;

                if (IsIconic(hwndTarget)) {
                    ShowWindow(hwndTarget, SW_RESTORE);
                    SetForegroundWindow(hwndTarget);
                }
                else {
                    if (GetForegroundWindow() == hwndTarget) {
                        PostMessageW(hwndTarget, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                    }
                    else {
                        SetForegroundWindow(hwndTarget);
                    }
                }
                break; 
            }
        }
    }

    void OnCommand(const char* command) override {

    }

    int GetRequiredWidth() override {
        if (g_AppIcons.empty()) return 100;
        int iconSize = 32;
        int padding = 15;
        int margins = 40;
        
        int width = (g_AppIcons.size() * iconSize) + ((g_AppIcons.size() - 1) * padding) + margins;
        return width;
    }
};

extern "C" __declspec(dllexport) IRenderer* CreateRenderer() {
    return new PinkRenderer();
}