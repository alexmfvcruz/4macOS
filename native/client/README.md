# C1 native data libraries

Codex builds the original TCML, TChart and selected CPU Engine Lib files.
Claude Code owns the Windows/MFC/Direct3D value compatibility headers.
No window, renderer, sound, socket or server process is started by these tools.

## Build and run on macOS ARM64

```
sh tools/build-client-native.sh --target tables
sh native/client/tests/check.sh --tables-only
```

The first command builds TCML/TChart, the shared dump program and formatter,
D3DX and parser-stack tests. Apple Clang uses `gnu++14`, ARM64, `-O1`,
`-ffp-contract=off`, ASan, UBSan and fatal sanitizer errors. `--no-sanitize`
builds separately into `build/client/plain/`; checks use the sanitized
build. Every original unit receives the board's three macros and prefix header.
`CXX` can select a compiler wrapper; the default is `clang++`.

`sh tools/build-client-native.sh` also builds the six engine translation units
and packet/compressor/bitstream/BSP tests. Q-B10's compatibility additions are
present at baseline 31908a1; the default build now passes. Run:

```
sh tools/build-client-native.sh
sh native/client/tests/check.sh
```

Do not interpret passing compilation as a playable client. The transport and
overlapped methods of CPacket are excluded from the native slice. The original
packet encryption/checksum methods remain and are tested; no successful stub
transport or encryption method exists.

## Portable Windows reference program

Build **the same** `native/client/tests/dump/dump.cpp` as a console program.
It contains no native-only include or preprocessor branch. It uses standard C++
and the original TCML/TChart public types/APIs with byte-oriented MFC CString.

Windows include directories, relative to the repository root:

* `source/4Story 5.0 Source Files/Lib/Own/TCML/TCML`
* `source/4Story 5.0 Source Files/Lib/Own/TChart/TChart`
* `source/4Story 5.0 Source Files/Lib/Own/TProtocol/include`
* `source/4Story 5.0 Source Files/Lib/3rdParty/DirectX9 (June 2010)/Include`
* The MSVC/MFC headers from the Windows build environment.

Link the original `TCML.lib`, `TChart.lib` and architecture-matching `d3dx9.lib`.
Build Release Win32 and Release x64 with the original libraries' runtime/MFC
settings, byte/MultiByte character set, C++14 or later, `NDEBUG`,
`FOURSTORY_CLIENT_MOD` and **x64 only** `FOURSTORY_CLIENT_64`.
Do not define `FOURSTORY_CLIENT_NATIVE`, `_UNICODE`, `UNICODE`, `_DEBUG` or
`USE_VLD`; do not add native include directories or a native prefix header.
MSVC defines `_WIN64` for x64 normally. Disable precompiled-header use for this
source unless the Windows project supplies a matching precompiled header.

```
c1-dump.exe "C:\path\to\4Story 5.0 Client" "C:\path\to\windows-dump.tsv"
```

Both platforms accept `ASSET_ROOT OUTPUT.tsv`. Paths passed to the program
must identify the same asset version. Missing input/output files return a
nonzero result. Native output is `build/client/sanitized/native-dump.tsv`.
The full layout has over 40 million decoded records; the selected dump is about
1.9 GB. Outputs, libraries and logs are ignored, not committed.

## Dump format and limits

Four tab-separated columns: dataset, key, field path, value. Datasets sort
alphabetically; map keys are numeric, child/list order follows the source.
Floats are eight uppercase hexadecimal digits of their bit pattern. Bytes
outside printable ASCII and backslashes use `\xNN`; output uses LF on Windows
and macOS. The same formatter source is included by the format test.

The dump covers every retained loaded scalar/array in the selected item, skill,
light, fog and global NPC records; environment lists; transformed map nodes;
all string slots; the entire layout tree and the 68-byte font payload fields.
It does not read uninitialized render state or output pointer addresses.

Original APIs discard reserved scalars, NPC template/menu IDs and the raw map
transform inputs. Supplemental `asset.raw/<path>` records preserve their input
bytes; they **do not verify the decoding of values discarded by the libraries**.
The raw 147 MB layout is not duplicated; all of its retained fields are decoded.
Skill function/upgrade/arrow tables are not loaded by this selected dump.
NPC template/menu references are not resolved because those additional tables
are not loaded. This qualification is submitted to Claude Code as Q-B11 before
using the output to close C1-J1.

`asset_coverage.py` enumerates all `.tcd` paths, sizes, hashes and literal
InitChart loader evidence. Run it after a successful dump. Exactly nine selected
files have `PASS_SELECTED_DUMP`; the rest remain `NOT_TESTED`. `UNMAPPED`
means this bounded literal-name search did not identify a loader, not that none
exists. This manifest does not satisfy C1's all-file loading criterion.

Windows build and Windows/native reference comparison: **NOT TESTED by Codex**.

## Extended startup and asset coverage (A-C8)

```
sh tools/build-client-native.sh --target tables
sh native/client/tests/check-extended.sh
```

The extended program loads TString followed by all InitChart tables in client
order, including optional RPS. It serializes retained initialized fields from
71 public roots with 62 typed visitors. It then tries all 146 NPC filename
matches (including three legacy nine-digit names), plus auction, dynamic help,
root titles, legacy pets and cash-shop first-line text. Original TChart readers
are used; no new binary format is invented for unidentified files.

Build `native/client/tests/dump/startup_dump.cpp` on Windows using the same
include/link/definitions above. Keep `dump.cpp` and `startup_fields.inc` beside
it, but compile only startup_dump.cpp for this executable. The original selected
dump is unchanged, so its earlier paired comparison remains reproducible.

```
c1-startup-dump.exe ASSET_ROOT OUTPUT.tsv LOAD_LOG.tsv NPC_LIST.txt
```

Generate the relative NPC list using asset_coverage.py's `--npc-list` option.
Omit NPC_LIST to run startup tables only. With the list, extra readers run after
NPC loading; their table reloads occur after the startup dump. A completed run
can contain rejected NPC assets: consult LOAD_LOG and the extended manifest.
Exit 0 means the dump run completed, not that every asset decoded successfully.
The manifest requires the completion marker, emitted only after dump output
closes successfully. Missing mandatory inputs or unsupported mantle versions
fail before any table loads; negative runtime tests verify these cases.

Current measured coverage: 208 PASS_DECODED, one PASS_TEXT, one EMPTY_ACCEPTED,
eight FAIL_ARCHIVE and eleven NOT_TESTED, out of 229 TCD paths. The 67 startup
TCDs pass ASan/UBSan. No sanitizer report occurred during the full run. Retained
fields are compared; discarded fields still need raw-input agreement, and the
manifest records source hashes. See research/archive/c1/c1-extended-coverage.md for exact
gaps and native artifact hash. Extended Windows comparison is NOT TESTED.
C1's all-file loading criterion remains open.
