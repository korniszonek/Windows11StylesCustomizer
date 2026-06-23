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
#include <string>
#include <mutex>

using namespace Gdiplus;

struct AppIcon {
    HWND hwndTarget;      // real target icon
    HICON hIcon;          // winApi icon
    Gdiplus::Bitmap* bmp; // bitmap gdi+
    RECT hitBox;          // positon (x,y, width,height)
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

    // we force 32 bit format with alpha chanel to eliminate black squares - which are hard to convert into alpha
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; 
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // allocation space in ram for icons
    std::vector<DWORD> pixels(width * height);

    GetDIBits(hdc, iconInfo.hbmColor, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);

    // check of alpha chanel
    // if alpha = 0 then its posibly 24 bit icon
    bool hasAlpha = false;
    for (int i = 0; i < width * height; ++i) {
        if ((pixels[i] & 0xFF000000) != 0) {
            hasAlpha = true;
            break;
        }
    }

    // alpha by hand for 32 bit format
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

    // cleanup
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
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(PixelOffsetModeHighQuality);

        graphics.FillPath(pgb, path);

        std::lock_guard<std::mutex> lock(g_AppsMutex);

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
        std::lock_guard<std::mutex> lock(g_AppsMutex);

        for (const auto& app : g_AppIcons) {
            if (x >= app.hitBox.left && x <= app.hitBox.right &&
                y >= app.hitBox.top && y <= app.hitBox.bottom) {

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
        std::string cmd(command);
        if (cmd == "REFRESH") {
            RefreshApplications();
        }
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