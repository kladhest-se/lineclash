# Public HTTP status API

Default: `http://localhost:31458/status`. All paths and methods currently return the same full status snapshot; there is no write API. Query with:

```sh
curl --fail http://localhost:31458/status
```

The response is HTTP 200, `application/json`, with Content-Length and connection close. CORS allows all origins. No authentication is required. Requests have a 511-byte header limit and five-second total lifetime; at most 16 HTTP clients are active at once. Excess or incomplete requests may be closed without a response. Responses are bounded to 512 KiB.

| Root field | Type / contents |
| --- | --- |
| server_name, version, motd | strings |
| port, status_port | integer TCP ports |
| timestamp | local server time string, `YYYY-MM-DD HH:MM:SS`; no timezone offset |
| players | object: online, playing, idle, bots_active, registered counts |
| rooms | array: id, state, players, spectators, playing, bots and difficulty counts, max_players, round, paused and room settings |
| rooms[].player_list | nick, team, slot, is_bot, optional bot_difficulty, status, level, lines, score |
| rooms[].room_player_wins / room_team_wins | persistent nick/team win summaries |
| rooms[].session_player_wins / session_team_wins | current session win summaries |
| lobby_players | array: nick, status for authenticated players selecting a room |
| leaderboard | up to 20 entries: rank (string), nick, team, wins, attacks |
| team_leaderboard | up to 20 entries: rank (string), team, wins, members |
| user_stats | up to 100 active users: nick, team, wins, attacks, taunts_sent, specials_sent, blocks_placed, lines_cleared, messages_sent, kicks_from_server, kicks_from_room, bans_received |
| recent_games | up to 50 entries: game_id, room_id, started_at, ended_at, winner, winner_team, players |
| server_stats | uptime_seconds, games_started, games_ended, specials_fired, lines_cleared, connects, disconnects, msgs_sent, msgs_recv, bytes_sent, bytes_recv, kicks and selected game defaults |

Settings and boolean flags are integers; room state is `open` or `playing`. Player status is `idle`, `playing`, `eliminated`, `spectator`, or `lobby` as applicable. There is no pagination or schema-version negotiation; clients should tolerate extra fields.

Names, team memberships, activity and moderation counters are public through this endpoint. It exposes no password hashes. Operators should choose whether to expose these statistics. The listener binds all IPv4 interfaces; restrict it with a firewall/reverse proxy or disable it with `status_api = off` or `status_port = 0` in the server config. See [configuration](CONFIGURATION.md).

## Public directory API

The local dedicated-server status API described above remains available. The separate public website API accepts authenticated server announcements and provides MariaDB-backed listings, matches, counts and server-scoped win totals. See [web services](WEB-SERVICES.md) for routes, payloads and deployment.

See [discovery, ranked history, health reports, replays and music shortcuts](ENHANCEMENTS.md) for the latest controls, deployment changes and verification limits.
