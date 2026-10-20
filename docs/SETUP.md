# Setup guide

## Running the overlay

1. Download `spotter_overlay.exe` from the [Releases](https://github.com/vijaynadimpalli/iracing-spotter-overlay/releases) page.
2. Move it to a folder you can write to, such as `Documents\iRacingSpotter`. Avoid `Program Files`, because the overlay saves `spotter_overlay.ini` next to the `.exe` and Windows may block writing there.
3. Set iRacing to run in **windowed or borderless** mode (see below).
4. Double-click `spotter_overlay.exe`. You can start it before or after iRacing. It waits for the sim and reconnects automatically if you restart iRacing.
5. On first run the HUD opens in **Move Mode** (blue frame with instructions):
   - Drag it to where you want it.
   - Double-click the left or right half to test the chime for that ear.
   - Right-click, or press `Enter` / `Esc` / `Ctrl + Shift + M`, to lock it. Your position is saved.

From then on the HUD stays invisible until there is something to show.

To move it again later, press `Ctrl + Shift + M`.

### Windows SmartScreen / antivirus warnings

The `.exe` is not code-signed, so Windows SmartScreen may say "Windows protected your PC". Click **More info → Run anyway** if you trust it. The overlay is open source, so if you prefer you can [build it yourself](#building-from-source) from the code you can read. If a release lists a SHA256 checksum, you can verify your download with:

```powershell
Get-FileHash .\spotter_overlay.exe -Algorithm SHA256
```

### Display mode

The overlay is a transparent always-on-top desktop window. Windows cannot draw a normal window on top of an **exclusive fullscreen** game, so run iRacing in **windowed** or **borderless windowed** mode. If you can't see the HUD in the game, this is the first thing to check.

### Audio

Chimes play through your Windows **default** audio output, left ear and right ear separately. Use `Ctrl + Shift + A` to mute them. There is no volume setting. Use the Windows volume mixer for the `spotter_overlay.exe` entry.

### Quitting

There is no tray icon. End `spotter_overlay.exe` in Task Manager.

## Building from source

### What you need

- Windows 10 or 11
- A C++17 compiler. The project is built and tested with **MinGW-w64 `g++`**, for example the portable [w64devkit](https://github.com/skeeto/w64devkit).
- PowerShell (included with Windows)

There are no third-party libraries. The needed pieces of the iRacing shared-memory layout are embedded in `spotter_overlay.cpp`.

### Build and test

From the repository folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

The script:

1. Finds `g++.exe`. It looks for `%TEMP%\toolchain\w64devkit\bin\g++.exe` first, then on your `PATH`.
2. Builds and runs `spotter_tests.exe`. If any test fails, it stops.
3. Builds `spotter_overlay.exe`.

Add `-RestartOverlay` to launch the new build when it finishes. If the overlay is already running it is stopped and restarted automatically.

### Manual build

```powershell
g++ -O2 -std=c++17 -mwindows -static spotter_overlay.cpp -o spotter_overlay.exe -luser32 -lgdi32 -lwinmm
```

To run only the tests:

```powershell
g++ -O2 -std=c++17 -static spotter_tests.cpp -o spotter_tests.exe -luser32 -lgdi32 -lwinmm
.\spotter_tests.exe
```

## Troubleshooting

| Problem | Try this |
| :--- | :--- |
| Nothing shows up in game | Use windowed/borderless mode. Check that you are on track and not in a replay. Press `Ctrl + Shift + M` to confirm the HUD is on-screen. |
| HUD is off-screen after changing monitors | Delete `spotter_overlay.ini` and restart the overlay. |
| No sound | Check it isn't muted (`Ctrl + Shift + A`) and that Windows' default output device is your headphones/speakers. |
| A car's pillar looks the wrong size | Edit its length in the `[CarLengths]` section of `spotter_overlay.ini`. |
| Settings aren't saved | Move the `.exe` out of a protected folder such as `Program Files`. |

Still stuck? Open an [issue](https://github.com/vijaynadimpalli/iracing-spotter-overlay/issues/new/choose) and attach your `spotter_overlay.ini`.
