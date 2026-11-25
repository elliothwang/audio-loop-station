# Development

How to build, test, and release Audio Loop Station, and how the code is laid out. To use the app
instead, see [Install and run](install.md).

## Requirements

- Windows 10 or 11 (64-bit)
- Visual Studio 2022, or Build Tools for Visual Studio 2022, with the **Desktop development with C++**
  workload
- CMake 3.22 or newer
- git
- A network connection for the first configure, which downloads JUCE 8.0.6 and Catch2 3.7.1

Optional: `clang-format`, for formatting. It also ships with Visual Studio.

## Build and test

```
cmake --preset windows
cmake --build --preset debug
ctest --preset debug
```

Use the `release` preset in place of `debug` for an optimized build. The app is built to
`build\src\app\audio_loop_station_artefacts\<Debug or Release>\Audio Loop Station.exe`.

The MSVC runtime is linked statically (see the top-level `CMakeLists.txt`), so the exe runs on a
machine without the Visual C++ Redistributable. Every target, including JUCE and Catch2, has to use
the same runtime, or linking fails.

## Formatting

```
clang-format -i <changed files>
clang-format --dry-run --Werror <changed files>
```

## Continuous integration

GitHub Actions runs three jobs on every pull request and every push to `main`:

- **build-and-test**: configures, builds Debug, and runs `ctest` on Windows.
- **release-exe**: builds Release for x64, runs `ctest` on it, and uploads `AudioLoopStation.exe` as
  the run's **AudioLoopStation** artifact. Download it from the run's **Summary** page to try a PR's
  build without compiling.
- **format**: runs `clang-format --dry-run --Werror` on every C++ file.

## Layout

| Folder | Purpose | Framework headers allowed |
|---|---|---|
| `src/core/` | Sample clock, loop timing, loop controller, playback math, pan law | No |
| `src/audio/` | Audio engine: recorder, track buffers, command queue, audio callback | Yes (JUCE) |
| `src/storage/` | Save and load loops (one WAV per track plus `loop.json`) | Yes (JUCE files and WAV) |
| `src/ui/` | Window, transport bar, track lanes, saved loops panel | Yes (JUCE) |
| `src/app/` | `main()`, wires everything together | Yes |
| `tests/` | Unit tests for `core`, `audio`, and `storage` | n/a |
| `docs/` | Install guide, user guide, and this page | n/a |

`src/core/` must not include JUCE, miniaudio, or Dear ImGui headers. The `no_framework_includes` test
enforces this.

## Releasing

Graders and users download the exe from GitHub Releases, so each release attaches the x64 Release
build from CI. Don't attach a local build: on an ARM PC it's ARM64, which won't run on an x64 PC.

1. Bump `VERSION` in the `project()` line of the top-level `CMakeLists.txt` if it changed, and merge
   to `main`.
2. Download the exe from the CI run for that commit on `main`:
   ```
   gh run list --workflow CI --branch main --limit 1
   gh run download <run id> --name AudioLoopStation
   ```
   This saves `AudioLoopStation.exe`, already renamed, since GitHub replaces spaces in asset names
   and the install guide links to that name.
3. Optionally, check the exe doesn't need the Visual C++ runtime. From a Developer PowerShell for
   VS 2022:
   ```
   dumpbin /dependents AudioLoopStation.exe
   ```
   The list should only have Windows system DLLs, with no `MSVCP140.dll`, `VCRUNTIME140.dll`, or
   `ucrtbase.dll`.
4. Create the release, with the tag matching the version:
   ```
   gh release create v0.1.0 AudioLoopStation.exe --title "v0.1.0" --notes "<what changed>"
   ```

## Team workflow

- Changes to `main` go through a pull request with one approving review.
- Every PR must build, pass the tests, and you must be able to explain every line.
