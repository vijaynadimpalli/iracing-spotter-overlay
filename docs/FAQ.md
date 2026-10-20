# FAQ

## General

### What is this?
A small Windows overlay that shows iRacing's spotter information on screen. It has left and right pillars, a draft proximity bar, and left or right ear chimes. It is made for people who already know the iRacing spotter. See the [README](../README.md#how-to-read-the-hud) for how to read it.

### How is this different from iRacing's built-in spotter?
It follows the same signals, but it also:
- draws *where along your car* each opponent is, using the real length of both cars, so multi-class racing is represented properly,
- shows a front bar so you know when to pull out of a draft,
- plays a left-ear or right-ear chime for each side.

### Is it free?
Yes. It is open source under the [MIT license](../LICENSE).

### Will this get me banned?
The overlay only reads iRacing's shared-memory telemetry, the same public interface that apps like SimHub, CrewChief and Trading Paints use. It does not inject into, hook or modify iRacing. This is not an official statement from iRacing, so if you are unsure, check iRacing's current rules and terms yourself.

### Does it cost FPS?
It is designed to be very light. It is a small native window with no GPU rendering, and it only redraws when iRacing publishes new data. Expect negligible impact, though I haven't published benchmarks yet.

### Does it work on Mac or Linux?
No. It uses Windows APIs and iRacing's Windows shared memory.

## Using it

### I can't see the overlay in the game
1. iRacing must be in **windowed or borderless** mode. Windows can't draw over exclusive fullscreen.
2. The HUD only draws when something is happening: a car alongside, or a car within 3.5 m ahead. It is invisible otherwise. Press `Ctrl + Shift + M` to show the blue Move Mode frame and check its position.
3. It is hidden in **replays**, in the garage and when you are off track.
4. Delete `spotter_overlay.ini` to reset its position.

### Does it work in VR?
Not currently. It is a desktop window, so it won't appear inside a headset.

### Does it work on triple screens or multiple monitors?
You can drag the HUD to any monitor in Move Mode. It is designed for the screen iRacing is displayed on. Triple screen setups are untested, so reports are welcome.

### Does it work in replays or spectate mode?
No. It is intentionally disabled during replays.

### How do I move it again?
Press `Ctrl + Shift + M`, drag it, then right-click, `Enter`, `Esc` or `Ctrl + Shift + M` to lock it.

### How do I mute the chimes? Can I change the volume?
`Ctrl + Shift + A` toggles mute, and it is remembered. There is no built-in volume control yet. Use the Windows volume mixer entry for `spotter_overlay.exe`.

### How do I close it?
End `spotter_overlay.exe` in Task Manager. A proper exit option is a good [feature request](https://github.com/vijaynadimpalli/iracing-spotter-overlay/issues/new/choose).

### The chime plays through the wrong device
Chimes use the Windows default output device. Change that in Windows sound settings, then restart the overlay.

### Can I change the colours or size?
Not through settings yet. Colours and sizes are constants at the top of `spotter_overlay.cpp`, and you can change them and rebuild. A config option is a welcome contribution.

## Cars

### A car's pillar is too long or too short
The overlay uses a length per car. Open `spotter_overlay.ini` and edit that car under `[CarLengths]` (length in metres, between 2.0 and 8.0). Restart the overlay or rejoin the session. If you are happy with your number, please open an issue or PR so everyone gets the fix. See [CONTRIBUTING](../CONTRIBUTING.md#adding-or-fixing-a-car-length).

### A new iRacing car isn't in the list
It falls back to a guess based on its class, then to 4.70 m. It will be written into your `.ini` the first time you meet it, so you can adjust it there.

### Does it support oval racing?
Yes. NASCAR and other oval cars are in the length table, and the spotter logic follows the same iRacing signals.

## Contributing

### How can I help?
See [CONTRIBUTING.md](../CONTRIBUTING.md). The easiest ways are reporting bugs, verifying car lengths and testing on different setups (triple screen, different Windows versions, etc.).

### Where do I report a bug?
[Open an issue](https://github.com/vijaynadimpalli/iracing-spotter-overlay/issues/new/choose) and include your Windows version, iRacing display mode, the car(s) involved and your `spotter_overlay.ini`.
