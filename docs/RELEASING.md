# LineClash releases

This project adapts the release coordinator supplied in ZIFburn's Archive.zip. The credential, annotated-tag and GitHub release flows are retained; Swift/universal-app/notarization packaging is replaced with LineClash's native verification and archives. Upstream MIT notices are retained in scripts/LICENSE-ZIFburn and scripts/LICENSE-BitSkim. LineClash remains licensed as stated in LICENSE.md.

## Local configuration

The default settings file is outside the source tree:

```text
/Users/frossmant/Development/tools/git-release/projects/lineclash.json
```

It is installed with owner-only file permissions. `LINECLASH_RELEASE_CONFIG` or `--config PATH` selects another file. A non-secret template is in examples/release-config.json. The public destination is:

- Public: https://github.com/kladhest-se/lineclash

HTTPS API URLs and SSH Git destinations must identify the same repository, including the repository owner and name. Source pushes default to `main`. Git uses existing SSH keys or Git credential helpers. API authentication is separate: the current profile reads `GITHUB_TOKEN` from the environment. No token values are stored in JSON. The selected Python interpreter is configured through `env.LINECLASH_RELEASE_PYTHON`.

For 1Password, replace the relevant credentials object with either an actual copied secret reference:

```json
{"provider": "1password", "reference": "op://ACTUAL-VAULT/ACTUAL-ITEM/ACTUAL-SECTION/ACTUAL-FIELD"}
```

or the exact `vault`, `item`, `field` and optional `account` values from your existing item. Do not construct LineClash credential names from another project's profile. Only publish/source actions use credentials; build and prepare do not resolve 1Password secrets. Tokens are removed from child build environments. Set up repository-scoped credentials and release access before publishing.

## Review, build and prepare

Python 3.9+, Git, a compiler, make, SQLite, SDL2, and the platform's build prerequisites are required. Start with:

```sh
make release-plan
make release-tools-test
make release-build
make release-check
# Edit docs/RELEASE-NOTES.md before building; source changes require a rebuild.
make release-prepare
```

`plan` is offline and prints destinations, version and commands. `build` runs scripts/verify-platform.sh, then creates:

- A native ZIP containing server, administration tool, GUI, POSIX terminal client where supported, license, docs, soundtrack credits, the complete music library and an example configuration.
- A complete source tar.gz and website tar.gz.
- BUILD-PROVENANCE.json and SHA256SUMS.

Native ZIPs require installed SDL2 and SQLite runtimes; they are not standalone installers and are not automatically signed or notarized. Existing package scripts can produce platform installers separately. Include explicitly reviewed prebuilt assets with repeatable `--asset PATH` arguments to `build`, for example a DMG or DEB. Additional asset hashes are recorded; importing one does not establish that its installer or another operating system was tested.

Artifacts are written to outputs/artifacts/. `prepare` stages exact bytes and per-file checksums in outputs/release/publish/ and writes outputs/release/manifest.json. Review its destinations, commit, tag, annotation, notes and hashes. All artifact types, including corresponding source, are published together. Archive files, binary contents and source inputs are checked again before publication. Native build outputs must remain available and be checked on the recorded host platform.

Set version.mk's `VERSION ?= MAJOR.MINOR.PATCH` before building. Default tags are annotated `vMAJOR.MINOR.PATCH`. A custom tag/annotation can be frozen during preparation:

```sh
python3 scripts/release.py prepare --tag v1.0.0 --tag-message 'LineClash 1.0.0'
```

Move the whole outputs/ directory aside when starting another build/release; preserve receipts and provenance. A source change, changed binary, unexpected staged file or conflicting existing asset causes validation to stop.

## Source repository and release publication

The current project folder was supplied without Git metadata. Preparation can inspect an uncommitted source archive, but actual publication requires a clean committed Git checkout at the project root with every selected release source tracked. Prefer cloning existing repository history and applying these changes. Do not initialize a replacement history over a populated remote.

Read-only source-push preflight (contacts the GitHub remote):

```sh
python3 scripts/release.py source --to github --dry-run
```

Explicitly upload the committed source branch:

```sh
python3 scripts/release.py source --to github --confirm-push
```

For genuinely empty remotes only, `source --init --dry-run` can create an initial local repository/commit after checking the selected remote is empty; this is still a local mutation. It refuses remotes with history. No source push is performed without --confirm-push. Building/preparing does not create Git repositories or tags.

After reviewing the prepared manifest, explicitly publish:

```sh
python3 scripts/release.py publish --to github --confirm-publish
```

This preflights GitHub and tag conflicts, creates an annotated local tag at the recorded commit, pushes that tag, uploads and verifies the exact bytes, and transitions a GitHub draft to public only after its full asset inventory matches. It never force-pushes source or tags. `release --to github --confirm-publish` combines prepare and publish using existing build outputs; it does not rebuild.

Receipts are stored in outputs/release/receipt.json. If publication fails, retain all outputs and rerun publish after resolving the problem. Matching remote artifacts are reused; conflicting hashes, notes or tags stop publication. A failed attempt can leave a partial upload or a GitHub draft. The tool records progress and does not blindly retry network writes.

Live GitHub publication requires an authorized run with working credentials and committed repository history.
