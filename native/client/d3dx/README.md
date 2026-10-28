# Native D3DX subset

`include/d3dx9.h` supplies row-vector math. With the canonical D3D9 include
path first (board contract item11), it also exposes device-backed DDS functions
in `d3dx9tex.h`. C1's existing math-only include path remains supported.
No library link change is needed: functions are inline.

Q-C11 items1/2:

- `D3DXGetImageInfoFromFileInMemory`: validated legacy 2D DDS only.
- `D3DXCreateTextureFromFileInMemoryEx`: file-mip upload through
  the supplied device. The engine's default sizes, explicit file mip count,
  managed pool, FILTER_NONE and default mip filter are supported.
- Eight added math functions: quaternion axis/identity, quaternion matrix,
  full centered transformation, transpose, vector lerp/2D length, sphere ray.

DDS source formats: DXT1–DXT5; RGB/BGR 32-bit A/X8, RGB24, R5G6B5,
A/X1R5G5B5, A/X4R4G4B4, A8, L8, A8L8 with canonical masks. Full same-format compressed blocks retain bytes. DXT1/DXT3 tiny mips
are periodically expanded and re-encoded, as measured against original D3DX.
Uncompressed channels can convert to supported RGB/alpha outputs or DXT1/DXT3;
DXT1/DXT3 can decode to RGB/alpha. The scalar encoder is adapted from MIT
DirectXTex (see `third_party/DirectXTex-NOTICE.md`). Disable floating-point
contraction (`-ffp-contract=off`) for the measured encoding results.
Uncompressed default NPOT dimensions round up to powers of two; FILTER_NONE
preserves the source at the top left and zero-fills padding. Compressed NPOT
inputs, arbitrary resizing, compressed transcoding and luminance conversion
are unsupported. The parser supports exact tight lower
mips or legacy DWORD-aligned lower mips, inferred from complete payload size;
it honors top-level pitch when present.

Unsupported input/options fail explicitly with D3DXERR_INVALIDDATA: non-DDS,
DX10/cube/volume/palette layouts, unrecognized masks, truncated/extra data,
arbitrary resizing, generated/skipped mips, filter variants, palettes, color keys and
unsupported format conversion. Invalid call pointers return D3DERR_INVALIDCALL;
device HRESULTs propagate. Output texture is null on failure and partial
resources are released. This is a bounded subset, not all Windows D3DX.

```
sh native/client/d3dx/check.sh
python3 native/client/d3dx/tests/extract_dds.py
runtime/legacy/d3dx-check/dds_test runtime/legacy/d3dx-check/assets/*.dds
python3 native/client/d3dx/gpu-check.py
```

Checks require the canonical header include path; GPU integration reuses the
approved local C3 dependencies and installs nothing. CPU test devices are
isolated test fixtures; unused methods fail, and no draw/transport is simulated.
GPU integration checks actual DXVK texture creation and mip-byte readback,
not visible rendering. Full logs/fixtures remain in `runtime/legacy/d3dx-check/`.

Representative asset extraction identifies DDS/tag mismatches. The runtime
loader requests ARGB for Effects ID30023 despite a DXT3 payload (supported).
Chakra ID1000000005 requests DXT1 from ARGB, and unknown-tag Effects records
request DXT3 from ARGB. These conversions now execute; no asset-specific
runtime lookup is used. The portable `tests/conversion_reference.cpp` captures
original Windows D3DX9_43 readbacks. The expanded 224-case set matches all 871
x64 mip files exactly. Win32 differs in 78 mip files; original DLL arithmetic
is architecture-dependent. See `research/archive/c2/c2-texture-conversion.md`.

Q-C11 item3 now adds `D3DXLoadSurfaceFromFileInMemory` (DDS only),
`D3DXLoadSurfaceFromSurface` and `D3DXCreateTexture`.
Surface copies accept FILTER_NONE, exact-size rectangles, lockable source and
destination, no palette or color key. Source snapshots make overlap safe.
RGB/alpha conversion and BC1/BC3 decoding share the DDS rules. Compressed raw
copies require block-aligned origins and boundaries, allowing partial blocks
at each surface edge. No resizing, non-DDS decoder, staging of non-lockable
surfaces or surface compressed encoding is implemented. Unsupported cases fail.
CreateTexture normalizes documented default/zero dimensions and mip counts,
then calls the supplied device. It does not select alternate formats or emulate
D3DXCheckTextureRequirements capability adjustments; device failures propagate.
The inspected loader calls use explicit sizes/formats and scratch/managed pools.
The GPU integration additionally checks DDS-to-scratch surface loading and
scratch-to-managed texture copies for all nine representative DDS records.

Q-C13 adds quaternion compound multiplication, VECTOR4/Vec3Transform,
16-byte-aligned MATRIXA16, left-handed projection/view matrices, yaw/pitch/roll,
and plane transformation. The plane function consumes the caller's supplied
inverse-transpose. Quaternion composition preserves D3DX's q2*q1 order.
ID3DXBuffer is declared for existing call sites; save-to-file/memory APIs
return D3DERR_NOTAVAILABLE, with the memory output cleared. They do not save.
The portable c13_math_test runs against native and original Windows D3DX9_43:
both x64/Win32 pass. The committed 72-float comparisons retain all bit
mismatches; maximum absolute difference is below 0.000001. This is numerical
agreement for these cases, not byte-identical math or full camera validation.

C2 canonical validation selects the original Windows x64 SSE arithmetic path
for Vec3Normalize, QuaternionInverse and quaternion products. Component sum
order and product grouping are preserved explicitly; `-ffp-contract=off` is
required. `tests/c2_math_reference.cpp` checks 870 exact-bit Windows captures
(including alias checks), not a tolerance. Win32 dispatch remains a separately
reported reference. See `research/archive/c2/c2-math-parity.md` for the binary identity,
observed dispatch targets and full-animation evidence.

Q-C14 adds Vec2 normalize/dot/squared length, Vec3 squared length and Vec2 scalar
compound multiplication/division. Inline arithmetic follows the bundled SDK,
including reciprocal-then-multiply division. Vec2 normalization uses the existing
x64 Vec3 reciprocal convention and supports in-place use. The new portable
`tests/c14_vectors_test.cpp` runs under ASan/UBSan in `check.sh`; its Windows
runtime comparison is pending. See `research/archive/c4/c4-q-c14.md`.
