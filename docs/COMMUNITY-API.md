# Community API reference

The community website exposes `/api/?action=ACTION`. Public GET requests return JSON without authentication. This is separate from the dedicated server’s local status endpoint.

| Action | Parameters | Response |
| --- | --- | --- |
| servers | `format=text` optional | Live server listings; text format supports native discovery |
| statistics | none | Recorded games, ranked games, registered servers and live servers |
| leaders | none | Up to 50 ranked winners, grouped by server and nickname |
| matches | `server`, `mode`, `offset` optional | Up to 50 matches; mode is `all`, `ranked` or `casual` |
| match | `server`, `database`, `game` | Match metadata and player statistics |
| player | `server`, `nick` | A server-scoped player’s latest 50 match records |
| replay | `server`, `database`, `game`; `download=1` optional | Recorded events, original trace and integrity metadata |

URL-encode parameter values. Match pagination offsets are 0, 50, 100 and so on. Database IDs distinguish server database generations; a match is identified by all three key fields. Server listings expire after their announcement TTL. Results and replays are server-reported evidence, not proof of cheat-free play. Nicknames do not establish an identity across servers.

```sh
curl --fail 'https://lineclash.example.org/api/?action=servers'
curl --fail 'https://lineclash.example.org/api/?action=matches&mode=ranked&offset=0'
curl --fail 'https://lineclash.example.org/api/?action=statistics'
```

## Errors and announcements

Invalid requests return 400; failed server authentication returns 403; missing matches return 404; unsupported methods return 405; service failures return 503. Responses include a JSON `error` message. Do not treat unavailable services as an empty successful result.

Operators send announcements using POST JSON with provisioned `server_id` and `token` values. Announcements have a 2 MiB request limit. The directory supervisor handles registration heartbeats, durable game delivery and retries. Keep its token private and use HTTPS. See [web services](WEB-SERVICES.md) for setup and [replay verification](ENHANCEMENTS.md) for integrity checks.
