# iRacing Multi-Class Spotter & Bonnet Proximity Overlay

A zero-allocation, ultra-low-latency C++17 native Windows overlay for iRacing that provides:
1. **Asymmetric Multi-Class Side-by-Side Spotter Pillars**: Accurately projects opponent front and rear bumpers onto your car's physical hull (`[0.0 = Front Bumper, 1.0 = Rear Bumper]`) using homologated chassis lengths (`Caterham 420R = 3.35m`, `GT3 = 4.52m–4.76m`, `GTP/LMDh = 5.10m`, etc.).
2. **Front Bonnet / Draft Pull-Out Proximity Bar**: Center-top horizontal bar that tracks your physical nose-to-tail bumper gap (`3.50m -> 0.00m`) while drafting and warns when to pull out of the slipstream (`Cyan/Green -> Amber -> Crimson Red`) before automatically handing off to the Left/Right pillar once alongside.
3. **100% Hard-Panned Stereo Audio Chimes**: Synthesizes two-tone harmonic chimes in memory (`880 Hz -> 1174.66 Hz` Left ear, `659.25 Hz -> 880 Hz` Right ear) via independent `HWAVEOUT` handles—no external `.wav` files required.
4. **High-DPI Native Per-Pixel Alpha Rendering**: Uses `DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` and an anti-aliased signed-distance-field (SDF) rounded-rectangle rasterizer with `UpdateLayeredWindow` (`< 0.03 ms` per frame, zero DirectX/GPU swapchain overhead, never steals focus from iRacing).

---

## Controls & Hotkeys

| Hotkey / Action | Function |
| :--- | :--- |
| **`Ctrl + Shift + M`** | Unlock / Lock HUD Move Mode |
| **Left-Click + Drag** *(while unlocked)* | Move the HUD anywhere on screen |
| **Double-Click Left / Right half** *(while unlocked)* | Test Left-ear or Right-ear stereo chime |
| **Right-Click / `Enter` / `Esc`** *(while unlocked)* | Lock HUD & save position to `spotter_overlay.ini` |
| **`Ctrl + Shift + A`** | Toggle Audio Chime Mute (persisted in `spotter_overlay.ini`) |

---

## Configuration (`spotter_overlay.ini`)

`spotter_overlay.ini` is automatically created next to `spotter_overlay.exe`. Whenever you join a session, the overlay also records every discovered `CarPath` and its resolved length (in meters) under `[CarLengths]`, allowing you to inspect or customize any car's length without recompiling:

```ini
[Overlay]
PosX=409
PosY=142
Muted=0

[CarLengths]
caterham caterham420r=3.35
```

---

## Building & Running Automated Tests

Run `build_and_test.ps1` to compile and execute the 57-assertion unit & integration test suite (`spotter_tests.cpp`) before building `spotter_overlay.exe`:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```
