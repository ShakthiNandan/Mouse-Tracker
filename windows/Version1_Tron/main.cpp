#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <stdio.h>
#include <vector>
#include <deque>
#include <gdiplus.h>
#include "tron_game.h"

using namespace Gdiplus;
#pragma comment (lib,"gdiplus.lib")

// Global Variables
BOOL isTracking = FALSE;
FILE* logFile = NULL;
const wchar_t LOG_FILENAME[] = L"mouse_log.txt";
const wchar_t CONTROL_CLASS_NAME[] = L"ControlWindowClass";
const wchar_t TRAIL_CLASS_NAME[] = L"TrailWindowClass";
const wchar_t LIVE_OVERLAY_CLASS_NAME[] = L"LiveOverlayClass";
const wchar_t GAME_OVERLAY_CLASS_NAME[] = L"GameOverlayClass";
const wchar_t SETTINGS_FILENAME[] = L"settings.ini";

std::vector<POINT> g_trailPoints;
std::deque<POINT> g_livePoints;
std::vector<POINT> g_sessionHistory; // Optimization: Memory buffer instead of file
ULONG_PTR gdiplusToken;
TronGame g_tronGame;

// Settings
int g_interval = 50;      
int g_penWidth = 2;
COLORREF g_penColor = RGB(0, 255, 0); 
BOOL g_autoClear = TRUE; 
int g_trailLength = 20;
int g_tronAiCount = 3; 

// Initial Interface States
BOOL g_showLive = FALSE;
BOOL g_showResult = TRUE;
BOOL g_autoSave = FALSE;

// UI Handles
HWND hStartBtn, hStopBtn, hLiveCheck, hResultCheck, hAutoSaveCheck, hTronBtn;
HWND hLiveOverlay = NULL;
HWND hGameOverlay = NULL;

// Forward declarations
LRESULT CALLBACK ControlProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TrailProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LiveOverlayProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK GameOverlayProc(HWND, UINT, WPARAM, LPARAM);
void LoadPointsFromFile();
void LoadSettings();
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
void SaveScreenToJPG();

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

    WNDCLASS wcTrail = {0};
    wcTrail.lpfnWndProc = TrailProc;
    wcTrail.hInstance = hInstance;
    wcTrail.lpszClassName = TRAIL_CLASS_NAME;
    wcTrail.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); 
    wcTrail.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcTrail);

    WNDCLASS wcLive = {0};
    wcLive.lpfnWndProc = LiveOverlayProc;
    wcLive.hInstance = hInstance;
    wcLive.lpszClassName = LIVE_OVERLAY_CLASS_NAME;
    wcLive.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcLive.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcLive);

    WNDCLASS wcGame = {0};
    wcGame.lpfnWndProc = GameOverlayProc;
    wcGame.hInstance = hInstance;
    wcGame.lpszClassName = GAME_OVERLAY_CLASS_NAME;
    wcGame.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcGame.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcGame);

    LoadSettings();
    g_tronGame.Init(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

    wchar_t title[64];
    wsprintf(title, L"Tracker (Interval: %dms)", g_interval);
    HWND hwnd = CreateWindowEx(0, CONTROL_CLASS_NAME, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 250, 260, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) return 0;
    ShowWindow(hwnd, nCmdShow);

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
        hStartBtn = CreateWindow(L"BUTTON", L"START", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 100, 60, hwnd, (HMENU)1, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        hStopBtn = CreateWindow(L"BUTTON", L"STOP", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 120, 10, 100, 60, hwnd, (HMENU)2, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        EnableWindow(hStopBtn, FALSE);

        hLiveCheck = CreateWindow(L"BUTTON", L"Show Live Fading Trail", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 80, 200, 30, hwnd, (HMENU)3, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        if (g_showLive) SendMessage(hLiveCheck, BM_SETCHECK, BST_CHECKED, 0);

        hResultCheck = CreateWindow(L"BUTTON", L"Show Result Trail", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 110, 200, 30, hwnd, (HMENU)4, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        if (g_showResult) SendMessage(hResultCheck, BM_SETCHECK, BST_CHECKED, 0);

        hAutoSaveCheck = CreateWindow(L"BUTTON", L"Auto-Save on Stop", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 140, 200, 30, hwnd, (HMENU)5, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        if (g_autoSave) SendMessage(hAutoSaveCheck, BM_SETCHECK, BST_CHECKED, 0);

        hTronBtn = CreateWindow(L"BUTTON", L"PLAY TRON MODE", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 10, 180, 210, 30, hwnd, (HMENU)6, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // START
            isTracking = TRUE;
            g_livePoints.clear();
            g_sessionHistory.clear(); 
            
            // Re-read settings just in case
            LoadSettings(); 
            
            if (SendMessage(hLiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                hLiveOverlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, LIVE_OVERLAY_CLASS_NAME, L"LiveTrail", WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);
                SetLayeredWindowAttributes(hLiveOverlay, RGB(0,0,0), 0, LWA_COLORKEY);
            }
            SetTimer(hwnd, 1, g_interval, NULL); 
            EnableWindow(hStartBtn, FALSE);
            EnableWindow(hLiveCheck, FALSE); 
            EnableWindow(hResultCheck, FALSE); // Allow toggling? No, safer to lock.
            EnableWindow(hAutoSaveCheck, FALSE);
            EnableWindow(hTronBtn, FALSE);
            EnableWindow(hStopBtn, TRUE);
        }
        else if (LOWORD(wParam) == 2) { // STOP
            KillTimer(hwnd, 1);
            isTracking = FALSE;
            
            if (hLiveOverlay) { DestroyWindow(hLiveOverlay); hLiveOverlay = NULL; }

            EnableWindow(hStartBtn, TRUE);
            EnableWindow(hLiveCheck, TRUE);
            EnableWindow(hResultCheck, TRUE);
            EnableWindow(hAutoSaveCheck, TRUE);
            EnableWindow(hTronBtn, TRUE);
            EnableWindow(hStopBtn, FALSE);

            // Copy session history to result trail if we want to show it
            g_trailPoints = g_sessionHistory; // Transfer ownership/copy
            
            if (SendMessage(hAutoSaveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED && !g_trailPoints.empty()) {
                HWND hTrail = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, TRAIL_CLASS_NAME, L"Mouse Trail", WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);
                SetLayeredWindowAttributes(hTrail, RGB(0,0,0), 0, LWA_COLORKEY);
                ShowWindow(hTrail, SW_SHOWMAXIMIZED);
                UpdateWindow(hTrail);
                Sleep(100); 
                SaveScreenToJPG();
                MessageBox(hwnd, L"Auto-saved trail.jpg!", L"Saved", MB_OK);
                DestroyWindow(hTrail);
                return 0; 
            }

            if (!g_trailPoints.empty()) {
                if (SendMessage(hResultCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    HWND hTrail = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, TRAIL_CLASS_NAME, L"Mouse Trail", WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);
                    SetLayeredWindowAttributes(hTrail, RGB(0,0,0), 0, LWA_COLORKEY);
                    ShowWindow(hTrail, SW_SHOWMAXIMIZED);
                }
            } else {
                // Only show error if the user *wanted* to see the result
                if (SendMessage(hResultCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    MessageBox(hwnd, L"No points to draw.", L"Info", MB_OK);
                }
            }
        }
        else if (LOWORD(wParam) == 6) { // PLAY TRON
            if (hGameOverlay == NULL) {
                hGameOverlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, GAME_OVERLAY_CLASS_NAME, L"TronOverlay", WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);
                SetLayeredWindowAttributes(hGameOverlay, RGB(0,0,0), 0, LWA_COLORKEY);
                g_tronGame.StartGame(g_tronAiCount);
                SetTimer(hwnd, 2, 16, NULL); 
                EnableWindow(hStartBtn, FALSE);
                EnableWindow(hTronBtn, FALSE);
            }
        }
        break;

    case WM_TIMER:
        if (wParam == 1 && isTracking) {
            POINT p;
            if (GetCursorPos(&p)) {
                // Optimization: ONLY store history if the user actually wants the Result Trail
                // The user said: "don't keep the entire history if the show trail is not enabled"
                if (SendMessage(hResultCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    g_sessionHistory.push_back(p);
                }

                if (hLiveOverlay) {
                    g_livePoints.push_back(p);
                    if (g_livePoints.size() > (size_t)g_trailLength) g_livePoints.pop_front();
                    InvalidateRect(hLiveOverlay, NULL, TRUE);
                }
            }
        }
        else if (wParam == 2 && hGameOverlay) {
            g_tronGame.Update();
            InvalidateRect(hGameOverlay, NULL, FALSE);
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                 KillTimer(hwnd, 2);
                 DestroyWindow(hGameOverlay);
                 hGameOverlay = NULL;
                 EnableWindow(hStartBtn, TRUE);
                 EnableWindow(hTronBtn, TRUE);
            }
        }
        break;

    case WM_DESTROY:
        if (logFile) fclose(logFile);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TrailProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HPEN hPen = CreatePen(PS_SOLID, g_penWidth, g_penColor); 
            HGDIOBJ oldPen = SelectObject(hdc, hPen);
            if (g_trailPoints.size() > 0) {
                MoveToEx(hdc, g_trailPoints[0].x, g_trailPoints[0].y, NULL);
                for (size_t i = 1; i < g_trailPoints.size(); ++i) {
                    LineTo(hdc, g_trailPoints[i].x, g_trailPoints[i].y);
                }
            }
            SelectObject(hdc, oldPen);
            DeleteObject(hPen);
            EndPaint(hwnd, &ps);
        }
        break;
    case WM_KEYDOWN: 
        if (wParam == 'S') {
            SaveScreenToJPG();
            MessageBox(hwnd, L"Saved trail.jpg", L"Saved", MB_OK);
        } else if (wParam == VK_ESCAPE) {
            g_trailPoints.clear(); 
            DestroyWindow(hwnd);
        }
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK LiveOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // Double buffering to eliminate flicker and stutter
            RECT rc;
            GetClientRect(hwnd, &rc);
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HGDIOBJ oldBm = SelectObject(hdcMem, hbmMem);

            Graphics graphics(hdcMem);
            graphics.Clear(Color(255, 0, 0, 0)); // Black background for transparency key
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);

            if (g_livePoints.size() > 1) {
                // Optimization: Copy checks to vector once
                std::vector<Point> pts;
                pts.reserve(g_livePoints.size());
                for (const auto& p : g_livePoints) {
                    pts.emplace_back((INT)p.x, (INT)p.y);
                }

                int totalPts = (int)pts.size();
                // Draw in "bands" of alpha to reduce Draw calls from N to ~20
                int steps = 30; 
                if (steps > totalPts) steps = totalPts / 2;
                if (steps < 1) steps = 1;
                
                int ptsPerStep = totalPts / steps;
                
                int r = GetRValue(g_penColor);
                int g = GetGValue(g_penColor);
                int b = GetBValue(g_penColor);

                for (int i = 0; i < steps; ++i) {
                    int startIdx = i * ptsPerStep;
                    // Ensure overlap to connect lines seamlessly
                    int endIdx = (i == steps - 1) ? totalPts : (startIdx + ptsPerStep + 1);
                    if (endIdx > totalPts) endIdx = totalPts;
                    
                    int count = endIdx - startIdx;
                    if (count < 2) continue;

                    // Calculate Alpha for this band
                    double ratio = (double)i / (double)steps;
                    int alpha = (int)(ratio * 255.0);
                    if (alpha < 20) alpha = 20; // Min visibility

                    Pen pen(Color(alpha, r, g, b), (REAL)g_penWidth);
                    graphics.DrawLines(&pen, &pts[startIdx], count);
                }
            }

            // Blit to screen
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
            
            SelectObject(hdcMem, oldBm);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);

            EndPaint(hwnd, &ps);
        }
        break;
    case WM_ERASEBKGND:
        return 1; 
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK GameOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Double buffer
            HDC hdcMem = CreateCompatibleDC(hdc);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBITMAP hbm = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HGDIOBJ oldBm = SelectObject(hdcMem, hbm);

            HBRUSH hBr = CreateSolidBrush(RGB(0,0,0));
            FillRect(hdcMem, &rc, hBr);
            DeleteObject(hBr);

            Graphics g(hdcMem);
            g.SetSmoothingMode(SmoothingModeAntiAlias);

            const auto& bikes = g_tronGame.GetBikes();
            for (const auto& b : bikes) {
                // Determine Color
                Color penColor;
                if (b.state == DEAD) {
                    penColor = Color(255, 60, 60, 60); // Gray for dead trails
                } else {
                    penColor = Color(255, GetRValue(b.color), GetGValue(b.color), GetBValue(b.color));
                }

                if (b.trail.size() > 1) {
                    Pen pen(penColor, 3.0f);
                    // Optimization: Draw entire trail as one polyline
                    g.DrawLines(&pen, reinterpret_cast<const Point*>(b.trail.data()), (INT)b.trail.size());
                }

                // Only draw head if alive
                if (b.state == ALIVE) {
                    SolidBrush brush(Color(255, 255, 255, 255));
                    g.FillRectangle(&brush, (INT)b.x - 3, (INT)b.y - 3, 6, 6);
                }
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

void LoadPointsFromFile() {
    g_trailPoints.clear();
    FILE* f = _wfopen(LOG_FILENAME, L"r");
    if (f != NULL) {
        int x, y;
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%d,%d", &x, &y) == 2) {
                POINT p = {x, y};
                g_trailPoints.push_back(p);
            }
        }
        fclose(f);
    }
}

void LoadSettings() {
    wchar_t path[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, path);
    wcscat(path, L"\\");
    wcscat(path, SETTINGS_FILENAME);

    g_interval = GetPrivateProfileInt(L"Settings", L"Interval", 50, path);
    g_penWidth = GetPrivateProfileInt(L"Settings", L"PenWidth", 2, path);
    int r = GetPrivateProfileInt(L"Settings", L"ColorR", 0, path);
    int g = GetPrivateProfileInt(L"Settings", L"ColorG", 255, path);
    int b = GetPrivateProfileInt(L"Settings", L"ColorB", 0, path);
    g_penColor = RGB(r, g, b);
    g_autoClear = GetPrivateProfileInt(L"Settings", L"AutoClear", 1, path);
    g_trailLength = GetPrivateProfileInt(L"Settings", L"TrailLength", 20, path);

    g_showLive = GetPrivateProfileInt(L"Settings", L"ShowLiveTrail", 0, path);
    g_showResult = GetPrivateProfileInt(L"Settings", L"ShowResultTrail", 1, path);
    g_autoSave = GetPrivateProfileInt(L"Settings", L"AutoSave", 0, path);

    g_tronAiCount = GetPrivateProfileInt(L"Settings", L"TronAICount", 3, path);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;          
    UINT  size = 0;         
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;
    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j; 
        }
    }
    free(pImageCodecInfo);
    return -1;  
}

void SaveScreenToJPG() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    SelectObject(hdcMemDC, hbmScreen);
    BitBlt(hdcMemDC, 0, 0, screenW, screenH, hdcScreen, 0, 0, SRCCOPY);
    Bitmap* bmp = Bitmap::FromHBITMAP(hbmScreen, NULL);
    CLSID jpgClsid;
    if (GetEncoderClsid(L"image/jpeg", &jpgClsid) != -1) {
        bmp->Save(L"trail.jpg", &jpgClsid, NULL);
    }
    delete bmp;
    DeleteObject(hbmScreen);
    DeleteObject(hdcMemDC);
    ReleaseDC(NULL, hdcScreen);
}
