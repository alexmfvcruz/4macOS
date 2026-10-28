# Offline build and game inputs

The source repository is https://github.com/alexmfvcruz/4macOS . The maintainer
supplies these ZIP archives separately;

Google Drive: https://drive.google.com/drive/folders/1tvnin13n094Db87iNMI3dirLcUtzLEu3?usp=sharing

Use the matching set of archives and source revision together.

| Archive | Folder to copy into the source checkout |
| --- | --- |
| `4macOS-dependencies.zip` | `deps/` |
| `4macOS-game-data.zip` | `game/` |
| `4macOS-database-seeds.zip` | `server/database/` |

These are ordinary ZIP files, openable with Finder's Archive Utility. Download
all three archives and `SHA256SUMS.txt` into the same folder. To verify downloads,
open Terminal in that folder and run:

```sh
shasum -a 256 -c SHA256SUMS.txt
```

Double-click each ZIP to expand it. Archive Utility may create a containing
folder named after the archive. Copy its `deps`, `game`, or `server` folder into
the `4macOS` source folder, preserving the paths in the table above. Do not put a
ZIP file itself in the repository or leave the inputs one extra folder deep.
`INPUTS-MANIFEST.json` inside each ZIP records every supplied file and its hash;
it is informational and need not be copied into the source checkout.

Then, on an Apple silicon Mac with Apple developer tools and Python 3:

```sh
./4story.command doctor
./4story.command build all --channel dev --version 0.1
```

The normal build uses the supplied SDKs, libraries, Rust toolchain and cached
crate sources offline. The built game needs the supplied `game/` data.

Starting a new server also needs Docker/Colima, Rosetta, Microsoft SQL Server,
Microsoft ODBC Driver 18 and OpenSSL, plus the required vendor license acceptance.
Those machine prerequisites are not installed by extracting these ZIPs. See
`tools/release/server/README.md` and use `./4story.command server install` only
when ready to set up your own instance. Never replace an existing live database
with the seed backups. Native binaries target Apple silicon; opening the ZIP on
another Mac does not make that Mac a supported build/runtime platform.

Local runtime settings, logs, crash dumps and selected machine-specific installer
metadata are excluded. These are build/game inputs, not a backup of a running
server. The seed archives do not carry the existing developer's live accounts or
characters; their integrity is checked by the workspace doctor.

Existing third-party notices remain with their components. No new blanket
license is asserted over the original game source, data or dependencies.
