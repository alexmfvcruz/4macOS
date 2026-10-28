# Preparing a GitHub source candidate

Run from the portable workspace:

```sh
python3 tools/publication/export_source.py
```

Default output: `.publication/4macOS/`. Local audit:
`.publication/4macOS-audit.json`. Outputs must be new paths. The original workspace
is read-only to the exporter. No commit, upload, dependency install or service
startup occurs. Review the excluded paths and redactions in the local report.
The candidate has no original registry exports or database backups, bundled
SDKs/toolchains, game directory, compiled libraries, IDE caches or runtime state.

The allowlist retains source/build descriptions, embedded application resources,
third-party source notices, tests, and shader bytecode used by resource generation.
It drops historical captures, baseline results and local deployment utilities.
Known local registry/env credential values are compared in memory; reports never
include their contents. Pattern and UTF-16 checks detect selected key/token/home
path/address patterns, but cannot prove the absence of every possible secret.
Unrecognized findings block a clean scan status and must be investigated.

Review distribution rights separately. Download availability does not establish
redistribution permission; no new license is invented for the original project.
Do not publish dependency/game/seed bundles without reviewing them separately.

Validate from the separate source folder with authorized `deps/`, `game/`, and
`server/database/` inputs copied into their documented paths. Run the doctor,
resource generator and relevant builds; record which checks actually ran. Avoid
starting servers or touching the working workspace's live database for this check.

After the candidate is reviewed, create a new Git repository there. Review the
staged file list before committing; only publish the selected source folder.
Do not run `git add .` in the original portable workspace.
