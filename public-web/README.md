# LineClash public website

The promotional page and public directory/API are deployed together. The directory and statistics require PHP 8.1+ with PDO MySQL and MariaDB 10.5+. There is no SQLite website backend.

See [deployment and API documentation](../docs/WEB-SERVICES.md) for schema migration, server tokens, announcement setup and tests. Store the real configuration as `config.inc.php` beside `index.php`; it loads automatically. `LINECLASH_WEB_CONFIG` remains an optional override. `config.example.php` is only a template. Protect `inc/`, `bin/` and configuration filenames at the web server.

## Deployment checklist

1. Serve this directory over HTTPS using PHP 8.1+ / PDO MySQL.
2. Create a MariaDB UTF-8 database and dedicated account. Copy `config.example.php` to `config.inc.php` beside `index.php` and fill in its values. Keep it readable only by the administrator and web-service account; deny HTTP access with the supplied Apache rules.
3. Run `php bin/manage.php migrate`, then `php bin/manage.php server ID HOST PORT NAME`; preserve the printed token privately on that dedicated server.
4. Deny HTTP access to `inc/`, `bin/` and `config*.php`. Apache rules and a development-only router are included; configure equivalent nginx rules.
5. Run `scripts/server_directory.py` from the complete LineClash distribution with that ID/token and this website’s `/api/` URL. Full command examples are in the distribution’s `docs/WEB-SERVICES.md`.

The primary public browser is `/servers/`; `/directory/` remains a combined community view. GET `/api/?action=servers|matches|statistics|leaders` returns JSON. Unannounced servers expire after 180 seconds. Local game databases remain SQLite; all web statistics are stored in MariaDB.

## Match-history upgrade

Rerun `php bin/manage.php migrate` before using the upgraded registrar. New matches include UTC epochs, ranked/casual eligibility, per-player results and compressed/checksummed public gameplay traces. GET API actions now also include `match`, `player`, `replay`; match/replay keys are `server`, `database`, `game`. Leaders count eligible ranked wins only. The directory has match/player pages and a seeking replay viewer. Native clients use `?action=servers&format=text`. Requests are limited to 2 MiB; replay decompression is separately bounded. Full deployment/upgrade and verification details are in the distribution's `docs/ENHANCEMENTS.md`.

The refresh helper automatically copies `config.example.php` to `config.inc.php` when missing, with mode 640. Existing configuration is preserved and privately backed up. Edit new sample values before running migrations; dry runs create no configuration file.

## Integrated community website

The homepage now includes live server cards, player/match totals, ranked leaders and recent matches. Shared navigation links to `/servers/`, `/leaderboards/`, `/matches/` and `/statistics/`. Server pages support search, open-slot filtering and address copying; leaderboard/match pages support server filters and pagination. Player histories and replays use the same site header, footer and theme. `/directory/` remains available as a combined community view for existing links.

Data is read directly from the existing MariaDB tables; this interface change requires no new migration. If the database is unavailable, the promotional homepage remains available and displays an explicit community-data notice; full data pages return 503 with the shared navigation. Unknown population is shown as unknown, not zero. No example servers or fabricated statistics are rendered in production.

Run `make web-test` with a disposable `LINECLASH_WEB_CONFIG` for real database, integration and outage checks. The test fixtures are confined to that disposable database. Refresh the website after publishing the source updates; `config.inc.php` is still preserved by the update helper.
