# Server configuration and operations

Start an ordinary-user instance from the project root:

```sh
cp server/lineclashd.cfg.example ./lineclashd.cfg
./server/lineclashd --config ./lineclashd.cfg
```

The example uses relative database/log paths, resolved from the process working directory. Use absolute paths for a service. The server accepts `--config PATH`, `--port PORT`, `--logfile PATH`, `--version` and `--help`; it does not accept `--db`. Command-line port/log overrides win over the file. Configuration is read at startup; restart after editing it.

Without `--config`, macOS uses `~/.lineclashd/lineclashd.cfg`, `database.sqlite3` and `lineclashd.log` in that directory. Linux and Windows defaults are `/etc/lineclashd.cfg`, `/var/lib/lineclashd/database.sqlite3` and `/var/log/lineclashd.log`; use an explicit config with writable paths on Windows or for a Linux user instance. A missing config triggers an attempt to write a commented default file. A service account needs read permission on its config and write permission in its database and log directories. Do not run the game server as root merely to use the default paths.

## File format and options

One `key = value` per line. `#` starts a comment; values are unquoted. Missing keys use defaults. Empty values are ignored. Boolean switches identified below accept `on/off`, `yes/no` or `1/0`; numeric flags use `1/0`.

| Key | Default | Meaning / accepted range |
| --- | --- | --- |
| server_name | LineClash | Name in discovery/status |
| port | 31457 | Game TCP port, 1–65535 |
| db_file | platform path above | SQLite data, including accounts and room settings |
| log_file | platform path above | Append-only server log |
| motd | Welcome to LineClash! Type /help for commands. | Greeting; maximum 511 bytes |
| status_api | on | Enable public HTTP API; boolean switch |
| status_port | 31458 | HTTP TCP port; 0 disables; invalid range disables |
| require_registered | 0 | Reject new-account registration when 1 |
| max_players_per_room | 8 | 2–8, including bots |
| num_lobbies | 3 | 1–10 rooms |
| global_taunts | 1 | Allow taunts server-wide |
| allow_bots | on | Allow hosts/admins to add bots; boolean switch |
| debug_auth | off | Legacy diagnostic toggle; authentication secrets are never logged |
| debug_ingame | off | Detailed gameplay logging; boolean switch |
| initiallevel | 1 | Starting level, 1–100 |
| linesperlevel | 2 | Lines per level increase, minimum 1 |
| levelinc | 1 | Levels per increase, minimum 1 |
| averagelevels | 1 | Average players' levels |
| speciallines | 1 | Lines per special award, minimum 1 |
| specialcount | 1 | Specials per award, minimum 0 |
| specialcapacity | 18 | Inventory size, 0–64 |
| speedup | on | End prolonged games with periodic garbage; boolean switch; alias speedup_enabled |
| speedup_delay | 10 | Minutes before speed-up; minimum 1 |
| speedup_interval | 60 | Seconds between additions; minimum 5 |
| speedup_lines | 1 | Lines per addition, 1–4 |
| lock_delay | 300 | Lock delay in milliseconds, 0–500; alias lock_delay_ms |
| reconnect_window | 15 | Slot reservation during an active game, seconds, 0–120; 0 disables |
| garbage_system | 0 | 0 classic, 1 guideline |
| garbage_targeting | 0 | 0 all opponents, 1 random opponent, 2 focused |
| btb_bonus | 1 | Guideline back-to-back bonus, 0–4 |
| combo_cap | 4 | Maximum combo bonus, 0–20 |
| perfect_clear_bonus | 10 | Perfect-clear garbage, minimum 0; 0 disables |
| garbage_hole_style | 0 | 0 random per line, 1 same per attack, 2 aligned |
| hold_enabled | 1 | Allow holding a piece |

Room settings and bot selections persist in the database. Existing saved room preferences can override configuration defaults; use room controls or server-console `help` to change those rooms.

## Accounts and administration

New nicknames register on first connection using the client's password flow when `require_registered = 0`. Existing accounts must authenticate with their password. Nicknames are 1–15 printable characters without spaces or colons. Deactivated or banned accounts cannot log in.

Bootstrap an administrator from the server's local console after registering that account:

```text
user set yournick privilege 1
```

Privilege levels are 1 administrator, 2 manager, 3 ordinary user. A room host can manage room settings subject to server policy. Use local-console `help` for commands and permission details. `user set yournick active 0` disables an account; `user set yournick password NEW_PASSWORD` resets its password. Keep terminal history and the console restricted.

For remote administration, save a connection with the actual tool options:

```sh
./server/lineclashd-tool --add --host localhost --port 31457 --username yournick --password YOUR_PASSWORD
./server/lineclashd-tool --list
./server/lineclashd-tool --command YOUR_SAVED_ALIAS "help"
```

Use the alias shown by `--list`. `--connect ALIAS` opens an interactive session; `--help` lists options. Command-line passwords can appear in process listings and shell history. Saved password hashes are authentication credentials: restrict access to client/tool settings and database backups. Transport is plain TCP; use trusted networks or a protected tunnel for remote administration and account registration.

## Exposure, backup and recovery

Both listeners bind all IPv4 interfaces. Use a firewall to restrict game/API access. Disable the API with `status_api = off` or `status_port = 0` if public statistics are unwanted. Changing `port` does not automatically change `status_port`. An HTTPS reverse proxy can front HTTP status, but does not encrypt the separate game connection.

Stop the server before making a simple file copy of the database, or use SQLite's online backup command while running:

```sh
sqlite3 /absolute/path/database.sqlite3 ".backup '/absolute/path/backup.sqlite3'"
```

Keep protected, dated backups. To restore, stop the server, preserve the current database and any `-wal`/`-shm` files together, replace the database with a known-good backup, and restart. Do not mix old journal files with a restored database. Accounts, privileges, bans, room settings and statistics belong to the same database. Logs need separate rotation/retention. Ctrl+C or SIGTERM stops the server; an in-progress game is not a saved-game checkpoint.

## Public website registration

Run the dedicated server through `scripts/server_directory.py` to announce it automatically. Its URL, approved server ID, local database and private state file are wrapper arguments; the token is `LINECLASH_DIRECTORY_TOKEN`. These are not `server.cfg` keys. Optional `--status-url` reads public counts from the local status API. Set `LINECLASH_DIRECTORY_URL` in the GUI environment for the web browser shortcut. See [setup](WEB-SERVICES.md).

See [discovery, ranked history, health reports, replays and music shortcuts](ENHANCEMENTS.md) for the latest controls, deployment changes and verification limits.
