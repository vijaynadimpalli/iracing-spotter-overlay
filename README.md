# iRacing Multi-Class Spotter & Bonnet Proximity Overlay

[![CI](https://github.com/vijaynadimpalli/iracing-spotter-overlay/actions/workflows/ci.yml/badge.svg)](https://github.com/vijaynadimpalli/iracing-spotter-overlay/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![Platform: Windows](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey)

A free, open-source, ultra-light on-screen spotter for [iRacing](https://www.iracing.com/). It is built around iRacing's own spotter (the left / right / "car alongside" calls) and adds multi-class awareness, a draft proximity bar, and positional stereo chimes.

<!-- TODO: add a screenshot or short GIF of the overlay in a race, e.g. docs/screenshot.png -->

> **You should already be familiar with how the iRacing spotter works.** This overlay shows the same information visually, so if you know what "car left" / "car right" / "three wide" mean in iRacing, you already know how to read it. Details are in [How to read the HUD](#how-to-read-the-hud).

## Features

1. **Multi-class side pillars.** The left and right pillars show *where along your car* an opponent is. The top of the pillar is your front bumper and the bottom is your rear bumper. Opponent lengths come from a built-in table of car lengths (for example `Caterham 420R = 3.35 m`, GT3 about `4.5–5.0 m`, GTP/LMDh `5.10 m`), so a long prototype alongside a short Caterham is drawn correctly.
2. **Front bonnet / draft proximity bar.** A horizontal bar at the bottom of the HUD tracks the nose-to-tail gap to the car ahead (`3.50 m → 0.00 m`) while you draft, and warns you when to pull out of the slipstream. It hands off to the side pillars once you are alongside.
3. **Hard-panned stereo chimes.** A short two-tone chime plays in the **left** or **right** ear when a car appears on that side. The sounds are synthesized in memory, so there are no audio files to install.
4. **Lightweight native rendering.** A plain Win32 window with per-pixel alpha (`UpdateLayeredWindow`). There is no DirectX or GPU swapchain, it is click-through while locked, and it never takes focus from iRacing.

The overlay only **reads** iRacing's public shared-memory telemetry. It does not inject into or modify the game.

## Requirements

- Windows 10 or 11
- iRacing running in **windowed / borderless** mode (see the [FAQ](docs/FAQ.md#i-cant-see-the-overlay-in-the-game))
- Nothing else. It is a single `.exe` with no installer and no admin rights.

## Quick start

1. Download `spotter_overlay.exe` from the [Releases](https://github.com/vijaynadimpalli/iracing-spotter-overlay/releases) page, or [build it yourself](docs/SETUP.md#building-from-source).
2. Put it in a folder you can write to (for example `Documents\iRacingSpotter`). It saves its settings next to the `.exe`.
3. Run it. On the **first run** the HUD starts in *Move Mode* (blue frame). Drag it where you want it.
4. Right-click, press `Enter`/`Esc`, or press `Ctrl + Shift + M` to lock it. Your position is saved.
5. Start iRacing and drive. The HUD appears when a car is alongside or you are drafting behind one.

More detail, including troubleshooting, is in [docs/SETUP.md](docs/SETUP.md).

## How to read the HUD

**Side pillars (left and right)**

- The pillar is a map of *your car*: top = front bumper, bottom = rear bumper.
- The **orange** block is the part of your car that an opponent currently overlaps.
- A short **red** cap means a car is near but not yet alongside: a cap at the **top** is a car just ahead of you, a cap at the **bottom** is a car just behind.
- Two cars on the same side are merged into one block that spans both of them.
- The pillars follow iRacing's own spotter state, so they appear when iRacing's spotter says there is a car there.

**Front bonnet bar (bottom, between the pillars)**

The bar fills outward from the centre as you close in on the car ahead:

| Colour | Gap to car ahead | Meaning |
| :--- | :--- | :--- |
| Teal / green | 3.50 m – 1.20 m | In the slipstream and closing |
| Amber | 1.20 m – 0.35 m | Pull out now |
| Red | under 0.35 m | Contact imminent |

The two small white ticks mark the 1.20 m "pull out" threshold. The bar disappears as soon as a side pillar is active.

**Chimes**

| Sound | Meaning |
| :--- | :--- |
| Chime in your left ear | A car just appeared on your left |
| Chime in your right ear | A car just appeared on your right |

**When it is hidden:** in replays, when you are not on track, in the garage, or when no car is alongside or directly ahead within 3.5 m.

## Controls & hotkeys

| Hotkey / Action | Function |
| :--- | :--- |
| `Ctrl + Shift + M` | Unlock / lock the HUD (Move Mode) |
| Left-click + drag *(while unlocked)* | Move the HUD anywhere on screen |
| Double-click left / right half *(while unlocked)* | Test the left-ear or right-ear chime |
| Right-click / `Enter` / `Esc` *(while unlocked)* | Lock the HUD and save its position |
| `Ctrl + Shift + A` | Mute / unmute the chimes (remembered between runs) |

There is no tray icon or close button. To quit, end `spotter_overlay.exe` in Task Manager.

## Configuration

`spotter_overlay.ini` is created next to `spotter_overlay.exe`.

```ini
[Overlay]
PosX=409
PosY=142
Muted=0

[CarLengths]
caterham caterham420r=3.35
```

- `PosX` / `PosY`: HUD position in screen pixels. Delete the file to reset to the screen centre and re-enter Move Mode.
- `Muted`: `1` mutes the chimes.
- `[CarLengths]`: every car you drive or race against is recorded here, keyed by iRacing's `CarPath`, with the length in **metres**. Edit a value to correct a car. Values between 2.0 and 8.0 are accepted and take effect the next time a session loads.

## Supported cars

The overlay ships with a length table covering most popular iRacing cars across sports cars, GT3/GT4/GTE, prototypes, open wheel, stock cars and more. Cars it does not recognise use a class-based guess and then a default of 4.70 m. A wrong length only makes the pillar fill slightly off, and you can fix any car in the `.ini` or [add it to the table](CONTRIBUTING.md#adding-or-fixing-a-car-length).

## Building & tests

```powershell
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

This compiles and runs the unit and integration test suite (`spotter_tests.cpp`), and only builds `spotter_overlay.exe` if the tests pass. See [docs/SETUP.md](docs/SETUP.md#building-from-source) for toolchain details.

## Contributing

Contributions are very welcome: bug reports, car lengths, ideas and code. Start with [CONTRIBUTING.md](CONTRIBUTING.md). Questions and "does it work with X?" go in [Discussions](https://github.com/vijaynadimpalli/iracing-spotter-overlay/discussions) or the [FAQ](docs/FAQ.md).

## Disclaimer

This is an unofficial, community project. It is not affiliated with or endorsed by iRacing.com Motorsport Simulations. iRacing is a trademark of its respective owner.

## License

[MIT](LICENSE)
