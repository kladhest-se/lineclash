# LineClash

**Multiplayer falling blocks — real-time, fast, brutal.**

Up to 8 players per room, AI bots, 10 specials, and full guideline gameplay. Open source and self-hosted.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/frossmant)

## Features

- Up to 8 players per room — spectator mode and team play included
- Guideline gameplay — SRS wall kicks, hold piece, lock delay, back-to-back bonus, combos, perfect clear
- 10 specials — add lines, nuke, switch field, block bomb, gravity, quake, and more
- Configurable garbage targeting — All, Random, or Focused
- AI bots with adjustable difficulty
- Persistent rooms — settings saved and restored across server restarts
- Reconnect window — disconnected players hold their slot
- HTTP stats API on port 31458
- SDL2 GUI client with HiDPI/Retina support
- Terminal CLI client for headless or SSH play

## Build

**Requirements:** `gcc`, `make`, `libSDL2` (GUI client only)

```bash
# Everything
make

# Server only — no SDL2 needed
make server

# GUI client
make client

# Terminal client
make cli
```

**macOS**
```bash
brew install sdl2
make
```

**openSUSE Leap**
```bash
sudo zypper install -y gcc make libSDL2-devel sqlite3-devel
make
```

**Ubuntu**
```bash
sudo apt install gcc make libsdl2-dev
make
```

## Run

```bash
# Start server (default port 31457)
./server/lineclashd

# GUI client
./clients/gui/lineclash

# Terminal client
./clients/cli/lineclash-cli <server>

# Admin tool
./server/lineclashd-tool localhost status
```

## License

[GPL-3.0](LICENSE.md) — © 2025 Tommy Frössman

## Sponsor

If you enjoy LineClash, consider supporting development:

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/frossmant)
