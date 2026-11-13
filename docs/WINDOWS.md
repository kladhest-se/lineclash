# Windows build and packaging

Target: native Windows 10/11 x64 using MSYS2 UCRT64. The SDL2 GUI, dedicated server and administration tool build on Windows.

Install [MSYS2](https://www.msys2.org/), open its **UCRT64** terminal, update with `pacman -Syu` (restart the terminal and repeat if requested), then:

```sh
pacman -S --needed make git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-python
git clone https://github.com/kladhest-se/lineclash.git
cd lineclash
make -j4
./clients/gui/lineclash.exe
# In a separate terminal, if hosting locally:
./server/lineclashd.exe
```

The GUI opens without an extra console window. It saves settings and servers under `%APPDATA%\LineClash`. The server uses `%APPDATA%\LineClash\Server` for its default configuration, database and logs, and also accepts `--config PATH`. Use `./server/lineclashd-tool.exe --help` for administration. Windows HTTPS discovery uses WinHTTP; no libcurl runtime is needed.

For verification and a distributable ZIP:

```sh
make verify
python scripts/package-windows.py
```

The ZIP in `dist/` contains `bin/lineclash.exe`, `bin/lineclashd.exe`, `bin/lineclashd-tool.exe`, non-system runtime DLLs, documentation, licenses and music. Extract the entire archive into a writable folder before launching the game. Runtime imports are checked recursively; packaging stops when a required DLL is missing. Python, `objdump`, GCC and libraries should come from the same UCRT64 environment. The normal release coordinator also bundles these DLLs for a native Windows release. Packaging alone does not establish runtime validation or signing.

For cross-compilation, set `PLATFORM=win32` and `WINDOWS_SDL_PREFIX` to the x64 SDL2 development prefix for the GUI; pass Windows SQLite include/link paths to the server Makefile. Cross-build into a separate checkout so object files cannot collide with native objects. `package-windows.py --runtime-dir PATH` adds a trusted runtime directory, and `--objdump x86_64-w64-mingw32-objdump` selects a cross inspector. An existing ZIP is never overwritten; select another `--output` or archive the previous ZIP first.

Verification evidence: all three x64 executables were cross-compiled and linked with genuine Windows SDL2 and SQLite import libraries on macOS. The GUI PE subsystem and recursive DLL imports were inspected. Native macOS checks cover shared gameplay, reconnects and SDL initialization. Windows execution, audio, network discovery and interactive gameplay still need the Windows verification job or a Windows host; cross-linking cannot establish those results.

Repository CI is disabled for now. Run verification and ZIP packaging manually on Windows before publishing a Windows build.
