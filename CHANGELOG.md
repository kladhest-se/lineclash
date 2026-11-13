# Changelog

## Unreleased

- Add Sci-fi Theme menu music and an 18-track streaming game library with player controls and complete soundtrack attribution.

- Bind joins to authenticated accounts and enforce gameplay state, board shape and special inventory.
- Add bounded automatic GUI/terminal reconnects, connection-health probes and round-specific slot recovery.
- Add native platform gates, portable tests and Windows build/system-statistics fixes.
- Integrate the archive's release coordinator with external GitHub profiles, tagged releases, checksums and resumable publication.

- Fix invalid status JSON and prevent slow HTTP clients from blocking games.
- Remove authentication use-after-free and secret diagnostic logging.
- Preserve protocol messages under backpressure; reject oversized input safely.
- Make game-result persistence independent of optional SQLite UPDATE syntax.
- Repair isolated integration tests and native-platform CI server artifacts.
- Unify build metadata and track generated-header dependencies.
- Add regression tests, configuration/player/API/deployment guides and complete license text.

## v1.0.0

- Multiplayer rooms with up to 8 players, spectator mode, and team play
- Guideline garbage system with back-to-back bonus, combos, and perfect clear
- Pending garbage queue with counter-attack on line clears
- Garbage targeting: All, Random, and Focused modes
- Hold piece, SRS wall kicks, and configurable lock delay
- 10 specials: add lines, clear lines, nuke, random clear, switch field, block bomb, gravity, quake, column clear
- AI bots with configurable difficulty using all game rules and specials
- Win streaks with fire indicator for consecutive round wins
- Reconnect window — disconnected players hold their slot
- Persistent room settings saved and restored across server restarts
- HTTP stats API on port 31458
- SDL2 GUI client with HiDPI/Retina support
- Terminal CLI client for headless and SSH play
- Admin tool for server management

### Directory services and menu playback

- Restrict the music player to active games; menus and waiting rooms return to the title theme.
- Add the MariaDB-backed web server browser, matches, statistics and public API.
- Add authenticated automatic server registration with durable, idempotent game delivery and expiring listings.
- Document deployment and keep private website configuration out of release archives.

### Discovery, history and playback enhancements

- Native HTTPS server discovery, favourites, recent servers, population filtering and latency sorting.
- Isolate saved credentials to the chosen endpoint and fix a full server-list load overflow.
- Ranked/casual match separation, UTC epochs, persistent gameplay-database identity and linked per-player results.
- Private registration health reports and prompt child-exit detection.
- Bounded public gameplay traces, integrity verification, match/player pages and replay playback/downloads.
- Music ducking and F8/F9/F10 shortcuts, plus expanded multiplayer and web regressions.

### Modern interface and deployment

- Modern slate/mint interface with a bundled antialiased Roboto atlas, proportional text, rounded controls, clearer rows and server-browser empty states.
- Consistent game/dialog typography and a useful solo-arena panel.
- Protected LineClash website refresh script with preview mode, PHP validation, locking and preservation of external configuration/local data.
- Leap 16 / Apache deployment guide for web002, dbs001 and mgt001; verified remote MariaDB TLS configuration.

### Site-local web configuration

- Load `config.inc.php` beside the website index by default; no Apache/PHP environment setting is required.
- Preserve the live configuration during refreshes and save a root-private recovery copy outside the document root before real updates.
- Protect config and backup filenames from HTTP access and update the deployment instructions.

- Website refresh now bootstraps missing config.inc.php from the linted sample, without overwriting existing files; dry runs only preview this action.

### Integrated public community website

- Add live servers, population/match totals, ranked leaders and recent matches directly to the homepage.
- Share responsive navigation, styling and footer across server browsing, leaderboards, match history, statistics, profiles and replays.
- Add server search/open-slot filters, address copying, leaderboard server filters and paginated match history.
- Keep the promotional homepage available during database outages and preserve legacy directory/profile/replay links.
- Cover the integrated pages, escaped names, filters, pagination and outage states in real MariaDB tests.
