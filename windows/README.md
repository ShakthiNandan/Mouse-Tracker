# Ultra-Light Mouse Tracker (C++ Win32)

A minimal, high-performance mouse tracking application written in C++ using the Win32 API.

## 🚀 Features
- **Extremely Low RAM**: Uses ~1-2 MB memory.
- **Minimal UI**: Tiny window with START / STOP controls.
- **Efficient Storage**: Logs cursor coordinates to a CSV file.
- **Visual Trail**: Draws a path connecting all recorded points when stopped.
- **Configurable**: Customize interval, color, and thickness via `settings.ini`.
- **Save to Image**: Press **'S'** in the trail view to save `trail.bmp`.

## 🛠 Building the App

You can compile this using **Visual Studio (MSVC)** or **MinGW (GCC)**.

### Option 1: Visual Studio (Recommended)
1. Open the **Developer Command Prompt for VS**.
2. Navigate to this directory.
3. Run:
   ```cmd
   build_msvc.bat
   ```

### Option 2: MinGW / GCC
1. Ensure `g++` is in your PATH.
2. Run:
   ```cmd
   build_mingw.bat
   ```

## 🎮 How to Use
1. Run `MouseTracker.exe`.
2. Click **START** to begin logging cursor positions.
3. Click **STOP** to end logging and visualize the mouse trail.
4. **Trail View Controls**:
   - **ESC**: Close trail.
   - **S**: Save the current trail as `trail.bmp`.

## ⚙️ Configuration (settings.ini)
Edit `settings.ini` to change:
- `Interval`: Tracking speed in ms (default 250).
- `AutoClear`: 1 to start fresh every time, 0 to keep history.
- `PenWidth`: Thickness of the line.
- `ColorR/G/B`: RGB color values for the trail.
