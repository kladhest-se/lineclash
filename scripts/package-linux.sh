#!/usr/bin/env bash
# scripts/package-linux.sh — Package Linux binaries into tar.gz + RPM + DEB
# Requires: clients/gui/lineclash + server/lineclashd already built (native)
# Usage: ./scripts/package-linux.sh [--rpm] [--opensuse] [--deb] [--all]
source "$(dirname "$0")/common.sh"

[[ "$OS" == "Linux" ]] || { fail "Linux packaging must run on Linux"; }

# ── Parse flags ───────────────────────────────────────────────
DO_RPM=0; DO_LEAP15=0; DO_DEB=0
for arg in "$@"; do
    case "$arg" in
        --rpm)            DO_RPM=1 ;;
        --opensuse)       DO_LEAP15=1; DO_LEAP16=1 ;;
        --opensuse-leap15) DO_LEAP15=1 ;;
        --deb)            DO_DEB=1 ;;
        --all)            DO_RPM=1; DO_LEAP15=1; DO_DEB=1 ;;
    esac
done

APP_NAME="LineClash"
RV="$(echo "$VERSION" | sed 's/[^0-9.].*$//' | sed 's/^\.\|\.$//;s/^$/0.9/')"
RA="x86_64"      # Linux packaging is x86_64 only
RA_RPM="x86_64"

[[ -f "$CLIENT_DIR/lineclash" ]] || fail "client/lineclash not found — run build-client.sh first"
[[ -f "$SERVER_DIR/lineclashd" ]] || fail "server/lineclashd not found — run build-server.sh first"

# ── Shared staging tree for RPM/DEB ──────────────────────────
_build_staging() {
    local rr="$1"
    mkdir -p "$rr/usr/games" "$rr/usr/share/lineclash" \
             "$rr/usr/share/applications" "$rr/usr/share/man/man6" \
             "$rr/usr/sbin" "$rr/usr/bin"

    mkdir -p "$rr/usr/share/doc/lineclash"
    cp "$ROOT/LICENSE.md" "$ROOT/README.md" "$rr/usr/share/doc/lineclash/"
    cp -R "$ROOT/docs" "$rr/usr/share/doc/lineclash/docs"
    cp "$ROOT/server/lineclashd.cfg.example" "$rr/usr/share/doc/lineclash/"
    cp "$CLIENT_DIR/lineclash"    "$rr/usr/games/lineclash";        chmod 755 "$rr/usr/games/lineclash"
    cp "$SERVER_DIR/lineclashd"   "$rr/usr/sbin/lineclashd";        chmod 755 "$rr/usr/sbin/lineclashd"
    [[ -f "$SERVER_DIR/lineclashd-tool" ]] && {
        cp "$SERVER_DIR/lineclashd-tool" "$rr/usr/bin/lineclashd-tool"; chmod 755 "$rr/usr/bin/lineclashd-tool"; }

    cp -R "$ROOT/music" "$rr/usr/share/lineclash/music"
    cp "$ROOT/CREDITS.md" "$rr/usr/share/doc/lineclash/CREDITS.md"
    cp "$ROOT/docs/licenses/Roboto-OFL.txt" "$rr/usr/share/doc/lineclash/Roboto-OFL.txt"

    cat > "$rr/usr/share/applications/lineclash.desktop" << DESK
[Desktop Entry]
Name=LineClash
GenericName=Multiplayer Falling Blocks
Comment=Multiplayer Falling Blocks in the LineClash tradition
Exec=/usr/games/lineclash
Icon=lineclash
Terminal=false
Type=Application
Categories=Game;ArcadeGame;
StartupNotify=false
DESK

    printf '.TH TETRINETZ 6 "2025"\n.SH NAME\nlineclash \\- multiplayer falling blocks client\n' \
        | gzip -c > "$rr/usr/share/man/man6/lineclash.6.gz"
}

# ── Fedora / RHEL RPM ─────────────────────────────────────────
if [[ $DO_RPM -eq 1 ]]; then
    header "Building Fedora/RHEL RPM"
    command -v rpmbuild &>/dev/null || sudo dnf install -y rpm-build 2>/dev/null || \
        sudo zypper install -y rpm-build 2>/dev/null || fail "rpmbuild not found"

    TOP="$(mktemp -d /tmp/lineclash-rpm-XXXXXX)"
    RR="$TOP/root"
    mkdir -p "$TOP"/{BUILD,RPMS,SOURCES,SPECS,SRPMS} "$RR"
    _build_staging "$RR"

    MOD_FILES="/usr/share/lineclash/music"
    EXTRA=""
    [[ -f "$RR/usr/sbin/lineclashd" ]]      && EXTRA="$EXTRA\n/usr/sbin/lineclashd"
    [[ -f "$RR/usr/bin/lineclashd-tool" ]]  && EXTRA="$EXTRA\n/usr/bin/lineclashd-tool"

    cat > "$TOP/SPECS/lineclash.spec" << SPEC
Name:        lineclash
Version:     ${RV}
Release:     1%{?dist}
Summary:     Multiplayer Falling Blocks (LineClash protocol)
License:     GPL-3.0-or-later
URL:         https://github.com/frossmant/lineclash
%description
LineClash — a cross-platform multiplayer falling blocks clone in the LineClash
tradition. Play with friends, bots, or spectate in real time.
Requires the SDL2 and libcurl runtimes used for this build.
%install
cp -a ${RR}/. %{buildroot}/
%files
%defattr(-,root,root,-)
/usr/share/doc/lineclash
/usr/games/lineclash
%dir /usr/share/lineclash
${MOD_FILES}
/usr/share/applications/lineclash.desktop
/usr/share/man/man6/lineclash.6.gz
$(printf "$EXTRA")
%changelog
* $(date '+%a %b %d %Y') frossmant <frossmant@github.com> - ${RV}-1
- Release ${RV}
SPEC

    rpmbuild --define "_topdir $TOP" --define "_builddir $TOP/BUILD" \
        --define "_rpmdir $TOP/RPMS"  --define "_sourcedir $TOP/SOURCES" \
        --define "_specdir $TOP/SPECS" --define "_srcrpmdir $TOP/SRPMS" \
        --define "_arch $RA_RPM" --define "_build_arch $RA_RPM" \
        --define "_host_cpu $RA_RPM" --define "_host_vendor pc" --define "_host_os linux" \
        --define "_target_cpu $RA_RPM" --define "_target_vendor pc" --define "_target_os linux" \
        -bb "$TOP/SPECS/lineclash.spec" \
        2>&1 | grep -v "^Processing\|^Executing\|^Provides\|^Requires\|^Checking\|elfdeps\|brp-" \
        || fail "rpmbuild failed"

    BUILT="$(find "$TOP/RPMS" -name "*.rpm" | head -1)"
    [[ -n "$BUILT" ]] || fail "RPM not produced"
    cp "$BUILT" "$DIST_DIR/lineclash-${RV}-1.fedora.${RA_RPM}.rpm"
    cp "$BUILT" "$DIST_DIR/lineclash-${RV}-1.el9.${RA_RPM}.rpm"
    rm -rf "$TOP"
    ok "Fedora RPM: lineclash-${RV}-1.fedora.${RA_RPM}.rpm"
    ok "RHEL RPM:   lineclash-${RV}-1.el9.${RA_RPM}.rpm"
fi

# ── openSUSE Leap RPM ─────────────────────────────────────────
DO_OPENSUSE=$DO_LEAP15
if [[ $DO_OPENSUSE -eq 1 ]]; then
    header "Building openSUSE Leap RPMs"
    command -v rpmbuild &>/dev/null || sudo zypper install -y rpm-build 2>/dev/null || \
        fail "rpmbuild not found"

    TOP="$(mktemp -d /tmp/lineclash-suse-XXXXXX)"
    RR="$TOP/root"
    mkdir -p "$TOP"/{BUILD,RPMS,SOURCES,SPECS,SRPMS} "$RR"
    _build_staging "$RR"

    MOD_FILES="/usr/share/lineclash/music"
    EXTRA=""
    [[ -f "$RR/usr/sbin/lineclashd" ]]      && EXTRA="$EXTRA\n/usr/sbin/lineclashd"
    [[ -f "$RR/usr/bin/lineclashd-tool" ]]  && EXTRA="$EXTRA\n/usr/bin/lineclashd-tool"

    _suse_one() {
        local leap_ver="$1" dist_tag="$2"
        cat > "$TOP/SPECS/lineclash.spec" << SPEC
Name:           lineclash
Version:        ${RV}
Release:        1.${dist_tag}
Summary:        Multiplayer Falling Blocks (LineClash protocol) - openSUSE Leap ${leap_ver}
License:        GPL-3.0-or-later
URL:            https://github.com/frossmant/lineclash
%description
LineClash - a cross-platform multiplayer falling blocks clone in the LineClash
tradition. Play with friends, bots, or spectate in real time.
Requires the SDL2 and libcurl runtimes used for this build.
Target: openSUSE Leap ${leap_ver}
%install
cp -a ${RR}/. %{buildroot}/
%files
%defattr(-,root,root,-)
/usr/share/doc/lineclash
/usr/games/lineclash
%dir /usr/share/lineclash
${MOD_FILES}
/usr/share/applications/lineclash.desktop
/usr/share/man/man6/lineclash.6.gz
$(printf "$EXTRA")
%changelog
* $(date '+%a %b %d %Y') frossmant <frossmant@github.com> - ${RV}-1
- Release ${RV} for openSUSE Leap ${leap_ver}
SPEC
        rm -rf "$TOP/RPMS"/*
        rpmbuild --define "_topdir $TOP" --define "_builddir $TOP/BUILD" \
            --define "_rpmdir $TOP/RPMS" --define "_sourcedir $TOP/SOURCES" \
            --define "_specdir $TOP/SPECS" --define "_srcrpmdir $TOP/SRPMS" \
            --define "_arch $RA_RPM" --define "_build_arch $RA_RPM" \
            --define "_host_cpu $RA_RPM" --define "_host_vendor pc" --define "_host_os linux" \
            --define "_target_cpu $RA_RPM" --define "_target_vendor pc" --define "_target_os linux" \
            -bb "$TOP/SPECS/lineclash.spec" \
            2>&1 | grep -v "^Processing\|^Executing\|^Provides\|^Requires\|^Checking\|elfdeps\|brp-\|Recognition\|regex error" \
            || { warn "openSUSE Leap $leap_ver RPM failed"; return 1; }
        local built; built="$(find "$TOP/RPMS" -name "*.rpm" | head -1)"
        [[ -n "$built" ]] || { warn "openSUSE Leap $leap_ver RPM not found"; return 1; }
        local out="$DIST_DIR/lineclash-${RV}-1.opensuse-leap${leap_ver}.${RA_RPM}.rpm"
        cp "$built" "$out"
        ok "openSUSE Leap $leap_ver: $(basename "$out")  ($(du -sh "$out" | cut -f1))"
    }

    [[ $DO_LEAP15 -eq 1 ]] && _suse_one "15" "lp15x" || true
    [[ $DO_LEAP16 -eq 1 ]] && _suse_one "16" "lp16x" || true
    rm -rf "$TOP"
fi

# ── Ubuntu DEB ────────────────────────────────────────────────
if [[ $DO_DEB -eq 1 ]]; then
    header "Building Ubuntu 24.04 .deb"
    command -v dpkg-deb &>/dev/null || sudo apt-get install -y dpkg 2>/dev/null || \
        fail "dpkg-deb not found"

DA="amd64"
    DEB_OUT="$DIST_DIR/lineclash_${RV}_ubuntu24.04_${DA}.deb"
    PKG="$DIST_DIR/deb-staging/lineclash_${RV}_${DA}"
    rm -rf "$PKG"; mkdir -p "$PKG/DEBIAN"
    _build_staging "$PKG"
    mkdir -p "$PKG/usr/share/doc/lineclash"

    cat > "$PKG/usr/share/doc/lineclash/copyright" << COPY
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: lineclash
Upstream-Contact: frossmant <https://github.com/frossmant>
Source: https://github.com/frossmant/lineclash
Files: *
Copyright: 2024 frossmant
License: GPL-3.0-or-later
COPY

    printf "lineclash (${RV}-1) noble; urgency=low\n\n  * Release ${RV}\n\n -- frossmant <frossmant@github.com>  $(date -R)\n" \
        | gzip -c > "$PKG/usr/share/doc/lineclash/changelog.gz"

    cat > "$PKG/DEBIAN/control" << CTRL
Package: lineclash
Version: ${RV}-1
Section: games
Priority: optional
Architecture: ${DA}
Maintainer: frossmant <https://github.com/frossmant>
Homepage: https://github.com/kladhest-se/lineclash
Depends: libsdl2-2.0-0, libcurl4t64 | libcurl4
Description: Multiplayer Falling Blocks (LineClash protocol)
 LineClash is a cross-platform multiplayer falling blocks clone in the
 LineClash tradition. Requires SDL2, libcurl and SQLite runtimes.
CTRL

    (cd "$PKG" && find usr -type f | sort | xargs md5sum > DEBIAN/md5sums 2>/dev/null || true)
    dpkg-deb --build --root-owner-group "$PKG" "$DEB_OUT" || fail ".deb build failed"
    rm -rf "$DIST_DIR/deb-staging"
    ok "Ubuntu .deb: $DEB_OUT  ($(du -sh "$DEB_OUT" | cut -f1))"
fi

hr
ok "Linux packaging complete — dist/"
ls -lh "$DIST_DIR"/lineclash* "$DIST_DIR/$APP_NAME"* 2>/dev/null | sed 's/^/    /'
