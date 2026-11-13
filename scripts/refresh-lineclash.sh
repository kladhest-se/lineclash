#!/bin/sh
# SPDX-License-Identifier: MIT
# Refresh LineClash's public website without replacing operator configuration.
set -eu
SITE=${LINECLASH_SITE:-/srv/www/vhosts/lineclash.kladhest.se}
REPOSITORY=${LINECLASH_REPOSITORY:-https://github.com/kladhest-se/lineclash.git}
BRANCH=${LINECLASH_BRANCH:-main}
OWNER=${LINECLASH_OWNER:-wwwrun:wwwrun}
WEB_CONFIG=$SITE/config.inc.php
DRY_RUN=
case "${1-}" in
    '') ;;
    --dry-run) DRY_RUN=--dry-run ;;
    *) echo "Usage: $0 [--dry-run]" >&2; exit 1 ;;
esac
[ "$#" -le 1 ] || { echo "Usage: $0 [--dry-run]" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "Run as root." >&2; exit 1; }
for tool in git rsync php flock; do
    command -v "$tool" >/dev/null 2>&1 || { echo "Required command is missing: $tool" >&2; exit 1; }
done
case "$SITE" in /*) ;; *) echo "SITE must be an absolute path." >&2; exit 1;; esac
[ ! -L "$SITE" ] || { echo "Refusing a symlink document root." >&2; exit 1; }
[ ! -L "$WEB_CONFIG" ] || { echo "Refusing a symlink configuration file." >&2; exit 1; }
if [ -e "$WEB_CONFIG" ] && [ ! -f "$WEB_CONFIG" ]; then
    echo "Configuration path exists but is not a regular file: $WEB_CONFIG" >&2; exit 1
fi
[ -d "$(dirname "$SITE")" ] || { echo "Site parent directory is missing." >&2; exit 1; }
exec 9>"${SITE}.refresh.lock"
flock -n 9 || { echo "Another website refresh is running." >&2; exit 1; }
TEMP=$(mktemp -d)
CONFIG_TEMP=
trap '[ -z "$CONFIG_TEMP" ] || rm -f "$CONFIG_TEMP"; rm -rf "$TEMP"' 0
trap 'exit 1' HUP INT TERM
git clone --quiet --depth 1 --single-branch --branch "$BRANCH" "$REPOSITORY" "$TEMP/repository"
SOURCE=$TEMP/repository/public-web
for file in index.php config.example.php inc/service.php inc/schema.sql inc/site.php inc/community.php inc/community-page.php assets/site.css assets/community.js assets/blocks.js assets/downloads.js assets/screenshots.js assets/screenshots/game.png assets/screenshots/servers.png bin/manage.php api/index.php directory/index.php servers/index.php leaderboards/index.php matches/index.php statistics/index.php documentation/index.php source/index.php credits/index.php privacy/index.php api/docs/index.php inc/music-credits.json inc/documentation.html inc/source-documentation.html inc/api-documentation.html .htaccess; do
    [ -f "$SOURCE/$file" ] || { echo "public-web/$file is missing; site left unchanged." >&2; exit 1; }
done
[ -z "$(find "$SOURCE" -type l -print -quit)" ] || { echo "Refusing symlinked website inputs." >&2; exit 1; }
# Parse new PHP without loading the live config or touching the database.
find "$SOURCE" -type f -name '*.php' -exec sh -c '
    for file do php -l "$file" >/dev/null || exit 1; done
' sh {} +
chown -R "$OWNER" "$SOURCE"
find "$SOURCE" -type d -exec chmod 750 {} +
find "$SOURCE" -type f -exec chmod 640 {} +
# Save a private recovery copy outside the document root before a real refresh.
BACKUP=
if [ -z "$DRY_RUN" ] && [ -f "$WEB_CONFIG" ]; then
    BACKUP_DIR=${SITE}.private-backups
    [ ! -L "$BACKUP_DIR" ] || { echo "Refusing a symlink backup directory." >&2; exit 1; }
    (umask 077; mkdir -p "$BACKUP_DIR")
    chown 0:0 "$BACKUP_DIR"
    chmod 700 "$BACKUP_DIR"
    BACKUP=$(mktemp "$BACKUP_DIR/config.inc.php.$(date -u +%Y%m%dT%H%M%SZ).XXXXXX")
    cp -p "$WEB_CONFIG" "$BACKUP"
    chown 0:0 "$BACKUP"
    chmod 600 "$BACKUP"
fi
# Excluded live files keep their contents, owner and permissions.
rsync -a --checksum --safe-links --delete-delay --delay-updates \
    --exclude='/config.php' --exclude='/config.inc.php' \
    --exclude='/.env' --exclude='*.local.php' \
    --exclude='/data/' --exclude='/uploads/' --exclude='/assets/releases/' \
    ${DRY_RUN:+"$DRY_RUN"} --itemize-changes "$SOURCE/" "$SITE/"
CREATED_CONFIG=
if [ ! -e "$WEB_CONFIG" ]; then
    if [ -n "$DRY_RUN" ]; then
        echo "Would create $WEB_CONFIG from config.example.php; fill in the database settings afterward."
    else
        # Stage privately and link without replacing a config created in the meantime.
        CONFIG_TEMP=$(mktemp "$SITE/.config.inc.php.XXXXXX")
        cp "$SOURCE/config.example.php" "$CONFIG_TEMP"
        chown "$OWNER" "$CONFIG_TEMP"
        chmod 640 "$CONFIG_TEMP"
        if ln "$CONFIG_TEMP" "$WEB_CONFIG"; then
            CREATED_CONFIG=yes
        elif [ ! -f "$WEB_CONFIG" ] || [ -L "$WEB_CONFIG" ]; then
            echo "Cannot safely create $WEB_CONFIG" >&2; exit 1
        fi
        rm -f "$CONFIG_TEMP"; CONFIG_TEMP=
    fi
fi
if [ -n "$DRY_RUN" ]; then
    echo "Preview only; $SITE and its database were not changed."
else
    echo "Updated $SITE; preserved local data/downloads."
    if [ -n "$CREATED_CONFIG" ]; then
        echo "Created $WEB_CONFIG from the sample. Edit its database settings before running migrations."
    else
        echo "Preserved existing config.inc.php."
    fi
    [ -z "$BACKUP" ] || echo "Private configuration backup: $BACKUP"
    echo "Next: php $SITE/bin/manage.php migrate"
    echo "Migrations need the installation DB privileges; config.inc.php loads automatically."
fi
