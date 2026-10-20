# Contributing

Thanks for wanting to help! This is a small project, so contributing is easy. You don't need to be a C++ expert to help with car lengths, testing or docs.

By taking part you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

## Ways to help

- **Report bugs.** Use the [bug report form](https://github.com/vijaynadimpalli/iracing-spotter-overlay/issues/new/choose).
- **Verify or add car lengths.** The easiest high-value contribution (see below).
- **Test on other setups:** triple screens, different Windows versions, different display modes. Report what works and what doesn't.
- **Suggest features** with the feature request form. Check existing issues first.
- **Improve docs.** Typos, clearer wording and screenshots are all welcome.
- **Fix issues.** Look for [`good first issue`](https://github.com/vijaynadimpalli/iracing-spotter-overlay/labels/good%20first%20issue) and [`help wanted`](https://github.com/vijaynadimpalli/iracing-spotter-overlay/labels/help%20wanted).

For anything larger than a small fix, please open an issue first so we can agree on the approach before you spend time on it.

## Project layout

| File | What it is |
| :--- | :--- |
| `spotter_overlay.cpp` | The whole application (a single translation unit). |
| `spotter_tests.cpp` | Test suite. It does `#include "spotter_overlay.cpp"` with `SPOTTER_TEST_BUILD` defined, which turns off `WinMain`. |
| `build_and_test.ps1` | Builds and runs the tests, then builds the overlay. |
| `docs/` | Setup guide and FAQ. |

Inside `spotter_overlay.cpp` the code is split into numbered sections:

1. Embedded iRacing shared-memory definitions
2. Tuning constants and geometry
3. Car length table and session YAML parser
4. `StereoChimeEngine` (audio)
5. Telemetry snapshot and offsets
6. `SpotterProcessor` (the spotter and front-bar logic, which is where most behaviour lives)
7. Shared-memory reading
8. `LayeredCanvas` (rendering)
9. Window state, config persistence and `WndProc`
10. Main loop

## Development setup

You need Windows 10/11 and a C++17 `g++` (MinGW-w64, for example [w64devkit](https://github.com/skeeto/w64devkit)). See [docs/SETUP.md](docs/SETUP.md#building-from-source).

```powershell
git clone https://github.com/vijaynadimpalli/iracing-spotter-overlay.git
cd iracing-spotter-overlay
powershell -ExecutionPolicy Bypass -File .\build_and_test.ps1
```

You do not need iRacing running to build or run the tests.

## Making a change

1. Fork the repo and create a branch: `git checkout -b fix/short-description`.
2. Make your change. Keep it focused, with one fix or feature per pull request.
3. **Add or update tests** in `spotter_tests.cpp` for any logic change in the spotter, parser or car length code.
4. Run `build_and_test.ps1`. All tests must pass.
5. Open a pull request and fill in the template. Describe what changed, why, and how you tested it. If it changes what the HUD looks like, add a screenshot.

### Code style

- C++17, no external dependencies.
- Match the surrounding code's formatting and naming.
- The render loop runs continuously, so avoid heap allocations and slow work there.
- Prefer named `constexpr` constants (see section 2) over magic numbers.
- Comment the *why* for anything non-obvious, especially geometry and threshold logic.

## Adding or fixing a car length

Lengths are in the table `kChassisLengthTable` near the top of `spotter_overlay.cpp`.

1. Find the car's `CarPath` in your `spotter_overlay.ini`. Every car you meet is recorded under `[CarLengths]`.
2. Find the real length in metres from the manufacturer or a reliable source. Please mention the source in your pull request.
3. Add or edit an entry:

   ```cpp
   { "porsche992gt3r", 4.63f }, // Porsche 911 GT3 R (992)
   ```

   The `token` is matched as a **substring** of the `CarPath`, and the **first match wins**. Put more specific tokens before more general ones (`caterham420r` comes before `caterham`).
4. Add a test case in `spotter_tests.cpp` if the car needs a specific token order.

Not comfortable with C++? Open an issue with the car name, its `CarPath` and the length, and someone will add it.

## Commit messages

Short, imperative subject line (for example `Fix front bar not clearing after pit exit`). Add a body if the *why* isn't obvious.

## Releases

Maintainers publish prebuilt `spotter_overlay.exe` builds on the [Releases](https://github.com/vijaynadimpalli/iracing-spotter-overlay/releases) page.

## License

By contributing, you agree that your contributions are licensed under the [MIT License](LICENSE).
