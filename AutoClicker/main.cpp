#ifndef UNICODE
#define UNICODE
#endif

#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

// Link against common controls for modern styling (if manifest is present) or just standard controls
#pragma comment(lib, "comctl32.lib")

// Global variables
const wchar_t CLASS_NAME[] = L"AutoClickerClass";
const wchar_t APP_TITLE[] = L"Auto Clicker - Monitor";

enum EventType {
    EVENT_MOVE = 0,
    EVENT_L_DOWN,
    EVENT_L_UP,
    EVENT_R_DOWN,
    EVENT_R_UP
};

struct MouseEvent {
    long long timeOffset; // ms from start
    EventType type;
    int x;
    int y;
};

std::vector<MouseEvent> g_recording;
BOOL g_isRecording = FALSE;
BOOL g_isPlaying = FALSE;
long long g_startTime = 0;

// UI Handles
HWND hRecordBtn, hStopBtn, hPlayBtn, hDelayEdit, hStatusLabel;
HWND hMainWindow;

// State tracking for recording
POINT g_lastPos = { -1, -1 };
BOOL g_lastLDown = FALSE;
BOOL g_lastRDown = FALSE;

// Settings
int g_playDelayMs = 2000;

// Helper to get time in ms
long long GetTimeMs() {
    static LARGE_INTEGER s_frequency;
    static BOOL s_useQPC = QueryPerformanceFrequency(&s_frequency);
    if (s_useQPC) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (1000LL * now.QuadPart) / s_frequency.QuadPart;
    } else {
        return (long long)GetTickCount();
    }
}

// Forward declarations
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
void SaveRecording(const wchar_t* filename);
void LoadRecording(const wchar_t* filename);
DWORD WINAPI PlaybackThread(LPVOID lpParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    // Basic styling
    InitCommonControls(); // Ensure controls are registered

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, APP_TITLE,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 350, 250,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);

    // Initial load if exists
    LoadRecording(L"recording.dat");

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void StartRecording() {
    g_recording.clear();
    g_isRecording = TRUE;
    g_startTime = GetTimeMs();

    // Reset loop state
    POINT p;
    GetCursorPos(&p);
    g_lastPos = p;
    g_lastLDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    g_lastRDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    SetTimer(hMainWindow, 1, 10, NULL); // Check every 10ms
    SetWindowText(hStatusLabel, L"Status: Recording...");
    EnableWindow(hRecordBtn, FALSE);
    EnableWindow(hPlayBtn, FALSE);
    EnableWindow(hStopBtn, TRUE);
}

void Stop() {
    if (g_isRecording) {
        KillTimer(hMainWindow, 1);
        g_isRecording = FALSE;
        SetWindowText(hStatusLabel, L"Status: Recording Stopped. Saved.");
        SaveRecording(L"recording.dat");
    }
    // If playing, the thread handles the stop flag check usually, but we can't easily kill thread safely without checking.
    // For simplicity, we just set flag. Thread checks this global.
    g_isPlaying = FALSE; 

    EnableWindow(hRecordBtn, TRUE);
    EnableWindow(hPlayBtn, TRUE);
    EnableWindow(hStopBtn, FALSE);
}

void StartPlayback() {
    if (g_recording.empty()) {
        MessageBox(hMainWindow, L"No recording found to play.", L"Info", MB_OK);
        return;
    }

    wchar_t delayText[16];
    GetWindowText(hDelayEdit, delayText, 16);
    g_playDelayMs = _wtoi(delayText);

    g_isPlaying = TRUE;
    CreateThread(NULL, 0, PlaybackThread, NULL, 0, NULL);
    
    EnableWindow(hRecordBtn, FALSE);
    EnableWindow(hPlayBtn, FALSE);
    EnableWindow(hStopBtn, TRUE);
    
    wchar_t msg[64];
    wsprintf(msg, L"Status: Waiting %dms...", g_playDelayMs);
    SetWindowText(hStatusLabel, msg);
}

void RecordStep() {
    long long now = GetTimeMs();
    long long delta = now - g_startTime;

    // Check Position
    POINT p;
    if (GetCursorPos(&p)) {
        if (p.x != g_lastPos.x || p.y != g_lastPos.y) {
            g_recording.push_back({delta, EVENT_MOVE, p.x, p.y});
            g_lastPos = p;
        }
    }

    // Check Buttons
    // GetAsyncKeyState returns short. MSB matches current state.
    BOOL lDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    BOOL rDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

    if (lDown != g_lastLDown) {
        g_recording.push_back({delta, lDown ? EVENT_L_DOWN : EVENT_L_UP, p.x, p.y});
        g_lastLDown = lDown;
    }
    if (rDown != g_lastRDown) {
        g_recording.push_back({delta, rDown ? EVENT_R_DOWN : EVENT_R_UP, p.x, p.y});
        g_lastRDown = rDown;
    }
}

DWORD WINAPI PlaybackThread(LPVOID lpParam) {
    if (g_playDelayMs > 0) {
        Sleep(g_playDelayMs);
    }

    if (!g_isPlaying) return 0; // Aborted during wait

    // Update UI from thread (careful, but SetWindowText is usually thread safe-ish for sending messages)
    // Better to PostMessage, but simple approach here:
    PostMessage(hMainWindow, WM_USER + 1, 0, 0); // Usage: User message to update status

    long long startPlayTime = GetTimeMs();

    // Use SendInput for better compatibility
    INPUT input = {0};
    input.type = INPUT_MOUSE;

    for (size_t i = 0; i < g_recording.size(); i++) {
        if (!g_isPlaying) break;

        const auto& evt = g_recording[i];
        
        long long currentTime = GetTimeMs();
        long long targetTime = startPlayTime + evt.timeOffset;
        
        if (targetTime > currentTime) {
            long long sleepTime = targetTime - currentTime;
            if (sleepTime > 0) Sleep((DWORD)sleepTime);
        }

        // Re-check stop
        if (!g_isPlaying) break;

        // Perform Action
        ZeroMemory(&input, sizeof(INPUT));
        input.type = INPUT_MOUSE;
        
        // Coordinates for SendInput are normalized (0-65535) if MOUSEEVENTF_ABSOLUTE
        // Or we can just use SetCursorPos + Click? 
        // Best approach for exact replica: SetCursorPos for move, mouse_event/SendInput for clicks.
        // SendInput with MOUSEEVENTF_ABSOLUTE is robust.
        // Screen resolution
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        // Convert to absolute (0-65535)
        double absX = ((double)evt.x * 65535.0) / (screenW - 1);
        double absY = ((double)evt.y * 65535.0) / (screenH - 1);
        
        input.mi.dx = (LONG)absX;
        input.mi.dy = (LONG)absY;
        input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE; // Always move to position

        if (evt.type == EVENT_MOVE) {
            // Just move
        } else if (evt.type == EVENT_L_DOWN) {
            input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        } else if (evt.type == EVENT_L_UP) {
            input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        } else if (evt.type == EVENT_R_DOWN) {
            input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        } else if (evt.type == EVENT_R_UP) {
            input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
        }

        SendInput(1, &input, sizeof(INPUT));
    }

    g_isPlaying = FALSE;
    PostMessage(hMainWindow, WM_USER + 2, 0, 0); // Playback finished
    return 0;
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        hMainWindow = hwnd;
        hRecordBtn = CreateWindow(L"BUTTON", L"RECORD", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 
            20, 20, 140, 50, hwnd, (HMENU)1, NULL, NULL);
        
        hPlayBtn = CreateWindow(L"BUTTON", L"PLAY", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 
            170, 20, 140, 50, hwnd, (HMENU)2, NULL, NULL);
        
        hStopBtn = CreateWindow(L"BUTTON", L"STOP", WS_TABSTOP | WS_VISIBLE | WS_CHILD, 
            95, 80, 140, 40, hwnd, (HMENU)3, NULL, NULL);

        CreateWindow(L"STATIC", L"Delay (ms):", WS_VISIBLE | WS_CHILD, 
            100, 140, 80, 20, hwnd, NULL, NULL, NULL);

        hDelayEdit = CreateWindow(L"EDIT", L"2000", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_NUMBER, 
            180, 140, 60, 20, hwnd, NULL, NULL, NULL);

        hStatusLabel = CreateWindow(L"STATIC", L"Status: Ready", WS_VISIBLE | WS_CHILD | SS_CENTER, 
            20, 180, 300, 25, hwnd, NULL, NULL, NULL);

        EnableWindow(hStopBtn, FALSE);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // RECORD
            StartRecording();
        } else if (LOWORD(wParam) == 2) { // PLAY
            StartPlayback();
        } else if (LOWORD(wParam) == 3) { // STOP
            Stop();
        }
        break;

    case WM_TIMER:
        if (wParam == 1 && g_isRecording) {
            RecordStep();
        }
        break;

    case WM_USER + 1: // Playing Started (Active)
        SetWindowText(hStatusLabel, L"Status: Playing...");
        break;

    case WM_USER + 2: // Playing Finished
        SetWindowText(hStatusLabel, L"Status: Playback Finished.");
        EnableWindow(hRecordBtn, TRUE);
        EnableWindow(hPlayBtn, TRUE);
        EnableWindow(hStopBtn, FALSE);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void SaveRecording(const wchar_t* filename) {
    std::ofstream outfile("recording.dat", std::ios::binary);
    if (!outfile.is_open()) return;
    
    size_t size = g_recording.size();
    outfile.write((char*)&size, sizeof(size));
    if (size > 0) {
        outfile.write((char*)&g_recording[0], size * sizeof(MouseEvent));
    }
    outfile.close();
}

void LoadRecording(const wchar_t* filename) {
    std::ifstream infile("recording.dat", std::ios::binary);
    if (!infile.is_open()) return;

    g_recording.clear();
    size_t size = 0;
    infile.read((char*)&size, sizeof(size));
    if (size > 0) {
        g_recording.resize(size);
        infile.read((char*)&g_recording[0], size * sizeof(MouseEvent));
    }
    infile.close();

    wchar_t buf[64];
    wsprintf(buf, L"Status: Loaded %d events.", (int)size);
    SetWindowText(hStatusLabel, buf);
}
