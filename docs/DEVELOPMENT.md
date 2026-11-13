# Development, tests and packaging

Native builds target macOS and Linux. Server and GUI Makefiles contain Windows cross-compilation branches, but the terminal client and POSIX test harness are intended for macOS/Linux. Windows and Amiga branches need platform validation before release; they are not established by a successful native macOS build.

## Checks

Install a C compiler, make, SQLite development headers/library, SDL2 for GUI, and Python 3 for HTTP tests. From the project root:

```sh
make -j4
make -C tests unit
make -C tests integration
# Or build first, then run both suites:
./scripts/run-tests.sh --all
```

Unit tests cover field/piece basics, socket fragmentation/coalescing, oversized input, output backpressure and descriptor reuse, failed-login logging and latest-game persistence. Integration tests exercise registration, login challenge, lobby selection, joining, player visibility, valid HTTP JSON, escaping, incomplete/slow clients, request expiry and API disablement. Tests start local servers on temporary ports using unique databases; local TCP access is required. The C harness requires POSIX processes/sockets. Port collisions are possible on a busy shared host; retry after checking startup diagnostics.

For memory checks, use the sanitizer-capable system compiler:

```sh
make -C tests clean
make -C tests unit CFLAGS='-Wall -Wextra -g -O1 -I../shared/include -fsanitize=address,undefined -fno-omit-frame-pointer'
```

## Release metadata

`version.mk` is the release-version source. `get-version.sh` supplies version, numeric build metadata and UTC date to Makefiles and scripts. Build numbers default to `major*10000 + minor*100 + patch`; set the environment variable `BUILD_NUMBER` to a numeric CI build identifier to distinguish builds of one release. `SOURCE_DATE_EPOCH` fixes the generated date for reproducibility. Packaging and test scripts do not increment counters or rewrite build headers. CI builds server artifacts on each packaging platform; integration jobs build a native server and the Linux images include SQLite headers and Python 3. Header generation uses atomic replacement and only changes content when metadata changes. Both clean and parallel builds are supported; header dependencies trigger recompilation.

```sh
make VERSION=1.0.1
BUILD_NUMBER=1234 make
sh get-version.sh
```

## Packages

Build all binaries on the destination platform first. Native scripts:

```sh
./scripts/build-server.sh
./scripts/build-client.sh
./scripts/build-cli.sh
# macOS, unsigned/ad-hoc signed local DMG:
./scripts/package-macos.sh
# Optional macOS developer identity:
./scripts/package-macos.sh --sign 'Developer ID Application: Name (TEAM)'
# Linux x86_64, install relevant rpmbuild/dpkg-deb tools first:
./scripts/package-linux.sh --deb
./scripts/package-linux.sh --rpm
./scripts/package-linux.sh --opensuse-leap15
```

Artifacts go into `dist/`. Linux packaging currently assumes x86_64. Inspect package contents and test installation on clean target systems before distributing. Code signing does not imply Apple notarization. Optional music assets and their licensing are described in [music/README.md](../music/README.md). Include LICENSE.md and corresponding source when distributing under the project's license.

## Public web page

`public-web/index.php` is the promotional PHP page, separate from the game and JSON API. For a local preview with PHP installed:

```sh
php -S 127.0.0.1:8080 -t public-web
```

For public hosting, configure a production PHP-capable web server with `public-web/` as its document root. Do not serve the project root or database directory. Review the page's release links/version before publishing. Fonts are requested from Google Fonts; the page is not an HTTP API proxy.

Use `make verify` for the full native gate, including client reconnect and offline release-tool checks. See [platform verification](PLATFORM-VERIFICATION.md), [trust review](TRUST-REVIEW.md), and [release workflow](RELEASING.md).

Music-specific checks: `make -C tests music music-ui`. The UI test uses a dummy display/audio device and writes previews in the system temporary directory. `make -C tests decoder-sanitize` checks all original audio decoders using AddressSanitizer/UndefinedBehaviorSanitizer where supported, without loading the native SDL library.

## Web-service checks

Run `make directory-test` for offline delivery checks. With `LINECLASH_WEB_CONFIG` pointing to a disposable MariaDB database, run `make web-test` for PHP API and real dedicated-server upload/shutdown tests. The `test:web-directory` job runs these against MariaDB. See [web services](WEB-SERVICES.md).

See [discovery, ranked history, health reports, replays and music shortcuts](ENHANCEMENTS.md) for the latest controls, deployment changes and verification limits.

The antialiased GUI font is compiled from `clients/gui/src/ui_font.h`; no extra font runtime is required. To regenerate it deliberately, install Pillow in a development environment and run `scripts/build_ui_font.py`. The bundled Roboto source is licensed under SIL OFL 1.1; all packages include its notice. Run `tests/test_web_refresh.py` for offline refresh-script safeguards.
