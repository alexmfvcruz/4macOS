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

Client startup was confirmed interactively by the maintainer after launching the
built macOS app bundle. A terminal-launched attempt aborted in macOS application
registration before rendering. An automated screenshot showed the app window and
build label; it is not evidence of login or gameplay.

A separate server cluster could not be started in the managed agent session:
existing development services occupied the normal game ports, while the session
sandbox denied the Docker socket, outgoing localhost connections and binding an
unused test port. Existing services were left running and no database was changed.

Not tested for this source export:

- First-time server installation or clean database restore.
- Starting the newly built server cluster with an isolated database.
- Login, character selection, joining the world, or gameplay.
- Fresh GitHub clone build, Windows builds, or other Mac/OS versions.

A successful build is not a successful server/client integration or gameplay test.
Redistribution permissions for the supplied original source and assets remain
unconfirmed; existing notices are retained and no new blanket license is asserted.
