/*
 * Mouse Tracker for Arch Linux (X11 + Cairo)
 * 
 * Dependencies: libx11, cairo
 * Compile: g++ -o mouse_tracker_linux main_linux.cpp -lX11 -lcairo -lpthread
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <deque>
#include <string>
#include <fstream>
#include <iostream>
#include <sys/time.h>

// Types
struct Point {
    int x, y;
};

// Globals
Display* dpy;
int screen;
Window winControl;
Window winOverlay; // For live trail
Window root;
bool isTracking = false;
bool showLiveTrail = false;
FILE* logFile = NULL;
std::vector<Point> trailPoints;
std::deque<Point> livePoints;

// Settings
int g_interval = 50;      // ms
int g_penWidth = 2;
double g_colorR = 0.0, g_colorG = 1.0, g_colorB = 1.0; // Cairo uses 0.0-1.0
int g_trailLength = 20;
bool g_autoClear = true;

const char* LOG_FILENAME = "mouse_log.txt";
const char* SETTINGS_FILENAME = "settings.ini";

// Helper to read INI (simplified for Linux)
int GetIniInt(const char* section, const char* key, int defVal) {
    FILE* f = fopen(SETTINGS_FILENAME, "r");
    if (!f) return defVal;

    char line[256];
    char currentSection[64] = "";
    int val = defVal;
    bool inSection = false;

    while (fgets(line, sizeof(line), f)) {
        // Trim newline
        line[strcspn(line, "\n")] = 0;
        
        if (line[0] == '[') {
            sscanf(line, "[%[^]]", currentSection);
            if (strcmp(currentSection, section) == 0) inSection = true;
            else inSection = false;
        } else if (inSection) {
            char k[64], v[64];
            if (sscanf(line, "%[^=]=%s", k, v) == 2) {
                if (strcmp(k, key) == 0) {
                    val = atoi(v);
                    break;
                }
            }
        }
    }
    fclose(f);
    return val;
}

void LoadSettings() {
    g_interval = GetIniInt("Settings", "Interval", 50);
    g_penWidth = GetIniInt("Settings", "PenWidth", 2);
    int r = GetIniInt("Settings", "ColorR", 0);
    int g = GetIniInt("Settings", "ColorG", 255);
    int b = GetIniInt("Settings", "ColorB", 0);
    g_colorR = r / 255.0;
    g_colorG = g / 255.0;
    g_colorB = b / 255.0;
    g_autoClear = GetIniInt("Settings", "AutoClear", 1) == 1;
    g_trailLength = GetIniInt("Settings", "TrailLength", 20);
}

void DrawButton(Window w, const char* label, int x, int y, int width, int height, bool active) {
    cairo_surface_t* surface = cairo_xlib_surface_create(dpy, w, DefaultVisual(dpy, screen), 250, 200);
    cairo_t* cr = cairo_create(surface);

    // Button bg
    if (active) cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    else cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_rectangle(cr, x, y, width, height);
    cairo_fill(cr);

    // Border
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, x, y, width, height);
    cairo_stroke(cr);

    // Text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 14);
    cairo_set_source_rgb(cr, 1, 1, 1);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, label, &extents);
    cairo_move_to(cr, x + (width - extents.width)/2, y + (height + extents.height)/2);
    cairo_show_text(cr, label);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
}

// Transparent overlay window creation
Window CreateOverlayWindow() {
    XVisualInfo vinfo;
    XMatchVisualInfo(dpy, DefaultScreen(dpy), 32, TrueColor, &vinfo);

    XSetWindowAttributes attrs;
    attrs.colormap = XCreateColormap(dpy, DefaultRootWindow(dpy), vinfo.visual, AllocNone);
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.override_redirect = True; // No window manager borders

    Window win = XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0, 
                  DisplayWidth(dpy, screen), DisplayHeight(dpy, screen), 0, vinfo.depth, InputOutput, 
                  vinfo.visual, CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect, &attrs);
    
    // Pass input through (click-through)
    XRectangle rect;
    XserverRegion region = XFixesCreateRegion(dpy, &rect, 1);
    XFixesSetWindowShapeRegion(dpy, win, ShapeInput, 0, 0, region);
    XFixesDestroyRegion(dpy, region);

    XMapWindow(dpy, win);
    return win;
}

void DrawOverlay() {
    if (!winOverlay) return;

    cairo_surface_t* surface = cairo_xlib_surface_create(dpy, winOverlay, 
                               DefaultVisual(dpy, screen), 
                               DisplayWidth(dpy, screen), DisplayHeight(dpy, screen));
    cairo_t* cr = cairo_create(surface);

    // Clear
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (livePoints.size() > 1) {
        cairo_set_line_width(cr, g_penWidth);
        for (size_t i = 0; i < livePoints.size() - 1; ++i) {
             double alpha = (double)i / (double)livePoints.size();
             cairo_set_source_rgba(cr, g_colorR, g_colorG, g_colorB, alpha);
             
             cairo_move_to(cr, livePoints[i].x, livePoints[i].y);
             cairo_line_to(cr, livePoints[i+1].x, livePoints[i+1].y);
             cairo_stroke(cr);
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XFlush(dpy);
}

long long GetTimeMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) return 1;
    screen = DefaultScreen(dpy);
    root = DefaultRootWindow(dpy);

    LoadSettings();

    // Create Control Window
    winControl = XCreateSimpleWindow(dpy, root, 100, 100, 250, 140, 1, 
                                     BlackPixel(dpy, screen), WhitePixel(dpy, screen));
    XSelectInput(dpy, winControl, ExposureMask | ButtonPressMask | KeyPressMask);
    XStoreName(dpy, winControl, "Mouse Tracker");
    XMapWindow(dpy, winControl);

    bool running = true;
    long long lastTick = GetTimeMs();

    while (running) {
        // Event Loop
        while (XPending(dpy) > 0) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            
            if (ev.type == Expose && ev.xany.window == winControl) {
                DrawButton(winControl, "START", 10, 10, 100, 50, isTracking);
                DrawButton(winControl, "STOP", 120, 10, 100, 50, !isTracking);
                DrawButton(winControl, showLiveTrail ? "[x] Live Trail" : "[ ] Live Trail", 10, 70, 210, 30, false);
            } else if (ev.type == ButtonPress && ev.xany.window == winControl) {
                int x = ev.xbutton.x;
                int y = ev.xbutton.y;
                
                // Start
                if (y >= 10 && y <= 60 && x >= 10 && x <= 110) {
                    if (!isTracking) {
                        const char* mode = g_autoClear ? "w" : "a";
                        logFile = fopen(LOG_FILENAME, mode);
                        if (logFile) {
                            isTracking = true;
                            livePoints.clear();
                            if (showLiveTrail) winOverlay = CreateOverlayWindow();
                        }
                    }
                }
                // Stop
                else if (y >= 10 && y <= 60 && x >= 120 && x <= 220) {
                    if (isTracking) {
                        isTracking = false;
                        if (logFile) { fclose(logFile); logFile = NULL; }
                        if (winOverlay) { XDestroyWindow(dpy, winOverlay); winOverlay = 0; }
                    }
                }
                // Checkbox
                else if (y >= 70 && y <= 100 && x >= 10 && x <= 220) {
                    showLiveTrail = !showLiveTrail;
                }
                
                // Redraw
                XClearArea(dpy, winControl, 0, 0, 0, 0, True);
            }
        }

        // Logic Loop (Timer)
        long long now = GetTimeMs();
        if (isTracking && (now - lastTick >= g_interval)) {
            lastTick = now;
            
            Window root_return, child_return;
            int root_x, root_y, win_x, win_y;
            unsigned int mask_return;
            
            if (XQueryPointer(dpy, root, &root_return, &child_return, 
                              &root_x, &root_y, &win_x, &win_y, &mask_return)) {
                
                // Log
                if (logFile) {
                    fprintf(logFile, "%d,%d\n", root_x, root_y);
                    fflush(logFile);
                }

                // Live Trail logic
                if (showLiveTrail) {
                    Point p = {root_x, root_y};
                    livePoints.push_back(p);
                    if (livePoints.size() > (size_t)g_trailLength) livePoints.pop_front();
                    DrawOverlay();
                }
            }
        }

        usleep(1000); // 1ms sleep to save CPU
    }

    XCloseDisplay(dpy);
    return 0;
}
