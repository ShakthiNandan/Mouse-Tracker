#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <stdio.h>
#include <vector>
#include <deque>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment (lib,"gdiplus.lib")

// Global Variables
BOOL isTracking = FALSE;
FILE* logFile = NULL;
const wchar_t LOG_FILENAME[] = L"mouse_log.txt";
const wchar_t CONTROL_CLASS_NAME[] = L"ControlWindowClass";
const wchar_t TRAIL_CLASS_NAME[] = L"TrailWindowClass";
const wchar_t LIVE_OVERLAY_CLASS_NAME[] = L"LiveOverlayClass";
const wchar_t SETTINGS_FILENAME[] = L"settings.ini";
std::vector<POINT> g_trailPoints;
std::deque<POINT> g_livePoints;
ULONG_PTR gdiplusToken;

// Settings
int g_interval = 50;      // ms
int g_penWidth = 2;
COLORREF g_penColor = RGB(0, 255, 0); // Green
BOOL g_autoClear = TRUE; 
int g_trailLength = 20;

// UI Handles
HWND hStartBtn, hStopBtn, hLiveCheck, hAutoSaveCheck;
HWND hLiveOverlay = NULL;

// Forward declarations
LRESULT CALLBACK ControlProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TrailProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LiveOverlayProc(HWND, UINT, WPARAM, LPARAM);
void LoadPointsFromFile();
void LoadSettings();
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);
void SaveScreenToJPG();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Register Control Window Class
    WNDCLASS wcControl = {0};
    wcControl.lpfnWndProc = ControlProc;
    wcControl.hInstance = hInstance;
    wcControl.lpszClassName = CONTROL_CLASS_NAME;
    wcControl.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcControl.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcControl);

    // Register Trail Window Class
    WNDCLASS wcTrail = {0};
    wcTrail.lpfnWndProc = TrailProc;
    wcTrail.hInstance = hInstance;
    wcTrail.lpszClassName = TRAIL_CLASS_NAME;
    wcTrail.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); 
    wcTrail.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcTrail);

    // Register Live Overlay Class
    WNDCLASS wcLive = {0};
    wcLive.lpfnWndProc = LiveOverlayProc;
    wcLive.hInstance = hInstance;
    wcLive.lpszClassName = LIVE_OVERLAY_CLASS_NAME;
    wcLive.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcLive.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wcLive);

    // Load Settings
    LoadSettings();

    // Create Control Window (Tiny)
    wchar_t title[64];
    wsprintf(title, L"Tracker (Interval: %dms)", g_interval);
    HWND hwnd = CreateWindowEx(
        0, CONTROL_CLASS_NAME, title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 250, 160, 
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Message Loop
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
        hStartBtn = CreateWindow(
            L"BUTTON", L"START",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 10, 100, 60,
            hwnd, (HMENU)1,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );

        hStopBtn = CreateWindow(
            L"BUTTON", L"STOP",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD,
            120, 10, 100, 60,
            hwnd, (HMENU)2,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );
        EnableWindow(hStopBtn, FALSE);

        hLiveCheck = CreateWindow(
            L"BUTTON", L"Show Live Fading Trail",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            15, 80, 200, 30,
            hwnd, (HMENU)3,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );

        // Checkbox for Auto Save
        hAutoSaveCheck = CreateWindow(
            L"BUTTON", L"Auto-Save on Stop",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            15, 110, 200, 30,
            hwnd, (HMENU)4,
            ((LPCREATESTRUCT)lParam)->hInstance, NULL
        );
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // START
            const wchar_t* mode = g_autoClear ? L"w" : L"a";
            logFile = _wfopen(LOG_FILENAME, mode);
            if (logFile != NULL) {
                isTracking = TRUE;
                
                // Clear live points
                g_livePoints.clear();

                // Check if Live Trail is enabled
                if (SendMessage(hLiveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    hLiveOverlay = CreateWindowEx(
                        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
                        LIVE_OVERLAY_CLASS_NAME, L"LiveTrail",
                        WS_POPUP | WS_VISIBLE | WS_MAXIMIZE,
                        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                        NULL, NULL, GetModuleHandle(NULL), NULL
                    );
                    SetLayeredWindowAttributes(hLiveOverlay, RGB(0,0,0), 0, LWA_COLORKEY);
                }

                SetTimer(hwnd, 1, g_interval, NULL); 
                EnableWindow(hStartBtn, FALSE);
                EnableWindow(hLiveCheck, FALSE); // Lock checkboxes
                EnableWindow(hAutoSaveCheck, FALSE);
                EnableWindow(hStopBtn, TRUE);
            } else {
                MessageBox(hwnd, L"Failed to open log file.", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        else if (LOWORD(wParam) == 2) { // STOP
            KillTimer(hwnd, 1);
            isTracking = FALSE;
            
            if (hLiveOverlay) {
                DestroyWindow(hLiveOverlay);
                hLiveOverlay = NULL;
            }

            if (logFile) {
                fclose(logFile);
                logFile = NULL;
            }
            EnableWindow(hStartBtn, TRUE);
            EnableWindow(hLiveCheck, TRUE);
            EnableWindow(hAutoSaveCheck, TRUE);
            EnableWindow(hStopBtn, FALSE);

            // Generate Trail for Auto-Save or Display
            LoadPointsFromFile();
            
            // Auto Save Logic
            if (SendMessage(hAutoSaveCheck, BM_GETCHECK, 0, 0) == BST_CHECKED && !g_trailPoints.empty()) {
                // To save the trail, we need to draw it first.
                // We'll quickly open the trail window, draw, save, and then keep it open (or close it? usually keep open to show result)
                // Actually, SaveScreenToJPG captures the *screen*.
                // So we must show the trail window first.
                
                HWND hTrail = CreateWindowEx(
                    WS_EX_TOPMOST | WS_EX_LAYERED, TRAIL_CLASS_NAME, L"Mouse Trail",
                    WS_POPUP | WS_VISIBLE | WS_MAXIMIZE,
                    0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                    NULL, NULL, GetModuleHandle(NULL), NULL
                );
                SetLayeredWindowAttributes(hTrail, RGB(0,0,0), 0, LWA_COLORKEY);
                ShowWindow(hTrail, SW_SHOWMAXIMIZED);
                UpdateWindow(hTrail); // Force paint immediately
                
                // Small delay to ensure paint? (Win32 paint is synchronous via UpdateWindow but DWM composition might lag)
                Sleep(100); 
                
                SaveScreenToJPG();
                // We keep the window open so user can see it too.
                // Just notify
                MessageBox(hwnd, L"Auto-saved trail.jpg!", L"Saved", MB_OK);
                return 0; // Window is already created, so we skip the normal creation block
            }

            if (!g_trailPoints.empty()) {
                // Open Trail Window (Fullscreen)
                HWND hTrail = CreateWindowEx(
                    WS_EX_TOPMOST | WS_EX_LAYERED, TRAIL_CLASS_NAME, L"Mouse Trail",
                    WS_POPUP | WS_VISIBLE | WS_MAXIMIZE,
                    0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                    NULL, NULL, GetModuleHandle(NULL), NULL
                );
                // Make black transparent
                SetLayeredWindowAttributes(hTrail, RGB(0,0,0), 0, LWA_COLORKEY);
                ShowWindow(hTrail, SW_SHOWMAXIMIZED);
            } else {
                MessageBox(hwnd, L"No points to draw.", L"Info", MB_OK);
            }
        }
        break;

    case WM_TIMER:
        if (isTracking && logFile) {
            POINT p;
            if (GetCursorPos(&p)) {
                // Log to file
                fprintf(logFile, "%d,%d\n", p.x, p.y);
                fflush(logFile); 

                // Update Live Trail
                if (hLiveOverlay) {
                    g_livePoints.push_back(p);
                    if (g_livePoints.size() > (size_t)g_trailLength) {
                        g_livePoints.pop_front();
                    }
                    // Force repaint of overlay
                    InvalidateRect(hLiveOverlay, NULL, TRUE);
                }
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

// Result Review Window
LRESULT CALLBACK TrailProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Draw Full Static Trail
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
    case WM_LBUTTONDOWN: 
        // Optional click to close logic
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Live Fading Overlay Window
LRESULT CALLBACK LiveOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            Graphics graphics(hdc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);

            if (g_livePoints.size() > 1) {
                int r = GetRValue(g_penColor);
                int g = GetGValue(g_penColor);
                int b = GetBValue(g_penColor);

                for (size_t i = 0; i < g_livePoints.size() - 1; ++i) {
                    // Calculate opacity: older points = more transparent
                    // i=0 is oldest. i=size-1 is newest.
                    // alpha = (i / total) * 255
                    int alpha = (int)((double)i / (double)g_livePoints.size() * 255.0);
                    if (alpha < 10) alpha = 10; // Min visibility

                    Pen pen(Color(alpha, r, g, b), (REAL)g_penWidth);
                    graphics.DrawLine(&pen, (INT)g_livePoints[i].x, (INT)g_livePoints[i].y, 
                                            (INT)g_livePoints[i+1].x, (INT)g_livePoints[i+1].y);
                }
            }

            EndPaint(hwnd, &ps);
        }
        break;
    
    case WM_ERASEBKGND:
        // Return 1 to prevent flickering (we draw entire bg transparent via colorkey, 
        // but actually we rely on DefWindowProc to fill with Black brush, then ColorKey removes it)
        // To do smooth GDI+ drawing, it's better to let it draw Black first.
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

    // defaults
    g_interval = GetPrivateProfileInt(L"Settings", L"Interval", 50, path);
    g_penWidth = GetPrivateProfileInt(L"Settings", L"PenWidth", 2, path);
    int r = GetPrivateProfileInt(L"Settings", L"ColorR", 0, path);
    int g = GetPrivateProfileInt(L"Settings", L"ColorG", 255, path);
    int b = GetPrivateProfileInt(L"Settings", L"ColorB", 0, path);
    g_penColor = RGB(r, g, b);
    g_autoClear = GetPrivateProfileInt(L"Settings", L"AutoClear", 1, path);
    g_trailLength = GetPrivateProfileInt(L"Settings", L"TrailLength", 20, path);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;  // Failure

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }
    free(pImageCodecInfo);
    return -1;  // Failure
}

void SaveScreenToJPG() {
    // Capture the entire screen
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, screenW, screenH);
    
    SelectObject(hdcMemDC, hbmScreen);
    BitBlt(hdcMemDC, 0, 0, screenW, screenH, hdcScreen, 0, 0, SRCCOPY);

    // Save as JPG using GDI+
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
