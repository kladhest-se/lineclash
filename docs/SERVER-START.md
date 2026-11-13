# Start a game server

Build or extract the dedicated server for your platform. You do not need MariaDB to run a game server: accounts, room settings and game records use its local SQLite database.

## macOS and Linux

From a source checkout, create a writable configuration and start an ordinary-user instance:

```sh
cp server/lineclashd.cfg.example ./lineclashd.cfg
./server/lineclashd --config ./lineclashd.cfg
```

From an extracted native package, use `./bin/lineclashd` instead. Set explicit writable `db_file` and `log_file` paths before running it as a service.

## Windows

Build the dedicated server and administration tool in an MSYS2 UCRT64 terminal:

```sh
pacman -S --needed make git mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3
git clone https://github.com/kladhest-se/lineclash.git
cd lineclash
make -j4 server
./server/lineclashd.exe
```

SDL2 is not needed for the server. Run `./server/lineclashd-tool.exe --help` for remote administration options. Keep required runtime DLLs available outside the MSYS2 environment. Press Ctrl+C in the server terminal to stop it.


Run `server/lineclashd.exe` from a source build or `bin/lineclashd.exe` from an extracted package. Defaults are stored under `%APPDATA%\LineClash\Server`. Use `--config PATH` to select another configuration.

## Connect and stop

The default game port is TCP 31457. Connect the game client to `localhost:31457` on the same machine, or to the server’s reachable host and port from another machine. Allow that game port through the host firewall when accepting remote players.

The separate status interface defaults to TCP 31458 and can be disabled. It is not required for a local match. See [configuration](CONFIGURATION.md) and the API page before exposing status data publicly.

Stop the process before replacing its database. See [server management](SERVER-MANAGEMENT.md) for accounts, backups and administration. Build instructions are on Source.

## Choose configuration and data paths

The server accepts `--config PATH`, `--port PORT`, `--logfile PATH`, `--version` and `--help`. Command-line port/log options override the configuration file; there is no `--db` argument. Set `db_file` in the configuration instead. Relative paths are resolved from the process working directory, so use absolute paths for background services.

Without `--config`, macOS uses `~/.lineclashd/lineclashd.cfg`; Windows uses `%APPDATA%\LineClash\Server\lineclashd.cfg`; Linux uses `/etc/lineclashd.cfg`. Default database/log paths are in the same server directory on macOS/Windows and `/var/lib/lineclashd/database.sqlite3` / `/var/log/lineclashd.log` on Linux. An ordinary Linux user should select a configuration with writable database and log paths. Use `server path` at the local console to check the paths actually in use.

## Stop and restart safely

For a foreground instance, press **Ctrl+C**, or enter `server shutdown` in its local console. On macOS/Linux, SIGTERM also requests shutdown. To warn connected players first, use:

```text
server shutdown 5 Planned maintenance
server shutdown cancel
```

The first command schedules a shutdown in five minutes; the second cancels a pending countdown. Wait for the process to exit before replacing binaries or restoring a database. An active round cannot resume after a server restart.

To restart, start the same executable with the same configuration and working directory. For a service-managed installation, stop and start the configured service rather than launching a second copy. Read configuration changes at startup; editing the file does not reload a running process. Keep writable database/log directories and avoid running the server as root.
