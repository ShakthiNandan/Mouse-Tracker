/*
 * Mouse Tracker for Hyprland (Arch Linux)
 * 
 * Tech Stack:
 * - C++
 * - GTK3 (UI & Main Loop)
 * - GtkLayerShell (For transparent overlay on Wayland)
 * - Hyprland IPC (For fast cursor tracking without polling heavy commands)
 * 
 * Dependencies (Arch):
 * sudo pacman -S gtk3 gtk-layer-shell gcc pkgconf
 * 
 * Compile:
 * g++ -o mouse_tracker_hyprland main_hyprland.cpp $(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0)
 */

#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>
#include <deque>
#include <string>
#include <filesystem>
#include <iostream>

using namespace std;

// -- Types --
struct Point {
    int x, y;
};

// -- Globals --
GtkWidget* window_control;
GtkWidget* window_live_overlay = nullptr;
GtkWidget* window_static_trail = nullptr;
GtkWidget* btn_start;
GtkWidget* btn_stop;
GtkWidget* chk_live;

FILE* logFile = NULL;
bool isTracking = false;
bool showLive = false;

// Trail Data
std::deque<Point> livePoints;
std::vector<Point> staticPoints;

// Settings
int g_interval = 20; // 50ms default
int g_penWidth = 3;
double g_colorR = 0.0, g_colorG = 1.0, g_colorB = 1.0;
int g_trailLength = 20;
bool g_autoClear = true;

const char* LOG_FILENAME = "mouse_log.txt";
const char* SETTINGS_FILENAME = "settings.ini";

// -- Helper Functions --

// Simple INI parser
int GetIniInt(const char* section, const char* key, int defVal) {
    FILE* f = fopen(SETTINGS_FILENAME, "r");
    if (!f) return defVal;

    char line[256];
    char currentSection[64] = "";
    int val = defVal;
    bool inSection = false;

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '[') {
            sscanf(line, "[%[^]]", currentSection);
            inSection = (strcmp(currentSection, section) == 0);
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
    g_interval = GetIniInt("Settings", "Interval", 20);
    g_penWidth = GetIniInt("Settings", "PenWidth", 3);
    int r = GetIniInt("Settings", "ColorR", 0);
    int g = GetIniInt("Settings", "ColorG", 255);
    int b = GetIniInt("Settings", "ColorB", 255);
    g_colorR = r / 255.0;
    g_colorG = g / 255.0;
    g_colorB = b / 255.0;
    g_autoClear = GetIniInt("Settings", "AutoClear", 1) == 1;
    g_trailLength = GetIniInt("Settings", "TrailLength", 20);
    
    // Safety clamp interval
    if (g_interval < 5) g_interval = 5;
}

// Hyprland Socket Reader for Cursor Pos
bool GetHyprlandCursor(int& x, int& y) {
    const char* sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!sig) return false; // Not running Hyprland?

    string socket_path = "/tmp/hypr/" + string(sig) + "/.socket.sock";

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path)-1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return false;
    }

    // Send command
    const char* cmd = "cursorpos";
    write(sock, cmd, strlen(cmd));

    // Read response
    char buffer[128];
    int len = read(sock, buffer, sizeof(buffer)-1);
    close(sock);

    if (len > 0) {
        buffer[len] = 0;
        // Format is "123, 456"
        if (sscanf(buffer,("%d, %d"), &x, &y) == 2) {
            return true;
        }
    }
    return false;
}

// -- Draw Callbacks --

static gboolean on_draw_live_overlay(GtkWidget *widget, cairo_t *cr, gpointer data) {
    if (livePoints.size() < 2) return FALSE;

    cairo_set_source_rgba(cr, 0, 0, 0, 0); // Transparent background
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    cairo_set_line_width(cr, g_penWidth);
    
    // Draw fading trail
    for (size_t i = 0; i < livePoints.size() - 1; ++i) {
        double alpha = (double)i / (double)livePoints.size();
        // Min visibility check
        if (alpha < 0.1) alpha = 0.1;

        cairo_set_source_rgba(cr, g_colorR, g_colorG, g_colorB, alpha);
        cairo_move_to(cr, livePoints[i].x, livePoints[i].y);
        cairo_line_to(cr, livePoints[i+1].x, livePoints[i+1].y);
        cairo_stroke(cr);
    }

    return FALSE;
}

static gboolean on_draw_static_trail(GtkWidget *widget, cairo_t *cr, gpointer data) {
    // Semi-transparent black background
    cairo_set_source_rgba(cr, 0, 0, 0, 0.7); 
    cairo_paint(cr);

    if (staticPoints.size() > 1) {
        cairo_set_source_rgb(cr, g_colorR, g_colorG, g_colorB);
        cairo_set_line_width(cr, g_penWidth);
        
        cairo_move_to(cr, staticPoints[0].x, staticPoints[0].y);
        for (size_t i = 1; i < staticPoints.size(); ++i) {
             cairo_line_to(cr, staticPoints[i].x, staticPoints[i].y);
        }
        cairo_stroke(cr);
    }
    
    // Instructions
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 20);
    cairo_move_to(cr, 50, 50);
    cairo_show_text(cr, "Press ESC to Close | Press S to Save Screenshot");

    return FALSE;
}

// -- Logic --

gboolean timer_tick(gpointer data) {
    if (!isTracking) return FALSE; // stop timer

    int x, y;
    if (GetHyprlandCursor(x, y)) {
        // Log
        if (logFile) {
            fprintf(logFile, "%d,%d\n", x, y);
            fflush(logFile);
        }

        // Live Trail
        if (showLive && window_live_overlay) {
            Point p = {x, y};
            livePoints.push_back(p);
            if (livePoints.size() > (size_t)g_trailLength) livePoints.pop_front();
            gtk_widget_queue_draw(window_live_overlay);
        }
    }

    return TRUE; // continue timer
}

// -- Actions --

void start_tracking() {
    LoadSettings(); // Reload in case it changed

    const char* mode = g_autoClear ? "w" : "a";
    logFile = fopen(LOG_FILENAME, mode);
    
    if (logFile) {
        isTracking = true;
        livePoints.clear();
        
        gtk_widget_set_sensitive(btn_start, FALSE);
        gtk_widget_set_sensitive(btn_stop, TRUE);
        
        showLive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_live));

        if (showLive) {
            // Create Overlay
            window_live_overlay = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            
            // Layer Shell Setup
            gtk_layer_init_for_window(GTK_WINDOW(window_live_overlay));
            gtk_layer_set_layer(GTK_WINDOW(window_live_overlay), GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(window_live_overlay));
            gtk_layer_set_anchor(GTK_WINDOW(window_live_overlay), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(window_live_overlay), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(window_live_overlay), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(window_live_overlay), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            
            // Make input pass-through (transparent for clicks)
            GdkDisplay* display = gdk_display_get_default();
            // In GTK3 layer shell, we generally rely on not setting keyboard interactivity
            // and relying on proper mapping. 
            // Setting the app as "Overlay" usually passes clicks through transparent areas in Hyprland 
            // IF we ensure the window has no input region. 
            // (Strictly implementing input_region in GTK3 is verbose, relying on Hyprland default behavior)
            
            // Transparent Visual
            GdkScreen *screen = gtk_widget_get_screen(window_live_overlay);
            GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
            gtk_widget_set_visual(window_live_overlay, visual);
            
            // Draw signal
            g_signal_connect(G_OBJECT(window_live_overlay), "draw", G_CALLBACK(on_draw_live_overlay), NULL);
            gtk_widget_show_all(window_live_overlay);
        }

        // Start Timer
        g_timeout_add(g_interval, timer_tick, NULL);
    }
}

static gboolean on_key_press_static(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_destroy(widget);
        window_static_trail = nullptr;
        return TRUE;
    }
    if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
        // Screenshot logic (using grim because native screenshotting on Wayland is restricted)
        system("grim trail_screenshot.png");
        // Show dialog
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widget),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_OK,
                                     "Saved trail_screenshot.png using 'grim'");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return TRUE;
    }
    return FALSE;
}

void stop_tracking() {
    isTracking = false;
    if (logFile) {
        fclose(logFile);
        logFile = NULL;
    }
    
    if (window_live_overlay) {
        gtk_widget_destroy(window_live_overlay);
        window_live_overlay = nullptr;
    }

    gtk_widget_set_sensitive(btn_start, TRUE);
    gtk_widget_set_sensitive(btn_stop, FALSE);

    // Load Points & Show Review
    staticPoints.clear();
    FILE* f = fopen(LOG_FILENAME, "r");
    if (f) {
        char line[128];
        int x, y;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%d,%d", &x, &y) == 2) {
                staticPoints.push_back({x, y});
            }
        }
        fclose(f);
    }

    if (!staticPoints.empty()) {
        window_static_trail = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        
        // Use Layer Shell "Top" mode to capture input
        gtk_layer_init_for_window(GTK_WINDOW(window_static_trail));
        gtk_layer_set_layer(GTK_WINDOW(window_static_trail), GTK_LAYER_SHELL_LAYER_OVERLAY); // Or TOP
        gtk_layer_set_anchor(GTK_WINDOW(window_static_trail), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(window_static_trail), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(window_static_trail), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(window_static_trail), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        
        // Exclusive (Keyboard)
        gtk_layer_set_keyboard_mode(GTK_WINDOW(window_static_trail), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);

        // Transparent visual (glassy effect)
        GdkScreen *screen = gtk_widget_get_screen(window_static_trail);
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        gtk_widget_set_visual(window_static_trail, visual);

        g_signal_connect(G_OBJECT(window_static_trail), "draw", G_CALLBACK(on_draw_static_trail), NULL);
        g_signal_connect(G_OBJECT(window_static_trail), "key-press-event", G_CALLBACK(on_key_press_static), NULL);
        
        gtk_widget_show_all(window_static_trail);
        // Grab focus
        gtk_widget_grab_focus(window_static_trail);
    }
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    
    // Check Hyprland
    if (!getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        printf("WARNING: HYPRLAND_INSTANCE_SIGNATURE not found. Tracking might fail.\n");
    }

    LoadSettings();

    // Create Main Window
    window_control = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_control), "Tracker");
    gtk_window_set_default_size(GTK_WINDOW(window_control), 250, 150);
    g_signal_connect(window_control, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Layout
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_container_add(GTK_CONTAINER(window_control), box);

    // Buttons
    btn_start = gtk_button_new_with_label("START");
    g_signal_connect(btn_start, "clicked", G_CALLBACK(start_tracking), NULL);
    gtk_box_pack_start(GTK_BOX(box), btn_start, TRUE, TRUE, 0);

    btn_stop = gtk_button_new_with_label("STOP");
    gtk_widget_set_sensitive(btn_stop, FALSE);
    g_signal_connect(btn_stop, "clicked", G_CALLBACK(stop_tracking), NULL);
    gtk_box_pack_start(GTK_BOX(box), btn_stop, TRUE, TRUE, 0);

    // Checkbox
    chk_live = gtk_check_button_new_with_label("Show Live Trail");
    gtk_box_pack_start(GTK_BOX(box), chk_live, FALSE, FALSE, 5);

    gtk_widget_show_all(window_control);
    gtk_main();

    return 0;
}
