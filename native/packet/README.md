# First native packet foundation

Run `sh tools/check-packet.sh` from the project root. The macOS test compiles with
C++17, warnings as errors, AddressSanitizer and UndefinedBehaviorSanitizer.

`wire.hpp` is an isolated encoder/decoder, not yet integrated into TNetLib. It
encodes a 16-byte unencrypted header and explicitly little-endian fixed-width
fields. The legacy Windows long is represented by int32_t, not the host long.
Strings are length-prefixed bytes without a terminator; no code-page conversion
is performed. Invalid inputs throw instead of emulating legacy zero-fill reads.

## Fixture provenance

`fixtures/scalars.hex` is source-derived, not captured from a running game:

- Header: size, message ID 0x1234, zero sequence, zero checksum.
- Fields in order: BYTE 0xab, WORD 0xcdef, DWORD 0x12345678, Windows long -2,
  float 1.5, INT64 0x0102030405060708, LPCTSTR "4Story".
- Independent construction: Python struct packing `<HHIq` for the header,
  `<BHiifq` for scalars, followed by `<i` length 6 and ASCII bytes.
- Source: original Packet.h header fields and Packet.cpp scalar/string writes,
  constructor/Clear/Write behavior. Both TNetLib copies have identical Packet files.

The 16-byte header assumes the original Windows compiler's normal layout; the
reference program checks this explicitly. Source-derived expected bytes are not
proof of compatibility with the supplied executable or a successful game login.

Build `tests/windows_reference.cpp` on Windows with the original TNetLib include
directory and library, using Release Win32/MultiByte. Configure ODBC/Winsock and
other library dependencies as in the original projects. Its stdout should equal
`fixtures/scalars.hex`. Save the output, exact build command and source/library
hashes when that check is performed. It compiled and ran successfully with MSVC x86 on 2026-10-02; output matched exactly. See research/database/windows-packet-validation.md for the recorded command and scope.

## Remaining scope

No transport, database access, service startup, string code-page conversion,
packet encryption/checksum, sequence verification or game handler is ported here.
The original Session implementation performs additional transformations; the
plaintext fixture does not validate them. Add reference vectors before migrating
those behaviors. Original game source remains unchanged.
