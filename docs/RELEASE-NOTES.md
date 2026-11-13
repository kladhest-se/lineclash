# LineClash 1.0.0

- Repair HTTP status JSON and slow-client handling.
- Harden authenticated identity, gameplay state and special inventory checks.
- Automatically reconnect desktop clients within the server grace period.
- Add platform verification, reconnect regressions and deployment documentation.

Native binaries require SDL2 for the GUI and SQLite. Native ZIPs are not standalone installers.
Linux/Windows runtime compatibility must be verified on those operating systems before advertising it.

## Public services and playback

The music player is available during rounds, including while watching after elimination. Menus and waiting rooms play only the title theme. The website adds a MariaDB-backed live server directory, recent matches, win totals and JSON API. The optional dedicated-server supervisor provides authenticated automatic registration and durable retry-safe game uploads. Draws and unranked rounds now also receive completed match records. Deployment and operator provisioning are documented in `docs/WEB-SERVICES.md`.

### Discovery and match-history upgrade

Native public server discovery, favourites/recents and filters now complement the website. Matches include eligibility-based ranked/casual separation, UTC times, linked player statistics, private registration health, and bounded checksum-verified gameplay replays. Database generation IDs survive loss of delivery state. Music adds alert ducking and F8/F9/F10 controls. Website operators must rerun the additive schema migration. See `docs/ENHANCEMENTS.md` for setup and limits.

### Interface and deployment refresh

The GUI now uses a high-resolution bundled Roboto font, a unified slate/mint palette, rounded controls, clearer server rows and useful empty states. A protected website refresh helper and a Leap 16 / Apache deployment guide cover the web002/dbs001/mgt001 topology, including certificate-verified remote MariaDB connections.

### Community website integration

The homepage now includes live server browsing, community totals, ranked leaders and recent matches. Dedicated Servers, Leaderboards, Matches and Statistics pages share the main site's navigation and styling, including player profiles and replays. Search, open-slot filtering, address copying and paginated history are included. Existing database tables and config.inc.php continue to work; no new migration is required for this UI change.
