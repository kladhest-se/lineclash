# LineClash

**Multiplayer falling blocks — real-time, fast, brutal.**

Up to 8 players per room, AI bots, 14 specials, and full guideline gameplay. Open source and self-hosted.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/frossmant)

## Features

- Up to 8 players per room — spectator mode and team play included
- Guideline gameplay — SRS wall kicks, hold piece, lock delay, back-to-back bonus, combos, perfect clear
- 14 specials — add lines, nuke, switch field, block bomb, gravity, quake, and more
- Configurable garbage targeting — All, Random, or Focused
- AI bots with adjustable difficulty
- Persistent rooms — settings saved and restored across server restarts
- Reconnect window — disconnected players hold their slot
- HTTP stats API on port 31458
- SDL2 GUI client with HiDPI/Retina support
- Terminal CLI client for headless or SSH play

## Build

**Requirements:** a C compiler, `make`, SQLite headers/library, SDL2 2.0.7+ and libcurl (GUI on macOS/Linux; Windows uses WinHTTP); Python 3 for HTTP integration tests

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
sudo zypper install -y gcc make libSDL2-devel sqlite3-devel libcurl-devel
make
```

**Ubuntu**
```bash
sudo apt install gcc make libsqlite3-dev libsdl2-dev libcurl4-openssl-dev python3
make
```

## Run

```bash
# Start server (default port 31457)
cp server/lineclashd.cfg.example ./lineclashd.cfg
./server/lineclashd --config ./lineclashd.cfg

# GUI client
./clients/gui/lineclash

# Terminal client
./clients/cli/lineclash-cli <server>

# Admin tool
./server/lineclashd-tool --help
```

## Guides

- [Playing and controls](docs/PLAYING.md)
- [Soundtrack credits](CREDITS.md) and [music player](music/README.md)
- [Configuration, accounts, deployment and backups](docs/CONFIGURATION.md)
- [Public HTTP API](docs/API.md)
- [Tests, builds, packaging and web hosting](docs/DEVELOPMENT.md)
- [Platform verification](docs/PLATFORM-VERIFICATION.md)
- [Client trust review](docs/TRUST-REVIEW.md)
- [GitHub source uploads and tagged releases](docs/RELEASING.md)

## License

[GPL-3.0-or-later](LICENSE.md) — © 2025 Tommy Frössman

## Sponsor

If you enjoy LineClash, consider supporting development:

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/frossmant)

## Public server directory and statistics

The website includes a live server browser, recent games and a JSON API backed by MariaDB. The dedicated-server supervisor automatically registers servers and retries game uploads. See [web service setup](docs/WEB-SERVICES.md) for provisioning, deployment, privacy and API details.

See [discovery, ranked history, health reports, replays and music shortcuts](docs/ENHANCEMENTS.md) for the latest controls, deployment changes and verification limits.

For your Leap 16 / Apache and remote MariaDB hosts, follow [the Kladhest deployment guide](docs/DEPLOYMENT-KLADHEST.md). The update helper is `scripts/refresh-lineclash.sh`.

The native GUI now uses antialiased Roboto typography and a unified slate/mint theme. See [the modern server browser](docs/images/gui-modern-empty.png). Restart the rebuilt client to use the new renderer.
