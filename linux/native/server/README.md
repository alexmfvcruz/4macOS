# Native server infrastructure

## Original server modules (milestone 4)

`sh tools/build-modules.sh` compiles the original TControlSvr, TLoginSvr,
TWorldSvr and TMapSvr sources into `native/server/build/module/` using the
compatibility layer in `native/server/win32/`. These are the executables that
load real state and talk to each other; start them with
`tools/server/native_cluster.py` (see `research/database/native-cluster.md`).
The live check is `native/server/tests/native_cluster_test.py`, run by
`sh tools/check-server.sh` when the audit SQL container is available. It
includes `native_play_test.py`, a scripted client (`game_client.py`) that logs
in, creates a character and enters the world; that test can also be run alone
against a started cluster. Evidence
and limits: `research/milestone-4.md`, "Completion".

The sections below describe the earlier `fourstory-service` infrastructure host.

Build with `sh tools/build-server.sh`. Run the full checks with
`sh tools/check-server.sh`; the TCP tests require localhost listener permission.
The build and original ODBC checks require `brew install unixodbc`.
The build also produces `native/server/build/libfourstory-shared.a` from the
original packet and five shared SQL implementations. Native consumers must use
`FOURSTORY_NATIVE_PACKET`, `FOURSTORY_NATIVE_ODBC`, and link unixODBC.
The C++ components use C++17 and POSIX sockets, with no ATL, Win32 threads or
IOCP dependency. The executable selects each role’s original-packet dispatch bridge. Original
game handlers and module initialization are not yet connected.

`fourstory-service` is currently an infrastructure host. Its listener announcement
does **not** establish database readiness, game authentication or a working
4Story service. Database settings are parsed but are not used to open a database
yet. Packet encryption and game dispatch remain integration work.

For a Control infrastructure configuration:

```ini
service=control
listen_address=127.0.0.1
port=3810
db_dsn=TGLOBAL_GSP
db_user=your_database_user
db_password_env=FOURSTORY_DB_PASSWORD
shutdown_ms=5000
```

Start a single host in the foreground:

```sh
native/server/build/fourstory-service /absolute/path/control.conf
```

Or supervise one or more explicit configurations:

```sh
python3 tools/server/supervise.py --shutdown-seconds 6 /absolute/path/control.conf
```

SIGINT/SIGTERM requests cancellation, stops accepting connections, joins workers
and drains queued sends. Workers must cooperate with cancellation; the supervisor
enforces the external hard deadline and reaps processes. Choose a supervisor
deadline longer than each configuration's shutdown_ms. Unexpected exits restart
with bounded exponential delay, up to a configurable limit; a service stable for
60 seconds resets its restart count. No system service is installed.

Runtime hooks run on the network owner. Workers remain gated through `prepare`
and listener setup. After successful setup they resume, then `activated` runs
on the owner for role actions that follow the original ResumeThreads. Failed or
cancelled preparation discards pending startup work and joins the waiting workers.
`prepare` finishes data initialization. `network_ready` runs after listener
creation while workers remain gated; Map environment setup belongs there. Workers use bounded `post()` commands to
send or close connections; IDs cannot accidentally address a reused socket.
Queued packets are not replayed after reconnect. Required dependency connection
failure defaults to stopping the service, matching the original Map behavior on
World loss; the supervisor restarts the process. Explicit `dependency_loss=retry`
enables native in-process retry. A successful TCP connection resets that retry
delay; game-level registration after connection is milestone 4.
The supervisor currently starts the supplied processes without a game-readiness
dependency gate. World-before-Map database/handshake readiness remains milestone 4.

`ControlDispatch` provides the Control batch dispatch bridge. Register handlers
for IDs from the original message roster, then assign `bind()` to Runtime's packet
hook. Handlers receive owned original `CPacket` storage and a connection ID;
returning an error closes that connection through the network owner. Registrations
are copied when binding, and the bound callbacks must finish before referenced
role state is destroyed. Actual Control business handlers still need porting.
`WorldDispatch` uses the same bridge with the original World message roster.
Its default binding uses the batch lane; internal producers can select database
or timer lanes explicitly. This routing does not imply DB query execution or a
working inter-service handshake.

`LoginDispatch::hooks()` supplies paired packet and close hooks. Install both
on one Runtime. It runs Login's network-worker dispatch in order per connection,
allows separate connections to progress independently, and discards queued
packets after that connection's handler error or EOF. Registrations use the
original Login message roster; actual authentication and per-worker DB contexts
remain integration work.

`MapDispatch` compiles the original Map message switch and uses explicit batch,
database, AI or log bindings. Message recognition is separate from authorization;
role handlers must supply session checks. The checked profile enables Sky Garden
as the supplied Map/World source does. UDP game logging and optional anti-cheat
branches remain separate integration work.

`role_timers.hpp` binds the original role intervals to worker-loop hooks during
`prepare`. Control queues an original CT_TIMER_REQ on batch; World preserves its
idle-wake callback order; Map updates wall/tick time before AI iterations and
passes unsigned elapsed ticks to its timer callback. Callers must supply the
actual role callbacks; incomplete callback sets are rejected.

`RoleLifecycle` connects explicit InitDB/LoadData and shutdown callbacks to these
phases. It preserves startup error codes, runs Login/World activation actions,
queues World/Map save work before drain, and closes the database after update
cleanup even if updating throws. It requires real callbacks; the infrastructure
host does not install placeholder database or game initialization.

`initialize_original_database<Role>` compiles the original role's InitDB body
against native ODBC and the role's original query registration. It can supply the
lifecycle database callback. Current evidence covers missing-DSN failure and
cleanup; a successful database connection is not yet established. The build now provides `libfourstory-roles.a` with all four original query
implementations isolated by role namespace; it links alongside the shared library
and unixODBC. The combined-link and symbol checks verify the inspected class
collisions are removed.

`ClearOriginalLoginUsers(&database)` exposes the original Login activation
cleanup body and can supply the lifecycle's `clear_login_users` callback. It uses
the original query guard. Its void return preserves the source's ignored query
failure; database readiness must verify the procedure's actual effect.

`OriginalRoleDatabase` owns the original database object and installs real
InitDB/Close callbacks, plus Login activation cleanup, into `RoleLifecycle`.
Password lookup happens at initialization, using the configured environment name
or an explicit provider. `bind_login` installs the original Login LoadData body
and retains caller-supplied `LoginStartupData` plus the client reference path.
Other roles still require application loading callbacks; all roles require
their worker/state cleanup callbacks. Successful database loading is unverified.
The bound hooks retain database ownership until their runtime work is finished.

See [the milestone evidence](../../research/milestone-3.md) for source mapping,
checks and outstanding gaps.

The foreground supervisor accepts optional local management requests with
`--control-stdin`: one JSON object per line, for example
`{"action":"status","service":0}`. Service indices refer to its configured
argument order. Actions are `start`, `stop`, and `status`; responses begin with
`CONTROL ` and contain JSON. State values preserve Windows stopped (1), start
pending (2), stop pending (3), and running (4). Running means a live process.
An explicitly stopped service stays stopped until started again. This interface
is local inherited stdin, not a network listener; original Control packet
handlers and machine/service mapping are not yet connected to it.

`original_control_start` / `original_control_stop` compile the shared original
Control method fragments. `ControlServiceManagement` maps DWORD service IDs to
configured supervisor indices; `SupervisorCommands` connects the callbacks to
dedicated inherited command/response pipes with a two-second deadline. Pipe
endpoints are borrowed and must outlive the callbacks; the host must ignore
SIGPIPE, as the native host already does. The live-process integration test
proves this start/stop path. Original QueryStatus processing and management
packet handlers still need integration.

`SupervisorCommands.status` retrieves process state, and the management adapter
maps failed/unknown lookups to original `DWORD(-1)` CANNOTCONTROL. Invalid state
values and mismatched service responses invalidate the dedicated pipe stream.
The original transition bodies are now compiled in `control_status_transition.hpp`:
reconnect notification, automatic-restart decision and service-change processing.
`original_control_poll_status` combines them in source order against the native
backend. Manager login has a native original-query boundary in
`control_manager_login.hpp`; successful database authentication remains unverified.

`control_management_worker` binds the original idle status-poll policy to
`Lane::management` and delivers original notifications on `Lane::batch`.
The runtime integration check polls a live supervisor child and shuts down
after observing the expected two notifications. Actual Control startup state
and management packet registrations remain required by the executable.
