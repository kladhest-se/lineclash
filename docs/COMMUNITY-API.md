# Community API reference

The community website exposes `/api/?action=ACTION`. Public GET requests return JSON without authentication. This is separate from the dedicated server’s local status endpoint.

## Public endpoints

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

## Native account sign-in

These POST JSON actions return plain text and require HTTPS. Do not put credentials or sessions in URL parameters. Password login is independent of browser cookies.

| Action | Body fields | Success response |
| --- | --- | --- |
| account-client-pin-begin | `client`: `desktop` or `amiga` | `central-pin DEVICE_SECRET PIN EXPIRES` (five minutes) |
| account-client-pin-poll | `device`: private device secret | `central-session TOKEN HANDLE EXPIRES EMAIL` after browser approval |
| account-client-register | `handle`, `email`, `password`, `format: "session"` | `central-session TOKEN HANDLE EXPIRES EMAIL`, or `central-verification-required MESSAGE` when activation is required |
| account-client-login | `login` (email or existing username), `password` | `central-session TOKEN HANDLE EXPIRES EMAIL` (12 hours) |
| account-client-join | `session`, `challenge`, `host`, `port` | `central-joined HANDLE` |
| account-client-logout | `session` | `central-signed-out` |

Registration enforces unique usernames/emails and the configured password policy. When activation is disabled, account creation and session issuance are atomic and registration signs the client in directly. With `account_require_email_verification` enabled, registration sends an activation email, returns `central-verification-required` with HTTP 200 and issues no session. The client remains signed out, directs the player to their inbox and offers the activation resend page. Email confirmation does not automatically sign the client in; the player signs in normally afterward. Pending accounts cannot authenticate through native passwords, browser passwords, PINs or existing sessions while the setting is enabled. No recovery code is returned. Clients must send `format: "session"`; older registration formats receive an update-required error before any account is created. Email is private and is not a verified ownership claim. Email password recovery uses the separate browser page `/account/recover/` only when SMTP is configured; no reset token is sent through the game protocol. Existing clients may still send `handle` for username login. A missing email on an older account is represented by `-` in the login response.

For PIN login, display only the eight-digit PIN and `/account/pin/` URL to the user; keep the random 64-character `DEVICE_SECRET` private and in memory. The browser accepts the PIN, username/email and password and requires explicit approval. Native clients poll over HTTPS at least five seconds apart. `central-pending` means keep waiting; `central-slow-down` means wait ten seconds before retrying. A denied, expired, consumed or revoked request returns `central-error ...`. These PIN state replies use HTTP 200; malformed inputs and source limits return 400/429. Only the device secret can redeem approval, once, for a normal twelve-hour account session. PINs alone cannot obtain sessions. Password resets or account deactivation prevent outstanding approvals from being redeemed. The PIN endpoints use LineClash’s text protocol; they are not a general-purpose OAuth service.

The game server creates a connection-bound request with `account-begin` and `direct=true`, authenticating with its own directory key and random nonce. It gives the client the returned 64-character challenge. The client authorizes that challenge through `account-client-join`; its target host and port must match the request's live verified directory entry. The server polls its own request using `account-poll` and consumes the approved identity once. Replays, expired requests and mismatched targets are rejected. Client passwords and session tokens are not part of the game protocol.

Client-account errors return plain text starting with `central-error` or `central-expired`, including database/service failures. Missing tables or columns return HTTP 503 with an instruction to run `php bin/manage.php migrate`; the client preserves that message. Failed credentials/sessions return 401, wrong targets return 403, concurrent approval conflicts return 409 and rate limits return 429. Password recovery invalidates existing sessions. Browser-code approval remains compatible with older clients.

## Errors and announcements

Invalid requests return 400; failed server authentication returns 403; missing matches return 404; unsupported methods return 405; service failures return 503. Responses include a JSON `error` message. Do not treat unavailable services as an empty successful result.

Operators send announcements using POST JSON with provisioned `server_id` and `token` values. Announcements have a 2 MiB request limit. The directory supervisor handles registration heartbeats, durable game delivery and retries. Keep its token private and use HTTPS. See [web services](WEB-SERVICES.md) for setup and [replay verification](ENHANCEMENTS.md) for integrity checks.
