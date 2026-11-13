# Platform verification

Run `bash scripts/verify-platform.sh` (or `make verify`) in a clean checkout on each target OS. It runs offline release-tool tests, a clean parallel build, field/network/authentication checks, real HTTP and trust/reconnect protocol tests, and a headless GUI reconnect lifecycle test. macOS/Linux also run the actual terminal-client reconnect tests. Logs are saved under dist/verification/ and collected by CI.

CI jobs verify:macos, verify:ubuntu and verify:leap15 run natively. Package jobs depend on their matching verification job. Windows verification is opt-in with `LINECLASH_WINDOWS_RUNNER=1` until a Windows runner tagged `windows, msys2` is registered. It uses PowerShell to start MSYS2 UCRT64 Bash at `LINECLASH_MSYS2_ROOT` (default C:\msys64).

Install the following on that Windows runner from its UCRT64/MSYS2 environment:

```sh
pacman -S --needed make git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-python
```

The Windows gate builds the server, administration tool and SDL2 GUI, runs portable C tests over loopback TCP, and runs the Python HTTP/protocol suites. The POSIX-only C fork harness and terminal client are excluded there. Windows process-statistics calls and executable/library targets have platform-specific implementations. Interactive administration on Windows has fewer terminal facilities than POSIX; single-command/authentication paths are portable.

Evidence from this development host: macOS native builds and runtime suites pass; MinGW compiles the shared/server/GUI sources, including the bundled audio decoders for Windows. The latest cross-build also links all three Windows x64 executables against genuine Windows SDL2/SQLite libraries and checks recursive DLL imports. It does not execute Windows binaries. No Linux container engine or native Windows runner is available here, so Linux/Windows runtime and installer validation remain pending those CI jobs. GUI lifecycle checks are headless; they do not replace visual gameplay testing on each OS.

The music gate checks all original audio hashes and credits, decodes all 19 selections, tests stereo streaming and controls with SDL dummy audio, and renders the player/About screens to check layout and wheel/Esc handling.

See [Windows builds](WINDOWS.md) for native UCRT64 commands, portable ZIP packaging and verification limits.

A GitHub-hosted Windows x64 workflow also runs the gate and packages a portable ZIP on pushes and pull requests; it does not require a separately registered Windows runner.
