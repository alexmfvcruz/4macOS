# 4macOS

Native macOS client/server code, original source inputs, build scripts and tests.
This source export preserves the portable workspace's directory structure.
It excludes generated programs, private runtime state, original database backups,
Windows registry/service exports, local deployment utilities and historical captures.

## Get the separate build inputs

The repository does **not** include the large offline dependency or game bundles.
See [input archive instructions](tools/publication/INPUTS.md), then copy
the unpacked inputs into this checkout at the same paths:

| Path | Contents |
| --- | --- |
| `deps/` | SDL3, DXVK Native, MoltenVK, Vulkan SDK, unixODBC, Rust toolchain/crates and dependency sources/notices |
| `game/` | Game data including `Tcd/TString.tcd` and `TClientCmd.tif` |
| `server/database/` | Clean seed backups and `manifest.json`, required by server installation and the workspace doctor |

These paths are ignored by Git. Keep personal runtime configuration and logs out
of copies. Do not substitute original database backups for the clean seeds.
The source export does not establish permission to redistribute the original
4Story code, artwork, data, or bundled third-party components. Existing notices
remain with their components; no new project-wide license is asserted.

See [publication validation](tools/publication/VALIDATION.md) for the exact build
evidence and the server/client runtime checks that remain untested.

## Build on Apple silicon

Install Apple developer tools and Python 3. With the
separate inputs in place:

```sh
./4story.command doctor
./4story.command build all --channel dev --version 0.1
```

The normal build uses the supplied dependency bundle offline. Server installation
additionally needs Docker/Colima, Rosetta, internet access and Microsoft license
acceptance. See `tools/release/server/README.md` before installing a server.
The default client login port is 4815:

```sh
./4story.command client --server YOUR_SERVER_IP
```

Source preparation and a successful build do not establish gameplay, Windows
compatibility, or a fresh server installation. Historical evidence captures have
been omitted. Optional historical tools may reference excluded fixtures; the
supported entry point is `4story.command`.

## Repository and large files

Code belongs in ordinary Git. Generated `build/` and private `runtime/` stay local.
Dependencies and game data are separate distributions; they are not committed or
tracked with LFS by this export. A maintainer can host authorized bundles through
GitHub Releases or external storage and supply SHA-256 checksums and versioned
instructions. Do not upload the original workspace ZIP as a source release.

`SOURCE-MANIFEST.json` records copied source-file hashes after publication
redactions. It covers the initial exported inputs and repository documents, excluding
itself; it is not automatically updated after development edits.
