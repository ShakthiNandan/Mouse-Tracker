#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include "snail_game.h"

using namespace Gdiplus;
#pragma comment (lib,"gdiplus.lib")

// Global Variables
const wchar_t CONTROL_CLASS_NAME[] = L"SnailControlClass";
const wchar_t OVERLAY_CLASS_NAME[] = L"SnailOverlayClass";

SnailGame g_snailGame;
ULONG_PTR gdiplusToken;
HWND hStartBtn;
HWND hOverlay = NULL;
HWND hControlWnd = NULL;
Bitmap* g_pTrailMap = NULL;
Graphics* g_pTrailGfx = NULL;

// Layout constants
const int LIST_START_Y = 100;
const int ITEM_HEIGHT = 30;

// Forward declarations
LRESULT CALLBACK ControlProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK OverlayProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASS wcControl = {0};
    wcControl.lpfnWndProc = ControlProc;
    wcControl.hInstance = hInstance;
    wcControl.lpszClassName = CONTROL_CLASS_NAME;
    wcControl.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcControl.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcControl);

    WNDCLASS wcOverlay = {0};
    wcOverlay.lpfnWndProc = OverlayProc;
    wcOverlay.hInstance = hInstance;
    wcOverlay.lpszClassName = OVERLAY_CLASS_NAME;
    wcOverlay.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcOverlay.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcOverlay);

    g_snailGame.Init(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    hControlWnd = CreateWindowEx(0, CONTROL_CLASS_NAME, L"Snail Game Control", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, 400, 600, NULL, NULL, hInstance, NULL);

    if (hControlWnd == NULL) return 0;
    ShowWindow(hControlWnd, nCmdShow);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    GdiplusShutdown(gdiplusToken);
    return 0;
}

LRESULT CALLBACK ControlProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        hStartBtn = CreateWindow(L"BUTTON", L"SPAWN SNAIL", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 50, 20, 280, 50, hwnd, (HMENU)1, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        // Start the overlay immediately if not already? Or wait for first spawn?
        // Let's create overlay if not exists.
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // SPAWN SNAIL
            if (hOverlay == NULL) {
                hOverlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, OVERLAY_CLASS_NAME, L"SnailOverlay", WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);
                SetLayeredWindowAttributes(hOverlay, RGB(0,0,0), 0, LWA_COLORKEY);
                SetTimer(hwnd, 1, 16, NULL); // ~60 FPS update
            }
            g_snailGame.SpawnSnail();
            InvalidateRect(hwnd, NULL, TRUE); // Repaint list
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw list of snails
            const auto& snails = g_snailGame.GetSnails();
            int y = LIST_START_Y;
            
            SetBkMode(hdc, TRANSPARENT);
            HFONT hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
            HGDIOBJ oldFont = SelectObject(hdc, hFont);

            for (const auto& s : snails) {
                // Background for strip
                RECT rRow = {10, y, 380, y + ITEM_HEIGHT};
                // FillRect for highlight? No, keep simple.

                // Draw Color Box
                RECT rColor = {20, y + 5, 40, y + 25};
                // Extract RGB from s.color (0xFFRRGGBB)
                int red = (s.color >> 16) & 0xFF;
                int grn = (s.color >> 8) & 0xFF;
                int blu = (s.color) & 0xFF;
                HBRUSH hBr = CreateSolidBrush(RGB(red, grn, blu));
                FillRect(hdc, &rColor, hBr);
                DeleteObject(hBr);

                // Draw Text
                wchar_t text[64];
                wsprintf(text, L"Snail #%d", s.id);
                TextOut(hdc, 50, y + 5, text, lstrlen(text));

                // Draw X Button (Red Box with X)
                RECT rX = {340, y + 5, 360, y + 25};
                HBRUSH hBrRed = CreateSolidBrush(RGB(200, 50, 50));
                FillRect(hdc, &rX, hBrRed);
                DeleteObject(hBrRed);
                
                SetTextColor(hdc, RGB(255, 255, 255));
                TextOut(hdc, 345, y + 5, L"X", 1);
                SetTextColor(hdc, RGB(0, 0, 0));

                y += ITEM_HEIGHT;
            }

            SelectObject(hdc, oldFont);
            DeleteObject(hFont);
            EndPaint(hwnd, &ps);
        }
        break;

    case WM_LBUTTONUP:
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);

            // Check if clicked ON an X
            const auto& snails = g_snailGame.GetSnails();
            int y = LIST_START_Y;
            bool changed = false;
            
            // Iterate backwards so removing doesn't mess up
            // Actually, we just want to find WHICH one we clicked.
            // Since coordinates are clear...
            for (const auto& s : snails) {
                if (xPos >= 340 && xPos <= 360 && yPos >= y + 5 && yPos <= y + 25) {
                    g_snailGame.RemoveSnail(s.id);
                    changed = true;
                    break; 
                }
                y += ITEM_HEIGHT;
            }
            
            if (changed) {
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        break;

    case WM_TIMER:
        if (wParam == 1 && hOverlay) {
            g_snailGame.Update();
            
            // Lazy init persistent bitmap
            if (!g_pTrailMap) {
                 g_pTrailMap = new Bitmap(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), PixelFormat32bppARGB);
                 g_pTrailGfx = new Graphics(g_pTrailMap);
                 g_pTrailGfx->SetSmoothingMode(SmoothingModeAntiAlias);
                 g_pTrailGfx->Clear(Color(0,0,0)); // Initialize with Black (transparent key)
            }

            // Draw new segments to persistent bitmap
            const auto& snails = g_snailGame.GetSnails();
            for (const auto& s : snails) {
                if (abs(s.x - s.prevX) > 0.1f || abs(s.y - s.prevY) > 0.1f) {
                     int a = (s.color >> 24) & 0xFF;
                     int r = (s.color >> 16) & 0xFF;
                     int g = (s.color >> 8) & 0xFF;
                     int b = (s.color) & 0xFF;
                     
                     // Use the random color
                     Color trailColor(255, r, g, b);
                     Pen pen(trailColor, 4.0f);
                     pen.SetStartCap(LineCapRound);
                     pen.SetEndCap(LineCapRound);
                     
                     g_pTrailGfx->DrawLine(&pen, s.prevX, s.prevY, s.x, s.y);
                }
            }

            InvalidateRect(hOverlay, NULL, FALSE);
            
            // Check for Escape to exit
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                 // Maybe just clear all trails? Or Exit?
                 // Let's just minimize or hide overlay?
                 // User might just want to stop looking at it.
                 // Let's just do nothing for now, or maybe toggle overlay visibility
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Double buffer setup
            HDC hdcMem = CreateCompatibleDC(hdc);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

            // Clear background with black (transparent key)
            HBRUSH hBr = CreateSolidBrush(RGB(0,0,0));
            FillRect(hdcMem, &rc, hBr);
            DeleteObject(hBr);

            Graphics g(hdcMem);
            g.SetSmoothingMode(SmoothingModeAntiAlias);

            const auto& snails = g_snailGame.GetSnails();
            
            // Draw Persistent Trails
            if (g_pTrailMap) {
                g.DrawImage(g_pTrailMap, 0, 0);
            }
            
            for (const auto& snail : snails) {
                int a = (snail.color >> 24) & 0xFF;
                int r = (snail.color >> 16) & 0xFF;
                int gr = (snail.color >> 8) & 0xFF;
                int b = (snail.color) & 0xFF;

                // Draw Snail (Head)
                SolidBrush brush(Color(255, r, gr, b)); 
                g.FillEllipse(&brush, (INT)snail.x - 10, (INT)snail.y - 10, 20, 20);
            }
            
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
            SelectObject(hdcMem, oldBm);
            DeleteObject(hbm);
            DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);
        }
        break;
    case WM_ERASEBKGND:
        return 1; 
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
