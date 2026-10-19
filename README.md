# Audio Loop Station

Audo Loop Station: a desktop looper for layering up to four mono tracks over a shared loop clock.
Team: Elliot Hwang, Hunter Shipman, Robert Thom.

## Layout

| Folder     | Purpose                                                          | Framework headers allowed |
| ---------- | ---------------------------------------------------------------- | ------------------------- |
| `src/core/`    | Loop clock, track data, loop controller                      | No                        |
| `src/audio/`   | Audio engine: device I/O and callback                        | Yes (JUCE or miniaudio)   |
| `src/storage/` | Save and load loops (one WAV per track plus a settings file) | No                        |
| `src/ui/`      | Screens and controls                                         | Yes (JUCE or Dear ImGui)  |
| `src/app/`     | `main()`, wires everything together                          | Yes                       |
| `tests/`       | Unit tests for `core` and `storage`                          | n/a                       |
| `docs/`        | Project plan and architecture notes                          | n/a                       |

`src/core/` and `src/storage/` must not include JUCE, miniaudio, or Dear ImGui headers. The `no_framework_includes` test enforces this.
`src/audio/` and `src/ui/` are empty until the team picks a framework.

## Requirements

- Windows with a C++17 compiler (Visual Studio or Build Tools with the C++ workload)
- CMake 3.21 or newer
- git (CMake uses it to download Catch2)

Optional: `clang-format`, for formatting (it also ships with Visual Studio).

## Build and test

```
cmake --preset windows
cmake --build --preset debug
ctest --preset debug
```

The first configure downloads Catch2, so it needs network access.

## Formatting

```
clang-format -i <changed files>
clang-format --dry-run --Werror <changed files>
```

## Team workflow

- Changes to `main` go through a pull request with one approving review.
- Every PR must build, pass the tests, and you must be able to explain every line.
