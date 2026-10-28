# Run your own 4macOS server

This workflow is for the source workspace on an Apple silicon Mac. You need the
source repository plus the dependency, game-data and clean database-seed ZIPs.
Follow [the input archive instructions](../../publication/INPUTS.md) first.

## Build and check

Install Apple developer tools and Python 3. In the source folder:

```sh
./4story.command doctor
./4story.command build all --channel dev --version 0.1
```

The normal build uses the supplied libraries, SDKs, Rust toolchain and cached
crate sources offline. It builds account components, all four native server
programs and the client. Building does not start servers or restore a database.

## Install your database and configuration

Use an interactive Terminal:

```sh
./4story.command server install
```

The installer checks or installs Homebrew, Docker, Colima, unixODBC, OpenSSL,
Microsoft ODBC Driver 18 and Rosetta. Internet access and vendor license
acceptance are required. SQL Server Express runs in a Colima virtual machine.
The supplied clean seeds initialize a new instance; do not use the original
release's developer database backups.

Install asks for the IPv4 address players use. For players on your network,
use this Mac's LAN address. Loopback (`127.0.0.1`) does not work as the player
login address with this server. Internet play additionally requires your own
public address and appropriate router/firewall configuration.

This workspace uses its own resources:

| Resource | Location/name |
| --- | --- |
| Colima profile | `4story-port` |
| Docker context | `colima-4story-port` |
| SQL Server container | `4story-port-sql` |
| SQL data volume | `4story-port-data`, inside that Colima profile |
| SQL port | `127.0.0.1:14334` |
| Databases | `TGLOBAL_GSP`, `TGAME_GSP` |
| Database password | Login Keychain entry `4Story Port Server`, account `sa` |
| Configuration and state | `runtime/server/` |
| Server logs | `runtime/server/logs/` |

The installer keeps an existing complete pair of databases. It refuses a
partially installed pair. Re-running installation can update configuration;
it is not a request to erase existing accounts or characters.

## Start, create an account and play

```sh
./4story.command server start
./4story.command server status
./4story.command server create-account
./4story.command client --server YOUR_SERVER_IP
```

`create-account` asks for account credentials in Terminal. Creating an existing
account again changes its password. The default login port is 4815. The server
assigns GM status to the first character on a later server start.

Startup requires these ports to be free:

| Port | Service |
| --- | --- |
| 3615 | Control |
| 4815 | Login |
| 3815 | World |
| 5815 | Map |
| 4700 | Local debug receiver |

The default topology supports one cluster on these ports. Startup refuses port
conflicts; it does not stop another cluster for you. Control and World are
management services reachable on the local network. Use a trusted network;
do not expose their ports publicly. For remote players, only the game ports
4815 and 5815 need forwarding. SQL remains bound to loopback.

## Stop and diagnose

```sh
./4story.command server logs
./4story.command server stop
```

Stop shuts down this workspace's game services, saves state, then stops its SQL
container and Colima profile. It disconnects players. Read the role logs when a
role fails to become ready; a listening port alone does not prove initialization.

Builds and client launch have been checked for this source export. First-time
server installation, starting a separate new cluster, login and gameplay remain
unverified. See [publication validation](../../publication/VALIDATION.md). Opening
the ZIPs works with macOS Archive Utility; the binaries and prerequisites still
have their own platform requirements. Older macOS compatibility is unverified.
