# Manage a game server

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


## Everyday administrator commands

The following commands use the local console syntax. Remote command availability depends on the authenticated account's privilege. Start with `help` for the current server's full reference.

| Task | Command |
| --- | --- |
| Check health and paths | `server status`, `server path` |
| List rooms and users | `room list`, `user list` |
| Inspect an account | `user list yournick` |
| Announce maintenance | `say global Maintenance starts in five minutes` |
| Change the greeting | `server motd set Welcome to our server` |
| Disable / enable an account | `user set yournick active 0` / `user set yournick active 1` |
| Kick from the server | `user kick server yournick Reason` |
| Ban / unban an account | `ban add user yournick Reason` / `ban remove user yournick` |
| List bans | `ban list` |
| Set a room topic | `room set 1 topic Casual practice` |
| Disable room specials | `room set 1 specials 0` |
| Add a practice bot | `bot add 1 easy` |
| List / remove bots | `bot list 1`, then `bot remove 1 SLOT` |

Kicking removes a connection; banning prevents future logins until removed. A room host's `/kick` applies to the room and does not grant server-wide administration. Assign administrator privileges only to trusted accounts; use ordinary accounts for everyday play. Account deletion is permanent, so disabling an account is usually a better first step.

## Operational checks

After starting or restarting, inspect the log for database or listener errors, run `server status`, and connect a client to the game port. Confirm the expected rooms and account access before inviting players. If settings seem unchanged, check `server path`, the process working directory and saved room settings: persisted rooms can override initial configuration defaults.

If startup fails, check config readability, database/log directory write permissions, and whether another process already owns the chosen port. Back up the database before maintenance and confirm the backup can be opened before relying on it.
