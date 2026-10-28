# Build the 4macOS servers on Linux (x86_64)

This folder holds the source needed to build the four original 4Story servers
(Control, Login, World and Map) as native Linux x86_64 programs. It is a
self-contained copy: the build reads only what is in this folder.

Built and tested on Ubuntu 22.04.5 with clang 14 and libc++ 14 (Hetzner VPS,
2 vCPU, 3.8 GB RAM). Other distributions may work but are untested.

## What is here

| Path | What |
| --- | --- |
| `tools/build-modules.sh` | the build script (run it with `bash`) |
| `native/server/` | the Win32/ATL/ODBC compatibility layer the servers are built against |
| `source/4Story 5.0 Source Files/Server/` | the original server sources |
| `source/4Story 5.0 Source Files/Lib/Own/` | the original shared libraries and protocol headers |

The game client, game data and the macOS tools are not part of this folder.

## 1. Install the build tools

```
sudo apt-get install -y clang libc++-dev libc++abi-dev unixodbc-dev e2fsprogs
```

## 2. Use a case-insensitive folder

The original sources include headers with Windows spelling (`SQL.h`,
`WinCrypt.h`), so the build must run in a case-insensitive directory. On Linux
that is an ext4 volume with the `casefold` feature and the `+F` attribute on the
folder (Linux kernel 5.2 or newer). One way to get one:

```
sudo truncate -s 1G /srv/4story-build.img
sudo mkfs.ext4 -q -O casefold /srv/4story-build.img
sudo mkdir -p /srv/4story-build
sudo mount -o loop /srv/4story-build.img /srv/4story-build
sudo mkdir /srv/4story-build/work
sudo chattr +F /srv/4story-build/work
sudo chown "$USER" /srv/4story-build/work
```

Copy this `linux` folder into `/srv/4story-build/work/` and work from there.

## 3. Build

```
cd /srv/4story-build/work/linux
bash tools/build-modules.sh
```

Use `bash`, not `sh`: the script needs `jobs -r`. To build only some roles, name
them: `bash tools/build-modules.sh TLoginSvr TMapSvr`.

The programs are written to `native/server/build/module/`:
`TControlSvr`, `TLoginSvr`, `TWorldSvr` and `TMapSvr`.

## Running them

The programs need SQL Server 2022, Microsoft ODBC Driver 18 for SQL Server
(`msodbcsql18`) and the runtime libraries `libc++1`, `libc++abi1`, `libunwind`,
`libltdl7` and `unixodbc`. This folder does not include a launcher, database
seeds or an installer. Setting up the database and starting the four programs
together is not covered here yet.

Prebuilt programs are attached to the repository's releases.
