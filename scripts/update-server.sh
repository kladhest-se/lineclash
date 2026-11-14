#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Build the public source, preserve server data, and install a systemd service.
set -euo pipefail
REPOSITORY=${LINECLASH_SERVER_REPOSITORY:-https://github.com/kladhest-se/lineclash.git}
SOURCE=${LINECLASH_SERVER_SOURCE:-/root/lineclash}
PREFIX=${LINECLASH_SERVER_PREFIX:-/opt/lineclash}
CONFIG=${LINECLASH_SERVER_CONFIG:-/etc/lineclashd.cfg}
STATE=${LINECLASH_SERVER_STATE:-/var/lib/lineclashd}
UNIT_DIR=${LINECLASH_SYSTEMD_DIR:-/etc/systemd/system}
LOCK=${LINECLASH_UPDATE_LOCK:-/run/lock/lineclash-update.lock}
SERVICE=lineclashd.service
[ "$#" -eq 0 ] || { echo "Usage: $0" >&2; exit 2; }
[ "$(id -u)" -eq 0 ] || { echo "Run this script as root." >&2; exit 1; }
[ "$(uname -s)" = Linux ] || { echo "This installer requires Linux and systemd." >&2; exit 1; }
for path in "$SOURCE" "$PREFIX" "$CONFIG" "$STATE" "$UNIT_DIR" "$LOCK"; do
    [[ "$path" =~ ^/[A-Za-z0-9_./-]+$ && "$path" != / && ! -L "$path" ]] || { echo "Use absolute, non-symlink paths without spaces: $path" >&2; exit 1; }
done
for tool in git make gcc python3 tar install getent useradd systemctl flock; do
    command -v "$tool" >/dev/null || { echo "Missing $tool. On openSUSE: zypper install -y git gcc make sqlite3-devel libcurl-devel python3" >&2; exit 1; }
done
exec 9>"$LOCK"
flock -n 9 || { echo "Another server update is running." >&2; exit 1; }
if [ -d "$SOURCE/.git" ]; then
    [ "$(git -C "$SOURCE" branch --show-current)" = main ] || { echo "Checkout must be on main: $SOURCE" >&2; exit 1; }
    git -C "$SOURCE" diff --quiet && git -C "$SOURCE" diff --cached --quiet || { echo "Commit or move local source edits before updating." >&2; exit 1; }
    git -C "$SOURCE" fetch "$REPOSITORY" main
    git -C "$SOURCE" merge --ff-only FETCH_HEAD
elif [ -e "$SOURCE" ]; then
    echo "Existing source directory is not a Git checkout: $SOURCE" >&2; exit 1
else
    git clone --depth 1 --single-branch --branch main "$REPOSITORY" "$SOURCE"
fi
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
# Build a clean copy; ignored binaries from an older build cannot be reused.
git -C "$SOURCE" archive HEAD server shared scripts version.mk get-version.sh | tar -x -C "$WORK"
make -C "$WORK/server" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
"$WORK/server/lineclashd" --version
getent passwd lineclash >/dev/null || useradd --system --user-group --home-dir "$STATE" --no-create-home --shell /usr/sbin/nologin lineclash
install -d -m 750 -o lineclash -g lineclash "$STATE"
if [ ! -e "$CONFIG" ]; then
    install -d -m 755 "$(dirname "$CONFIG")"
    sed -e "s|^db_file = .*|db_file = $STATE/database.sqlite3|" -e "s|^log_file = .*|log_file = $STATE/lineclashd.log|" "$SOURCE/server/lineclashd.cfg.example" > "$WORK/server.cfg"
    install -m 660 -o root -g lineclash "$WORK/server.cfg" "$CONFIG"
fi
[ -f "$CONFIG" ] || { echo "Configuration must be a regular file: $CONFIG" >&2; exit 1; }
# Match the server's key=value format; never source operator configuration as shell.
{
    IFS= read -r DATABASE
    IFS= read -r LOGFILE
    IFS= read -r PORT
} < <(python3 - "$CONFIG" "$STATE" <<'PYCONFIG'
from pathlib import Path
import sys
values={}
for line in Path(sys.argv[1]).read_text().splitlines():
    if line.strip().startswith('#') or '=' not in line: continue
    key,value=line.split('=',1);values[key.strip()]=value.strip()
print(Path(values.get('db_file','/var/lib/lineclashd/database.sqlite3')).resolve())
print(Path(values.get('log_file','/var/log/lineclashd.log')).resolve())
print(values.get('port','31457'))
PYCONFIG
)
[[ "$DATABASE" = "$STATE/"* ]] || { echo "Set an absolute db_file under $STATE before installing the service. Existing configuration was not replaced." >&2; exit 1; }
[[ "$LOGFILE" = "$STATE/"* || "$LOGFILE" = /var/log/lineclashd.log ]] || { echo "Set log_file under $STATE (or /var/log/lineclashd.log). Existing configuration was not replaced." >&2; exit 1; }
[[ "$PORT" =~ ^[0-9]+$ && "$PORT" -ge 1 && "$PORT" -le 65535 ]] || { echo "Invalid configured game port." >&2; exit 1; }
# Do not terminate a separately started daemon or another service on the game port.
if ! systemctl is-active --quiet "$SERVICE"; then
    python3 - "$PORT" <<'PYPORT'
import socket,sys
s=socket.socket()
try:s.bind(('0.0.0.0',int(sys.argv[1])))
except OSError:sys.exit('Game port is already in use. Stop the manually started server (Ctrl+C), then run this updater again.')
finally:s.close()
PYPORT
fi
COMMIT=$(git -C "$SOURCE" rev-parse --short HEAD)
install -d -m 700 "$PREFIX/backups"
BACKUP=$(mktemp -d "$PREFIX/backups/$(date -u +%Y%m%dT%H%M%SZ)-$COMMIT.XXXXXX")
cp -p "$CONFIG" "$BACKUP/server.cfg"
[ ! -f "$UNIT_DIR/$SERVICE" ] || cp -p "$UNIT_DIR/$SERVICE" "$BACKUP/$SERVICE"
for binary in lineclashd lineclashd-tool; do
    [ ! -f "$PREFIX/bin/$binary" ] || cp -p "$PREFIX/bin/$binary" "$BACKUP/$binary"
done
systemctl stop "$SERVICE" 2>/dev/null || {
    if systemctl is-active --quiet "$SERVICE"; then
        echo "Could not stop the running service; update aborted." >&2; exit 1
    fi
}
if [ -f "$DATABASE" ]; then
    python3 - "$DATABASE" "$BACKUP/database.sqlite3" <<'PYBACKUP'
import sqlite3,sys
with sqlite3.connect(sys.argv[1]) as source,sqlite3.connect(sys.argv[2]) as backup:source.backup(backup)
PYBACKUP
    chmod 600 "$BACKUP/database.sqlite3"
    if [ -f "$DATABASE.directory-key" ]; then
        cp -p "$DATABASE.directory-key" "$BACKUP/database.sqlite3.directory-key"
        chmod 600 "$BACKUP/database.sqlite3.directory-key"
    fi
fi
# These are the service's own private state files, including WAL and directory key.
chown -R --no-dereference lineclash:lineclash "$STATE"
chgrp lineclash "$CONFIG"; chmod g+rw,o-rwx "$CONFIG"
if [ "$LOGFILE" = /var/log/lineclashd.log ]; then
    touch "$LOGFILE"; chown lineclash:lineclash "$LOGFILE"; chmod 640 "$LOGFILE"
fi
install -d -m 755 "$PREFIX/bin" "$PREFIX/scripts" "$UNIT_DIR"
for binary in lineclashd lineclashd-tool; do
    install -m 755 "$WORK/server/$binary" "$PREFIX/bin/$binary.new"
    mv -f "$PREFIX/bin/$binary.new" "$PREFIX/bin/$binary"
done
install -m 755 "$WORK/scripts/server_directory.py" "$PREFIX/scripts/server_directory.py"
install -m 644 "$SOURCE/LICENSE.md" "$PREFIX/LICENSE.md"
cat > "$WORK/$SERVICE" <<UNIT
[Unit]
Description=LineClash dedicated server
Wants=network-online.target
After=network-online.target

[Service]
Type=simple
User=lineclash
Group=lineclash
WorkingDirectory=$STATE
ExecStart=$PREFIX/bin/lineclashd --config $CONFIG
Restart=on-failure
RestartSec=5
TimeoutStopSec=20
UMask=0077
StandardInput=null
NoNewPrivileges=true
PrivateTmp=true
ProtectHome=true
ProtectSystem=strict
ReadWritePaths=$STATE $CONFIG $LOGFILE

[Install]
WantedBy=multi-user.target
UNIT
# ReadWritePaths requires its file targets to exist before service startup.
mkdir -p "$(dirname "$LOGFILE")"
[ -e "$LOGFILE" ] || install -m 640 -o lineclash -g lineclash /dev/null "$LOGFILE"
install -m 644 "$WORK/$SERVICE" "$UNIT_DIR/$SERVICE"
systemctl daemon-reload
systemctl enable "$SERVICE"
systemctl restart "$SERVICE"
sleep 2
if ! systemctl is-active --quiet "$SERVICE"; then
    echo "Server did not stay running. Backup: $BACKUP" >&2
    journalctl -u "$SERVICE" -n 30 --no-pager >&2
    exit 1
fi
printf 'Updated LineClash (%s). Configuration and data retained.
Backup: %s
' "$COMMIT" "$BACKUP"
printf 'Status: systemctl status %s
Logs: journalctl -u %s -f
' "$SERVICE" "$SERVICE"
