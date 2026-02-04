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
const wchar_t SETTINGS_FILENAME[] = L"settings.ini";

std::vector<POINT> g_trailPoints;
ULONG_PTR gdiplusToken;

// Settings
int g_interval = 50;      
int g_penWidth = 2;
COLORREF g_penColor = RGB(0, 255, 0); 
BOOL g_autoClear = TRUE; 
int g_trailLength = 20;

// Initial Interface States
BOOL g_autoSave = FALSE;

// UI Handles
HWND hStartBtn, hStopBtn, hAutoSaveCheck;

// Forward declarations
LRESULT CALLBACK ControlProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK TrailProc(HWND, UINT, WPARAM, LPARAM);
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

    LoadSettings();

    wchar_t title[64];
    wsprintf(title, L"Tracker (Interval: %dms)", g_interval);
    HWND hwnd = CreateWindowEx(0, CONTROL_CLASS_NAME, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 250, 160, NULL, NULL, hInstance, NULL);

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

        hAutoSaveCheck = CreateWindow(L"BUTTON", L"Auto-Save on Stop", WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX, 15, 80, 200, 30, hwnd, (HMENU)5, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        if (g_autoSave) SendMessage(hAutoSaveCheck, BM_SETCHECK, BST_CHECKED, 0);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // START
            const wchar_t* mode = g_autoClear ? L"w" : L"a";
            logFile = _wfopen(LOG_FILENAME, mode);
            if (logFile != NULL) {
                isTracking = TRUE;
                SetTimer(hwnd, 1, g_interval, NULL); 
                EnableWindow(hStartBtn, FALSE);
                EnableWindow(hAutoSaveCheck, FALSE);
                EnableWindow(hStopBtn, TRUE);
            } else {
                MessageBox(hwnd, L"Failed to open log file.", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        else if (LOWORD(wParam) == 2) { // STOP
            KillTimer(hwnd, 1);
            isTracking = FALSE;
            
            if (logFile) { fclose(logFile); logFile = NULL; }

            EnableWindow(hStartBtn, TRUE);
            EnableWindow(hAutoSaveCheck, TRUE);
            EnableWindow(hStopBtn, FALSE);

            LoadPointsFromFile();
            
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
                // ALWAYS SHOW RESULT
                HWND hTrail = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, TRAIL_CLASS_NAME, L"Mouse Trail", WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, GetModuleHandle(NULL), NULL);
                SetLayeredWindowAttributes(hTrail, RGB(0,0,0), 0, LWA_COLORKEY);
                ShowWindow(hTrail, SW_SHOWMAXIMIZED);
            } else {
                MessageBox(hwnd, L"No points to draw.", L"Info", MB_OK);
            }
        }
        break;

    case WM_TIMER:
        if (wParam == 1 && isTracking && logFile) {
            POINT p;
            if (GetCursorPos(&p)) {
                fprintf(logFile, "%d,%d\n", p.x, p.y);
                fflush(logFile); 
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
    g_autoSave = GetPrivateProfileInt(L"Settings", L"AutoSave", 0, path);
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
