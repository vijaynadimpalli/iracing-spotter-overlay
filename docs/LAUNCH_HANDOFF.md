# Launch handoff

Working notes for whoever (human or agent) picks up the public launch of this project. This file is a task list, not user documentation. Delete it once the work is done.

## Goal

Share the repo on Reddit (r/iRacing, r/simracing) as a free, open-source project. It needs clean code, a good README, setup docs, an FAQ, and contributor info.

## Project facts (verified from the code)

- Windows-only C++17 overlay. The whole app is `spotter_overlay.cpp` (about 1,500 lines). `spotter_tests.cpp` does `#include "spotter_overlay.cpp"` with `SPOTTER_TEST_BUILD` defined.
- Build and test: `build_and_test.ps1`. It needs MinGW-w64 `g++`, looked up at `%TEMP%\toolchain\w64devkit\bin` first, then on `PATH`.
- It reads iRacing shared memory (`Local\IRSDKMemMapFileName`) read-only. It follows iRacing's own `CarLeftRight` state.
- It is hidden in replays, when not on track, and in the garage. First run with no `.ini` starts in Move Mode.
- Hotkeys: `Ctrl+Shift+M` (move/lock), `Ctrl+Shift+A` (mute). There is no tray icon or exit option, so users must use Task Manager.
- Config is `spotter_overlay.ini` next to the `.exe`, with `[Overlay]` (`PosX`, `PosY`, `Muted`) and `[CarLengths]` (accepted range 2.0–8.0 m).
- Author preference: the README says users should already know the iRacing spotter UI, so no long explanation of the basic concept is needed.

## Already done (merged to `main`)

README rewrite, `LICENSE` (MIT), `docs/SETUP.md`, `docs/FAQ.md`, `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, issue and PR templates, and `.github/workflows/ci.yml`.

## To do

Items marked **(owner)** need the repo owner, because they need Windows, iRacing or GitHub admin access. An agent should prepare what it can and say clearly what is left.

### 1. Verify CI
- [ ] Check the Actions run on `main` for `.github/workflows/ci.yml`.
- [ ] If it fails, fix the workflow or `build_and_test.ps1`. Possible causes: `g++` missing from `PATH` on the runner, or test behaviour with no audio device. The tests mute audio, but this has not been confirmed on a runner.
- [ ] The README CI badge should show passing.

### 2. Screenshot / GIF **(owner)**
- [ ] The owner captures the overlay during a race (a short GIF is best).
- [ ] Save it as `docs/screenshot.png` or `docs/demo.gif` and replace the `TODO` comment in `README.md` with the image.

### 3. GitHub settings **(owner)**
- [ ] Enable **Discussions** (the issue template config links to it).
- [ ] Create labels: `good first issue`, `help wanted`, `bug`, `enhancement`.
- [ ] Add repo description and topics (for example `iracing`, `sim-racing`, `overlay`, `spotter`, `telemetry`, `cpp`, `windows`).
- [ ] Open 3–5 `good first issue` items. Ideas are listed under "Optional improvements" below.

### 4. Release v0.1.0 **(owner)**
- [ ] Build `spotter_overlay.exe` on Windows with `build_and_test.ps1`.
- [ ] Create a GitHub Release with the `.exe` attached, a short changelog, and the SHA256 (`Get-FileHash`).
- [ ] Mention the SmartScreen warning in the notes (the `.exe` is unsigned).

### 5. Claims to confirm **(owner)**
These statements in the docs could not be verified from the code. Correct the docs if any is wrong.
- [ ] Does not draw over exclusive fullscreen (windowed or borderless is required).
- [ ] Does not appear in VR.
- [ ] Triple screen behaviour is untested.
- [ ] The "will this get me banned?" answer in `docs/FAQ.md`. Reword if needed after checking iRacing's current rules.
- [ ] FPS impact. The FAQ says "negligible" but there are no benchmarks.

### 6. Reddit post
- [ ] Read the self-promotion rules for r/iRacing and r/simracing before posting.
- [ ] Draft the post: short title, one-line pitch, what makes it different from the stock spotter (real car lengths for multi-class, draft pull-out bar, stereo chimes), GIF, download link, repo link, and a clear ask for feedback (especially car lengths and triple-screen reports).
- [ ] Only post after steps 1–4 are done.

## Optional improvements (good for `good first issue` / later PRs)

- Tray icon with an **Exit** option, and maybe a settings menu.
- Config options for colours, HUD scale, and chime volume (all currently compile-time constants).
- Remove the hardcoded `%TEMP%\toolchain\w64devkit` lookup preference, or add a `CMakeLists.txt` so MSVC users can build too.
- Split `spotter_overlay.cpp` into modules (car lengths, audio, processor, rendering, main) so tests don't need to `#include` a `.cpp`.
- Add `.clang-format` and `.editorconfig`.
- Verify car lengths against manufacturer data and add missing cars.
- Tests for the config and car length handling.

## Notes for the next agent

- The repo can't be built or run on macOS, so don't claim a build or test passed unless CI or a Windows run confirms it.
- Keep the README claims accurate to the code. When in doubt, check `spotter_overlay.cpp` (thresholds are in section 2, spotter logic in section 6).
- Preserve existing comments and docstrings unrelated to your change.
