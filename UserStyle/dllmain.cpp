#include "pch.h"
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <vector>
#include <gdiplus.h>
#include <shellapi.h> 
#include <mutex>
#include <dwmapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

#include "../IRenderer.h" 

using namespace Gdiplus;

struct AppIcon {
    HWND hwndTarget;
    HICON hIcon;
    Gdiplus::Bitmap* bmp;
    RECT hitBox;
    bool isStartButton;
};

std::vector<AppIcon> g_AppIcons;
std::mutex g_AppsMutex;

Gdiplus::Bitmap* CreateBitmapFromHICON_Secure(HICON hIcon) {
    if (!hIcon) return nullptr;

    ICONINFO iconInfo = { 0 };
    if (!GetIconInfo(hIcon, &iconInfo)) return nullptr;

    BITMAP bmColor = { 0 };
    GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmColor);

    int width = bmColor.bmWidth;
    int height = bmColor.bmHeight;

    HDC hdc = GetDC(NULL);

    // Force 32 bit format with alpha channel
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<DWORD> pixels(width * height);

    GetDIBits(hdc, iconInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);

    bool hasAlpha = false;
    for (int i = 0; i < width * height; ++i) {
        if ((pixels[i] & 0xFF000000) != 0) {
            hasAlpha = true;
            break;
        }
    }

    if (!hasAlpha && bmColor.bmBitsPixel == 32) {
        for (int i = 0; i < width * height; ++i) {
            pixels[i] |= 0xFF000000;
        }
    }

    Gdiplus::Bitmap* finalBitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
    Gdiplus::BitmapData bmpData;
    Gdiplus::Rect rect(0, 0, width, height);

    if (finalBitmap->LockBits(&rect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData) == Gdiplus::Ok) {
        BYTE* dest = (BYTE*)bmpData.Scan0;
        BYTE* src = (BYTE*)pixels.data();
        memcpy(dest, src, width * height * 4);
        finalBitmap->UnlockBits(&bmpData);
    }

    if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);

    return finalBitmap;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if ((style & WS_POPUP) && !(style & WS_SYSMENU)) return TRUE;

    int length = GetWindowTextLengthW(hwnd);
    if (length == 0) return TRUE;

    wchar_t className[256];
    if (GetClassNameW(hwnd, className, 256)) {
        std::wstring cls(className);

        if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" || cls == L"Shell_SecondaryTrayWnd") {
            return TRUE;
        }

        if (cls == L"ApplicationFrameWindow") {
            int cloaked = 0;
            HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
            if (SUCCEEDED(hr) && cloaked != 0) {
                return TRUE;
            }
        }
    }

    HICON hIcon = NULL;
    hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_BIG, 0);

    if (!hIcon) {
        hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
    }
    if (!hIcon) {
        hIcon = (HICON)SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0);
    }
    if (!hIcon) {
        hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    }

    if (!hIcon) {
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
    }

    if (hIcon) {
        AppIcon app;
        app.hwndTarget = hwnd;
        app.hIcon = hIcon;
        app.bmp = CreateBitmapFromHICON_Secure(hIcon);
        app.hitBox = { 0,0,0,0 };
        app.isStartButton = false;
        g_AppIcons.push_back(app);
    }
    return TRUE;
}

void RefreshApplications() {
    std::lock_guard<std::mutex> lock(g_AppsMutex);

    for (auto& app : g_AppIcons) {
        if (app.bmp) delete app.bmp;
    }
    g_AppIcons.clear();

    // Injected Vector Start Button
    AppIcon startApp;
    startApp.hwndTarget = NULL;
    startApp.hIcon = NULL;
    startApp.bmp = nullptr;
    startApp.hitBox = { 0,0,0,0 };
    startApp.isStartButton = true;
    g_AppIcons.push_back(startApp);

    EnumWindows(EnumWindowsProc, 0);

    std::sort(g_AppIcons.begin(), g_AppIcons.end(), [](const AppIcon& a, const AppIcon& b) {
        if (a.isStartButton) return true;
        if (b.isStartButton) return false;

        DWORD pidA = 0, pidB = 0;
        GetWindowThreadProcessId(a.hwndTarget, &pidA);
        GetWindowThreadProcessId(b.hwndTarget, &pidB);
        return pidA < pidB;
        });
}

class UserPresetRenderer : public IRenderer {
    ULONG_PTR gdiplusToken;
    GraphicsPath* path = nullptr;
    SolidBrush* baseBrush = nullptr;
    SolidBrush* startBrush = nullptr;
    Pen* borderPen = nullptr;
    int lastW = 0, lastH = 0;

    void EnsureResources(int w, int h, const DynamicConfig& config) {
        if (w != lastW || h != lastH || config.isDirty) {
            if (baseBrush) delete baseBrush;
            if (startBrush) delete startBrush;
            if (borderPen) delete borderPen;
            if (path) delete path;

            path = new GraphicsPath();
            int r = config.radius; 
            path->AddArc(0, 0, r, r, 180, 90);
            path->AddArc(w - r, 0, r, r, 270, 90);
            path->AddArc(w - r, h - r, r, r, 0, 90);
            path->AddArc(0, h - r, r, r, 90, 90);
            path->CloseFigure();

            baseBrush = new SolidBrush(Color(config.bgA, config.bgR, config.bgG, config.bgB));
            borderPen = new Pen(Color(config.borderA, config.borderR, config.borderG, config.borderB), 1.0f);

            startBrush = new SolidBrush(Color(config.borderA, config.borderR, config.borderG, config.borderB));

            lastW = w; lastH = h;
        }
    }

public:
    UserPresetRenderer() {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
        RefreshApplications();
    }

    ~UserPresetRenderer() {
        if (baseBrush) delete baseBrush;
        if (startBrush) delete startBrush;
        if (borderPen) delete borderPen;
        if (path) delete path;

        for (auto& app : g_AppIcons) {
            if (app.bmp) delete app.bmp;
        }
        g_AppIcons.clear();
        GdiplusShutdown(gdiplusToken);
    }

    void OnPaint(HDC hdc, int width, int height, const DynamicConfig& config) override {
        EnsureResources(width, height, config);

        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);

        graphics.FillPath(baseBrush, path);
        graphics.DrawPath(borderPen, path);

        std::lock_guard<std::mutex> lock(g_AppsMutex);

        int startX = 22;
        int iconSize = config.iconSize; 
        int padding = config.padding;   
        int posY = (height - iconSize) / 2;

        for (size_t i = 0; i < g_AppIcons.size(); ++i) {
            if (g_AppIcons[i].isStartButton) {
                int s = iconSize;
                int half = s / 2;
                int gap = 2;

                graphics.FillRectangle(startBrush, startX, posY, half - gap, half - gap);
                graphics.FillRectangle(startBrush, startX + half, posY, half - gap, half - gap);
                graphics.FillRectangle(startBrush, startX, posY + half, half - gap, half - gap);
                graphics.FillRectangle(startBrush, startX + half, posY + half, half - gap, half - gap);

                g_AppIcons[i].hitBox = { startX, posY, startX + iconSize, posY + iconSize };
                startX += iconSize + padding;
            }
            else if (g_AppIcons[i].bmp) {
                graphics.DrawImage(g_AppIcons[i].bmp, startX, posY, iconSize, iconSize);
                g_AppIcons[i].hitBox = { startX, posY, startX + iconSize, posY + iconSize };
                startX += iconSize + padding;
            }
        }
    }

    void OnMouseClick(int x, int y) override {
        std::lock_guard<std::mutex> lock(g_AppsMutex);

        for (const auto& app : g_AppIcons) {
            if (x >= app.hitBox.left && x <= app.hitBox.right &&
                y >= app.hitBox.top && y <= app.hitBox.bottom) {

                if (app.isStartButton) {
                    keybd_event(VK_CONTROL, 0, 0, 0);
                    keybd_event(VK_ESCAPE, 0, 0, 0);
                    keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
                    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                    break;
                }

                HWND hwndTarget = app.hwndTarget;
                if (!IsWindow(hwndTarget)) continue;

                LONG style = GetWindowLong(hwndTarget, GWL_STYLE);
                bool isMinimized = (style & WS_MINIMIZE) != 0;

                if (isMinimized) {
                    ShowWindow(hwndTarget, SW_RESTORE);
                    SendMessageW(hwndTarget, WM_SYSCOMMAND, SC_RESTORE, 0);
                    SetForegroundWindow(hwndTarget);
                }
                else {
                    ShowWindowAsync(hwndTarget, SW_MINIMIZE);
                    PostMessageW(hwndTarget, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                }

                break;
            }
        }
    }

    void OnCommand(const char* command) override {
        if (std::string(command) == "REFRESH") RefreshApplications();
    }

    int GetRequiredWidth(const DynamicConfig& config) override {
        if (g_AppIcons.empty()) return 100;
        int width = (g_AppIcons.size() * config.iconSize) +
            ((g_AppIcons.size() - 1) * config.padding) +
            config.margins;
        return width;
    }
};

extern "C" __declspec(dllexport) IRenderer* CreateRenderer() {
    return new UserPresetRenderer();
}
