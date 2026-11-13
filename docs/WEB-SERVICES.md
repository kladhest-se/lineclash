# Server directory and MariaDB statistics

The OpenFP archive informed the announce/expiry/API pattern and dark server-card presentation. This implementation is original LineClash code. It does not execute archive instructions or reuse its tank-specific services.

## Components

* `public-web/directory/`: live server browser, recent matches, aggregate counts.
* `public-web/api/`: public GET JSON (`action=servers`, `matches`, `statistics`, `leaders`, `match`, `player`, `replay`), authenticated POST announcements. Matches accept `offset=0`, then 50, 100, etc. Leaders include eligible completed competitive match wins, grouped by server and nickname, not globally verified accounts.
* `scripts/server_directory.py`: standard-library Python 3.10+ supervisor that starts the dedicated server and automatically announces it every 60 seconds. Network delivery runs in a separate process from gameplay; failures retry without blocking the server.
* Server gameplay/authentication retain local SQLite. The public website uses **MariaDB through PDO MySQL**, with no SQLite fallback. No game-account hashes, IP addresses, chat, or moderation records are uploaded.

A server must be provisioned by the website operator. Its token authenticates announcements, while its public host/port are operator-controlled. Listings are server-reported, not independently reachability-checked or cheat-proof. Offline listings expire after 180 seconds; match history remains. Stopping the server therefore requires no successful final network request.

## Website setup

Requirements: PHP 8.1+ with `pdo_mysql`, MariaDB 10.5+, HTTPS reverse proxy. Point the document root at `public-web`. Do not expose the repository root. Create a UTF-8 database and a dedicated database account. For example, in an administrative MariaDB session:

```sql
CREATE DATABASE lineclash CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'lineclash'@'localhost' IDENTIFIED BY 'replace-with-a-long-password';
GRANT SELECT, INSERT, UPDATE, CREATE, ALTER, INDEX, REFERENCES ON lineclash.* TO 'lineclash'@'localhost';
```

Copy `public-web/config.example.php` to `public-web/config.inc.php` (on the deployed site, `/srv/www/vhosts/lineclash.kladhest.se/config.inc.php`). Set the DSN, user, and password there and restrict read access to administrators and the web-service account. The file loads automatically for PHP and CLI administration. The supplied Apache rules deny HTTP access. `LINECLASH_WEB_CONFIG` remains an optional override for tests or custom installations; remove old environment overrides when switching to the default site-local file. Use certificate-verified TLS for the remote database; see the concrete deployment guide.

```sh
php public-web/bin/manage.php migrate
php public-web/bin/manage.php server main game.example.org 31457 'Main server'
```

The second command creates or updates the operator-approved address and prints a new token once. Re-running it rotates that server's token and clears its live announcement. Keep the token private; give each server its own ID/token. Never put it in a URL or the web configuration example. After migrations, the runtime DB account can be reduced to SELECT/INSERT/UPDATE; use a separate migration account for later upgrades.

Apache needs `AllowOverride` for `.htaccess` and mod_rewrite. For nginx, deny `/inc/`, `/bin/`, and `/config*.php` before the PHP location; disable directory listings. Only `index.php`, `directory/index.php`, and `api/index.php` need to be executable public entry points. Example:

```nginx
location ~ ^/(inc|bin)(/|$) { deny all; }
location ~ ^/config[^/]*\.php$ { deny all; }
```

For development only, use `LINECLASH_WEB_CONFIG=/private/path/web.php php -S 127.0.0.1:8080 -t public-web public-web/dev-router.php`. Do not use PHP's development server on public interfaces.

## Run an announcing dedicated server

Build normally (`make server`). Configure the server's `db_file` explicitly. The registrar's `--database` must point to that same file. Use absolute paths, a private writable state directory, and a service manager that restarts the supervisor after failures:

```sh
export LINECLASH_DIRECTORY_TOKEN='token-printed-by-the-admin-command'
python3 scripts/server_directory.py \
  --url https://games.example.org/api/ --server-id main \
  --database /var/lib/lineclashd/database.sqlite3 \
  --state /var/lib/lineclashd/directory.sqlite3 \
  -- /absolute/path/to/lineclashd --config /etc/lineclashd/server.cfg
```

Enable the server’s `status_api` and optionally add `--status-url http://127.0.0.1:31458/status` (use your configured `status_port`) to include live player/room counts. Only those public counts are forwarded; the full local API response is never uploaded. Without it the browser shows unknown counts. Keep the local status API behind your firewall.

The token comes from the supervisor's environment, never command arguments or logs. SIGINT/SIGTERM stops its child. HTTP attempts have five-second timeouts; HTTPS certificates are verified and redirects are refused. `--allow-local-http` permits only loopback HTTP for tests. Interval range is 15–120 seconds; keep the website TTL above the interval.

For Linux systemd, run the above command as `ExecStart`, with `WorkingDirectory` set to the project root, `User=lineclash`, `Restart=on-failure`, and a root-readable `EnvironmentFile` containing the token. Use `StateDirectory=lineclashd` and point both database paths there. On macOS use launchd, or run it in Terminal for local testing. On Windows use the same Python script with absolute Windows paths to `lineclashd.exe` and a service wrapper. Do not run a second dedicated server against the same SQLite database.

Preserve both gameplay and registrar databases across backups. The gameplay database now owns the generation ID (`service_metadata`); resetting the registrar state safely retries the same remote IDs. A replacement gameplay database automatically uses a new generation, avoiding numeric game-ID collisions even if the old registrar state remains. One-time migration adopts legacy delivery identity when needed. Never share state files between servers or run two registrars for the same server. Changing URL/ID creates a separate delivery namespace and replays history there. See [upgrade details](ENHANCEMENTS.md).

All completed games, including solo/bot rounds and draws, are uploaded in bounded batches (up to 20 games, reduced further when replays are large). Acknowledgements are persisted only after a successful MariaDB transaction. The remote unique key `(server_id, database_id, game_id)` makes retries safe, including an older game that finishes later. Unfinished/crash-abandoned games are excluded. Existing completed history uploads on first use. New matches include UTC epochs, competitive eligibility, linked human player results and bounded public gameplay traces. Legacy timestamps/ranking remain unknown/casual. Public ranked leaderboards exclude manually stopped rounds and solo/bot matches.

Set `LINECLASH_DIRECTORY_API` to the website's full HTTPS `/api/` URL and use **Find servers** in the native GUI. Favourites, recent-played and open-slot filters are available; new discoveries prompt for credentials. Set `LINECLASH_DIRECTORY_URL` to the HTTPS `/directory/` URL for the optional website shortcut. Neither discovery nor the website shares game passwords. Deployment/provisioning is required before public listings appear. See [controls, health reports, replay playback and upgrades](ENHANCEMENTS.md).

## API and verification

POST JSON fields: `server_id`, `token`, 32-hex `database_id`, `games` array, optional `status` with integer `players`, `rooms`, `users`, `capacity` only. Each game contains integer `game_id`, `room_id`, `player_count`, and strings `started_at`, `ended_at`, `winner_nick`, `winner_team`. Body limit 2 MiB, maximum 100 games accepted by the API (the registrar sends smaller bounded batches). New games additionally include `started_epoch`, `ended_epoch`, `ranked`, `end_reason`, human `players` counters, and optional `replay_gzip`, `replay_sha256`, `replay_truncated`. Replay decompression is bounded to 1.5 MiB/2,048 events. A successful response is `{"ok":true,"ack":[1,2]}` in submitted order. Unknown tokens return 403; malformed requests 400; database outages 503. Duplicate IDs retain the first accepted result. Public APIs never return authentication tokens or account password hashes; replay checksums are intentionally public. Use rate limiting at the reverse proxy, especially for POST.

Run `python3 tests/test_directory.py` for retry, authentication boundary, and out-of-order completion checks. Run `LINECLASH_WEB_CONFIG=/private/test-config.php python3 tests/test_web_mariadb.py` against an **empty disposable MariaDB database** for real API integration. It provisions and uses only its uniquely named test server. `make web-test` additionally builds the dedicated server and checks the complete supervisor → local SQLite → PHP → MariaDB flow and child shutdown. Run PHP lint and the existing native test suite as usual. See `public-web/config.example.php` for configuration keys.

CI includes `test:web-directory` with a disposable MariaDB 10.11 service and PHP 8.3. It runs PHP lint, registrar regressions and the full real-server upload checks. Local integration was also verified with MariaDB 13.0.2 on macOS; the CI job must run on its configured Docker runner to verify the pinned Linux combination.

A concrete setup for web002, dbs001 and mgt001 is in [the Kladhest deployment guide](DEPLOYMENT-KLADHEST.md), including remote MariaDB TLS, Apache configuration and the protected refresh script.

## Main website integration

The Home page presents the game and downloads. Server listings belong to `/servers/`; population, match totals, ranked leaders and recent results belong to `/statistics/`. Top navigation and a shared responsive theme connect these pages with filtered `/leaderboards/` and `/matches/` views, including player profiles and replay pages. `/directory/` remains a compatible combined view. Server search/open-slot filters, address copying, server-scoped leaderboards and paginated ranked/casual history are available without inspecting JSON. These pages use the existing database schema; no migration is added by the interface integration.

## Public website navigation and release downloads

The landing page introduces the game with screenshots, features, licensing, and a prominent **Get the game** section (`/#get`). It does not query MariaDB. `/servers/` contains live server search and addresses. `/statistics/` brings together community totals, server activity, ranked leaderboards, and recent matches; full filtered histories remain at `/leaderboards/` and `/matches/`. Player profiles and replay URLs are preserved. Falling blocks are shared across all public pages, respect reduced-motion preferences, and pause in hidden tabs.

The browser checks the public GitHub `kladhest-se/lineclash` latest-release API without credentials. GitHub returns only published stable releases; tags alone, drafts, and prereleases do not create downloads here. A 404 displays “Not released yet”; outages and rate limits display an unavailable state with a retry and direct GitHub link. Only uploaded package assets on the expected repository download path are linked; website packages, checksums, and provenance files are excluded. Asset names identify the actual platform; the page never invents unavailable installers. JavaScript-disabled visitors can follow the releases link or source-build instructions. Music and font licenses remain documented in `CREDITS.md`; game code is GPL version 3 or later.

After publishing a tagged GitHub release with package assets, the page picks it up on the next load. No website configuration, access token, database migration, or rebuild is required. Deploy these website changes with the normal refresh script; existing `config.inc.php` is preserved.

Run `node tests/test_web_downloads.js` for release lookup and asset validation checks, and `make web-test` for website/database integration.

The main navigation is **Home, Servers, Statistics, Documentation, Source**. Screenshots and **Get the game** are sections on Home (`/#screenshots` and `/#get`), not separate pages. The persistent Get the game button returns there from every page. `/documentation/` provides a quick start, desktop controls and links to maintained repository guides; `/source/` groups repositories, contributions, source checkout, platform build instructions (`/source/#build`), licensing and credits. These public pages work without a configured database and use the shared block animation.

Home introduces the game in this order: hero, about, features, specials, screenshots, then Get the game. Server browsing and statistics remain on their dedicated pages; build instructions live on Source. Download buttons in the header and hero jump directly to the lower download section.

The Home design uses a text introduction, numbered sections, and unboxed feature rows with subtle separators. Screenshots are grouped in their own Home gallery, which opens a keyboard-accessible dialog with fit-to-screen, actual-size and original-image options. The hero contains no screenshot. Screenshots and release links retain the shared colors and animated block background; mobile layouts stack into one column.

Home uses unboxed text features and a compact section index. Its dedicated screenshot gallery shows four previews initially, with three additional views inside a native collapsible “More screenshots” section. Music and credits are included in that gallery; separate music and licensing promotional sections are omitted. Licensing and credits remain on Source. Screenshot boards use independent deterministic sequences of real dropped pieces and different piece counts; the renderer verifies that no two sample boards match. These are illustrative native UI previews, not recorded match results.

To regenerate arena, AI, rules and room screenshots after building the GUI, compile `scripts/render_web_previews.c` with the same objects/libraries as the music UI test (excluding `render_sdl.o`), then run it. It writes `/tmp/lineclash-web-*.bmp`; convert these to PNG in `public-web/assets/screenshots/` and regenerate their 360px thumbnail variants.

Home omits static platform labels; published GitHub release assets supply the available downloads in Get the game. The introduction is a single paragraph; screenshots include server discovery in the visible gallery. Platform build instructions remain on Source.

The footer contains only Credits, Privacy, API and GitHub. `/credits/` lists soundtrack attributions and component notices; `/privacy/` describes public community data and third-party requests; `/api/docs/` documents the community API. These pages share the animated background and work without MariaDB. Website music-credit metadata in `public-web/inc/music-credits.json` is derived from `music/tracks.json`; keep it in sync when updating the soundtrack.

## Website documentation publishing

`/documentation/` is limited to playing, game-server configuration, discovery and replay usage. Build and development guides are on `/source/`; both API references are on `/api/docs/`. Website deployment, web-service operations, release workflows, verification and release notes stay in repository documentation, rather than being published as website chapters. Soundtrack attribution is on Credits.

The documentation overview links to focused guides for gameplay, clients and controls, and server operations. These three guides, the Source guide and the API guide are generated from selected canonical Markdown content into `public-web/inc/*-documentation.html`. Install `scripts/requirements-docs.txt`, then run `python3 scripts/build_web_docs.py`; `--check` detects stale output. No Markdown package is needed on the web host. Repository-only operator and development material remains intact.

## Native default registration

Deploy the updated `public-web` API before expecting new native servers to appear. POST `action=public-register` accepts a server-generated 64-character random token, name, game port, players and capacity. The API derives an `auto-` ID from its observed source address and port; subsequent announcements require the same token. Only token hashes are stored. Automatic directory identities cannot use the trusted game-results upload endpoint. No new SQL migration is needed for this endpoint.

Registration rejects non-public peer addresses by default. A disposable local test config may set `allow_local_registration => true`; keep that disabled on public hosts. Do not trust X-Forwarded-For from arbitrary requests. A reverse proxy requires deliberate trusted client-address configuration on Apache before registration can record the real game host address.

The native worker uses libcurl on macOS/Linux and WinHTTP on Windows. Linux server builds therefore also need libcurl development headers. HTTP failures are logged and retried without blocking gameplay. Protect `*.directory-key` files; source and website packaging excludes them.

### HAProxy client addresses

When Apache is behind HAProxy, set `trusted_proxies` in the site's existing `config.inc.php` to the exact HAProxy source IP, for example `['172.16.1.1']` on the private deployment. Leave it empty for direct access. Enable HAProxy's X-Forwarded-For support (`option forwardfor`); it must append the real source address or replace an untrusted inbound header. Never configure it to preserve a caller-supplied header without appending the observed address. The API walks the chain right to left and accepts forwarded headers only from configured proxy peers.

A forwarded RFC1918/LAN address remains private and is rejected for an internet listing. This is intentional: proxy trust does not make a private endpoint internet reachable. Internet clients need a publicly reachable game address and the appropriate port forwarding.

### Automatically mapping local servers to the shared public IP

In `config.inc.php`, `local_public => ['from' => ['172.16.1.100'], 'address' => 'auto']` maps only the listed observed client addresses to the website host's external IPv4. Configure trusted proxies first so a proxy's address does not stand in for unrelated clients. An HTTPS lookup to the fixed ipify endpoint is cached for ten minutes in a private temporary directory. Failures leave the server unlisted until retry; they never publish a private IP. A fixed public IP can replace `auto` if desired.

Use this only when the web host and game server share the same public egress IP. Forward the game TCP port on that public address to the correct LAN game host. Internal players can use Saved servers with the LAN address, or NAT reflection can make the public-IP listing reachable inside the network. Public-IP detection does not create NAT/firewall rules. With multiple internal servers, each needs its own externally forwarded game port.

## Server-owned public address and verified endpoints

Current native servers send `public_ip`, detected on the game server itself, with each registration. The API accepts only a public IP, then checks a fresh `directory-proof NONCE` HMAC challenge over the announced TCP game endpoint. This supports a private registration source without trusting its claimed public endpoint blindly. Public game-result uploads still require separate operator approval. Peer-based `local_public` mapping remains only for older servers; remove that mapping when all servers use the new protocol. Trusted proxy settings can remain for accurate source-based rate limiting.

Run `php bin/manage.php migrate` after refreshing this version: the additive `directory_registration_limits` table bounds registration requests to twenty per observed source per minute. Callback verification has a strict time/read budget and only targets numeric public addresses. The website must reach the advertised endpoint, including through NAT reflection when it shares the game server's LAN. Configure neither game-port TLS nor an HTTP reverse proxy for this callback; it uses the native TCP protocol.

## Central-account deployment

Refresh public-web and run `php bin/manage.php migrate` before updating servers and clients. This creates `accounts`, `account_devices`, `account_sessions` and the session-version upgrade. The website DB account needs SELECT, INSERT and UPDATE on these tables. Browser sessions require PHP's writable session directory, HTTPS, secure/HttpOnly/SameSite cookies and CSRF validation. Native profile sign-in uses a fixed HTTPS API endpoint with certificate and hostname verification. Browser-code approval remains available for older clients.

Only live, verified server directory keys can create/poll connection requests. Each request is bound to that server and a random connection nonce, lasts five minutes and can be redeemed once. Browser approval shows the registered server endpoint. Central signing-in does not grant a server administrative role or authorize statistics uploads. Guest/local server accounts remain separate. Keep a local administrator account for offline administration tools.

Old account_devices rows can be removed periodically by an administrator after expiry. Keep the accounts table in protected backups, including its password/recovery hashes. No real central account or production database was created by the local test suite.

### Client sessions and password policy

Native clients receive random twelve-hour session tokens; the database stores only their SHA-256 hashes. Logout revokes the corresponding session. Password recovery increments the account version, invalidating all earlier browser and native sessions. A client uses its session only over HTTPS to approve a fresh challenge for the selected verified game endpoint. Passwords and reusable sessions never go to game servers. Login and join endpoints have per-source limits; native login also has a per-handle limit.

Set the minimum length for **new and reset central passwords** in the existing site configuration:

```php
'account_password_min_length' => 6,
```

The default is 12 Unicode characters. The supported range is 1–72; invalid configuration values retain the default. The 72-byte password limit still applies. Changing the minimum does not prevent existing accounts from signing in. Website forms show the current requirement. The refresh script preserves `config.inc.php`, so add this option to your live configuration yourself.

The database account needs SELECT, INSERT and UPDATE on `account_sessions`; logout does not require DELETE. Administrators may periodically delete expired/revoked sessions and expired `account_devices` rows. Back up account records privately. Deploy the website and run migrations first, then update dedicated servers and clients. No real production accounts are created by the local tests.
