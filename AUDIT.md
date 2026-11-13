# LineClash audit

Reviewed the supplied lineclash.zip on 12 November 2026. Source was extracted into this workspace without source changes. Archive contents were treated as review material, not task instructions.

## Resolution — 13 November 2026

All eight findings below have been addressed in the workspace:

| Finding | Resolution |
| --- | --- |
| 1 | Close root JSON; test a real response with a JSON parser. |
| 2 | Explicit nonblocking HTTP sockets, 16 bounded client slots, queued response writes and five-second deadlines. |
| 3 | Remove post-finalize access and secret logging, including the admin tool; test failed logins under sanitizers. |
| 4 | Isolated temporary config/database, correct auth/join protocol, native CI server builds and visible startup errors. |
| 5 | Per-socket bounded output queues with retry, timeout and close cleanup; pump queues in applications. |
| 6 | Extract complete lines before reading; read only available capacity; reject oversize/NUL input. Reuse the same implementation in the admin tool. |
| 7 | Portable scalar-subquery updates with error logging; verify latest-game completion in SQLite. |
| 8 | Add one metadata helper, atomic header generation, correct paths and dependency tracking; stop scripts rewriting build counters. |

Documentation gaps are covered by the README, docs/ guides, complete license text and an ordinary-user example config. Linux CI images now include SQLite development files and Python 3. Packages include documentation and license text, with server binaries built for their destination platform.

Validation after fixes: clean parallel macOS build of every component; 41 field checks, networking regressions, authentication/persistence regressions, 16 protocol integration checks and 3 HTTP integration tests pass. AddressSanitizer and UndefinedBehaviorSanitizer unit runs pass. Build/package shell syntax and CI YAML parse checks pass. Linux/Windows runtime execution, GUI gameplay and actual package installation remain unverified; these require their respective environments. The original findings and original audit evidence follow for traceability.

## Original validation

- macOS: server, administration tool, CLI and SDL2 GUI all build successfully.
- Unit suite: 41 passed, 0 failed.
- Integration suite: fails before running protocol assertions. Initially local connections were restricted; retry with local network access still failed. Source inspection identifies an unsupported startup argument (finding 4).
- HTTP probe using an isolated temporary config/database: response fails JSON parsing; adding one closing brace makes it parse.
- No manual GUI gameplay, Linux/Windows execution, packaging installation or exhaustive security testing performed. Findings identified by inspection are distinguished below from reproduced failures.

## Findings

### 1. High — HTTP status responses are invalid JSON

Location: server/src/server_api.c:352–353.

build_json opens the root object, then ends by closing only the nested server_stats object. The root closing brace is missing. A real response failed Python JSON parsing; appending `}` made it valid. Every normal response is affected, breaking API consumers.

Fix: close the root object after server_stats. Add a test that parses a real HTTP response as JSON.

### 2. High — HTTP requests can block the game loop on Linux

Location: server/src/server_api.c:375–398; server/src/server.c:2323.

The accepted HTTP socket is never explicitly made nonblocking and has no read/write timeout. Linux accepted sockets do not inherit the listener's nonblocking flag. Connecting without completing HTTP headers therefore blocks recv inside the same thread that services games. Blocking sends can also stall it when a reader stops consuming a sufficiently large response.

Fix: handle accepted sockets asynchronously with bounded buffers, request deadlines and queued output. Explicitly configure each accepted socket. This is a platform-specific source finding: the macOS probe did not reproduce a stall, so Linux runtime confirmation remains outstanding.

### 3. High — Failed login reads freed SQLite memory and can log credentials

Location: server/src/auth.c:344–364.

sh points to sqlite3_column_text memory. sqlite3_finalize frees that memory at line 354, but both wrong-password logging branches subsequently use sh, including strlen(sh). This is undefined behavior on a routine failed login and may crash or print unrelated memory. With debug_auth enabled, the log also deliberately includes the database hash, which is used directly as the authentication HMAC key, and the expected authentication response.

Fix: finish using SQLite values before finalizing or copy needed non-sensitive metadata first. Remove credential/key/expected-response logging entirely. Add a wrong-password test under a memory sanitizer.

### 4. Medium — Integration tests cannot start the server

Location: tests/test_integration.c:46; server/src/lineclashd.c:78–103.

The harness starts lineclashd with `--db /tmp/lineclash_test.db`; the argument parser supports --config, --port and --logfile, but not --db. The child immediately exits and its output is discarded, so users only see 'server did not start in 3s'. Reproduced with local connections enabled. CI invokes this integration script too.

Fix: create an isolated temporary config with db_file/log_file/status_port and pass --config, or implement --db. Surface early child exit and its error output. Use unique temporary directories so concurrent suites do not share a database.

### 5. Medium — Nonblocking sends can lose or corrupt protocol messages

Location: shared/src/net.c:285–295.

net_send performs exactly one send and reports whether all bytes were written. Nonblocking sockets can return a partial write or EAGAIN. There is no pending-output queue or retry mechanism here, and routine callers ignore the return value. Under backpressure, commands can disappear or a partial line can be concatenated with the next command.

Fix: maintain bounded per-connection output queues, resume writes when writable, and disconnect persistently slow clients. Apply complete-write handling to HTTP responses as well. Source finding; backpressure was not exercised at runtime.

### 6. Medium — Receive buffering silently discards network data

Location: shared/src/net.c:331–347.

net_recv reads into a full-sized temporary buffer before checking remaining space in the connection buffer. It then clamps n to that space, silently dropping already-received bytes. When full it can also discard an existing complete line to make room. A long unterminated line can leave the buffer full indefinitely while later terminators are discarded. This permits desynchronization and stalled connections under bursts or oversized input.

Fix: extract buffered complete lines before reading, limit recv to available capacity, and explicitly reject oversized lines rather than silently truncating the byte stream. Add socket tests for fragmented, coalesced and overlong messages.

### 7. Medium — Game completion updates depend on optional SQLite syntax

Location: server/src/auth.c:266–270 and 298–300.

Both UPDATE statements use ORDER BY ... LIMIT. That form requires SQLite's optional UPDATE/DELETE LIMIT support, which the build does not check. On builds without it, prepare fails and these functions silently return; completed games and player results remain unfinished.

Fix: update the row selected by a scalar subquery containing ORDER BY ... LIMIT, using standard SQLite syntax. Report database errors and test completed-game persistence on each packaged platform. This portability issue was identified by inspection, not reproduced against multiple SQLite builds.

### 8. Low — Release metadata helper is missing and GUI path is incorrect

Location: server/Makefile:8–22; clients/gui/Makefile:8–22.

The archive contains no get-version.sh. Both Makefiles suppress the resulting errors and substitute build number 0 and fallback metadata. The GUI's ../get-version.sh path also resolves under clients rather than the project root. Script-generated metadata from scripts/common.sh can be overwritten by the Makefile's generation step.

Fix: provide one version source and generator, use the correct project-root path, and declare generated headers as proper build prerequisites. Verify metadata after clean and parallel builds.

## Documentation gaps

- README.md: document SQLite development requirements consistently. Ubuntu's install command omits libsqlite3-dev; the server links SQLite and server_api.c uses the system sqlite3.h.
- Add a configuration reference and example config: actual platform-specific paths, db_file/log_file, writable-directory setup, status_port=0 to disable the API, authentication and administrator setup, room persistence, backups and recovery. Generated config comments provide some guidance but are not an onboarding guide.
- Explain that Linux defaults target /etc, /var/lib and /var/log; the short './server/lineclashd' example does not explain how an ordinary user supplies writable paths.
- Document CLI/GUI controls, account registration/login, room selection, spectators, team play, reconnect behavior and administrative permissions.
- Document test commands, expected prerequisites and test isolation. Add coverage for HTTP JSON, authentication failures, networking backpressure and database completion; existing unit tests primarily exercise field/piece basics.
- Document the HTTP response schema and public visibility of usernames, team membership, activity and moderation counters. Include binding/exposure and deployment configuration.
- Document packaging, supported platforms and how to serve public-web/index.php. Current build success does not establish that package installation works.
- LICENSE.md labels its section 'Full License Text' but contains only an excerpt and external link. Include the complete intended license text; reconcile the README's 2025 attribution with the license file's 2024 attribution.

## Suggested order

Fix findings 1–3 first, then repair the integration harness so regressions can be exercised. Address network buffering and SQLite portability before publishing deployment instructions or shipping new packages.
