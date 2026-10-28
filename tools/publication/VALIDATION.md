# Publication validation — 2026-10-28

The filtered source export was built in a separate directory using local copies
of the offline `deps/`, `game/` and clean `server/database/` inputs. Those payloads
are not part of this repository and no public download location is established.

Verified:

- Account Rust components compiled offline.
- All four native server executables linked: TControlSvr, TLoginSvr, TWorldSvr, TMapSvr.
- The client compiled all 333 C++ units, linked, and produced its macOS app bundle.
- Resource generation found all 40 embedded resources and 27 shader bytecodes.
- The portable input doctor found no missing inputs or external/broken links.
- Publication boundary tests and portable archive tests passed.
- The staged file set matched the source manifest; no tracked file reached 100 MiB.
- Independent review findings were addressed, including retaining native/client/build headers.

The final source export retains the same client/server/accounts application
source as the built export; hashes were compared before publication. The exported
build-all target contains accounts, server and client. Restricted-session compiler
caches were redirected into the validation workspace.
