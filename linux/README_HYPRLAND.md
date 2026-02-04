# Mouse Tracker for Arch Linux (Hyprland)

This is a specialized version of the Mouse Tracker built specifically for **Hyprland** on Arch Linux.

It uses:
*   **GTK3 + GtkLayerShell**: For modern Wayland-native transparent overlays.
*   **Hyprland IPC**: Reads cursor position directly from the Hyprland socket for maximum performance (no process spawning).
*   **Grim**: For taking screenshots of the trail.

## 📦 Dependencies

Install the required packages on Arch:

```bash
sudo pacman -S gtk3 gtk-layer-shell gcc pkgconf grim
```

## 🛠 Compilation

Navigate to the folder containing `main_hyprland.cpp` and run:

```bash
g++ -o mouse_tracker_hyprland main_hyprland.cpp $(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0)
```

## 🚀 How to Run

```bash
./mouse_tracker_hyprland
```

## ⚙️ Configuration
The app uses the same `settings.ini` logic. Ensure `settings.ini` is in the same folder.

```ini
[Settings]
Interval=20
PenWidth=3
AutoClear=1
ColorR=0
ColorG=255
ColorB=255
TrailLength=20
```
