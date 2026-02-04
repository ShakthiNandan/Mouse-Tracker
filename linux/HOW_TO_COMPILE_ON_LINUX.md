# ⚠️ STOP: You are on Windows!

The error you are seeing is because **you are trying to compile a Linux/Hyprland application on Windows (MSYS2/MinGW).**

## 🚫 Why it failed
1.  **Hyprland** is a Linux Window Manager. It does not exist on Windows.
2.  **gtk-layer-shell** and **grim** are Wayland (Linux) tools. They do not exist on Windows.
3.  **MSYS2** is a tool to compile *Windows* apps, not Linux apps.

## ✅ How to Fix
You must move these files to your **actual Arch Linux machine** to compile them.

### Step 1: Copy Files to Linux
Copy the following files to your Arch Linux computer (via USB, Git, Google Drive, or SSH):
*   `main_hyprland.cpp`
*   `settings.ini`

### Step 2: Compile on Linux
Once you are on your Arch Linux terminal, run:

```bash
# 1. Install dependencies
sudo pacman -S base-devel gtk3 gtk-layer-shell gcc pkgconf grim

# 2. Compile
g++ -o mouse_tracker_hyprland main_hyprland.cpp $(pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0)

# 3. Run
./mouse_tracker_hyprland
```

**Do not try to run these commands in MSYS2 on Windows. They will not work.**
