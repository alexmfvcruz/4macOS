# Native accounts

Registration uses the original game GUI, a Rust HTTPS client/service and the
existing SQL Server login contract. No browser is embedded in the game.
The native form has created an account and logged into server/channel selection.
Touch ID login was removed on 2026-10-21 (user decision): the data protection
keychain needs a paid Apple Developer signature for distributed builds. Password
login is the only login path; the client stays ad-hoc signed.

## Files

- `core/`: shared byte limits, field validation and OS-random account names.
- `bridge/`: C ABI and one bounded registration worker. The game polls results.
- `service/`: Axum HTTPS API, request admission and parameterized Tiberius calls.
- `sql/`: additive migration and create-only transaction/procedure.
- `platform/`: macOS artwork decoding adapter.
- `assets/`: unchanged artwork from the approved mockup, with provenance hashes.
- `../client/app/src/accounts_ui.cpp`: original TComp controls and D3D9 modal.
- `tests/`: real C++ ABI, isolated SQL and transport checks.

## Run

From a checkout containing the feature:

```
./4story.command build accounts --checks
./4story.command accounts prepare
./4story.command accounts start
./4story.command client
./4story.command accounts status
./4story.command accounts stop
```

`prepare` adds the name uniqueness index, `NativeAccountProfile` e-mail table
and `NativeRegisterAccount` procedure. It refuses duplicate legacy names.
It creates a dedicated SQL login that can only execute that procedure.
It never updates existing account passwords or resets the game cluster.
`start` never migrates. The service binds to `127.0.0.1:4820` using HTTPS.
Private SQL/TLS files and logs stay in this checkout's ignored `runtime/accounts`.
CA trust is supplied to this client process; system trust is unchanged.

The client opens registration from **Create Account**. Its focused text field
uses a steady caret. Buttons show hover/pressed/disabled states, and **Show
Password** switches to **Hide Password** when the passwords are visible. Successful
registration opens an **Account created** confirmation; **OK** returns to normal
login with the account name filled in.

For another endpoint, set `FOURSTORY_ACCOUNTS_URL` to its complete HTTPS
`/v1/accounts` URL. `FOURSTORY_ACCOUNTS_CA` may identify a private CA PEM.
Certificates and hostnames are verified; redirects and inherited proxies are
blocked. HTTP requires the explicit `FOURSTORY_ACCOUNTS_DEV_HTTP=1` development
switch and an actual loopback IP. This is not the normal launcher path.

## Build and checks

Uses an existing Rust toolchain (tested with ARM64 Rust 1.92), Clang and macOS
frameworks. Set `FOURSTORY_RUST_TOOLCHAIN` to an existing toolchain directory
if necessary. No toolchain installation or rustup repair runs.

```
python3 native/accounts/build.py --fetch  # explicit dependency acquisition
python3 native/accounts/build.py --test  # locked, offline builds and tests
```

Direct crate versions and every transitive package are locked in `Cargo.lock`.
Downloads use `deps/cargo`; a main-checkout build can reuse the
already populated Codex checkout cache. Builds use `build/accounts`.
The C++ ABI probe is ASan/UBSan instrumented; Rust uses the normal compiler.

SQL tests require a newly created isolated database, never the game database.
Use the existing native-cluster Python environment for `tests/sql_fixture.py`.
It creates a named test database and records it in `runtime/accounts-test-db.txt`;
it never drops a database. `service/tests/isolated_sql.rs` is explicitly ignored
until its test database and private secret-file path are provided. The explicit
run tests concurrent duplicates and rollback; `sql_fixture.py verify` checks rows.
`tests/transport.py` and `tests/sql_runtime.py` refuse a live game database.
Generated fixtures and private test accounts are retained, not automatically deleted.

## Contracts and limits

Names: 1–50 ASCII letters, digits or underscores. New passwords: 8–64 printable
ASCII bytes, preserving spaces. E-mail is optional ASCII, at most 254 bytes.
These are registration policies, not historical plaintext-password limits.
The service derives the existing 40-hex SHA-1 credential; game packets and
Windows source paths are unchanged. The original SHA-1 protocol is preserved.
The new e-mail field is storage only: verification/recovery mail is not implemented.

Registration returns success only after SQL commits the account, user-info and
profile rows together. IDs allocate above both legacy tables and the identity
seed; existing orphan user-info rows are never reused. Duplicate names follow
SQL's existing case-insensitive collation. Requests are never automatically retried.
Cancellation can discard a result but cannot undo a transaction already committed;
an unknown response tells the user to try logging in before retrying.

A Rust service handles registration; the native C++ adapter shows the form.

Passwords are not logged by this feature. Owned Rust input buffers and native
credential buffers are cleared when released; this does not guarantee erasure of
copies inside the legacy engine, JSON, HTTP, TLS or SQL libraries. Normal legacy
login has its existing memory/transport behavior. Private test harness secrets
are kept only in ignored files with mode 0600 and never committed.

Windows compilation is not tested here; the Windows-view checker must pass.

## Main integration

The implementation branch has its own ignored runtime files. Before starting a
service from main, the integrator must preserve the existing private
`runtime/accounts/TGLOBAL_GSP.env` and `tls/` from the Codex checkout, copying
them with their permissions into main (never commit or print them). Stop the
Codex-owned HTTP service, then run prepare/start from main. The deterministic SQL
login already exists: preparation deliberately refuses to reset it when its
private configuration is missing. This does not affect the game server cluster.
