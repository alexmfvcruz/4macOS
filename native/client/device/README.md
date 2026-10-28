# C2 empty Direct3D 9 device

`empty_device.h`/`.cpp` implement `IDirect3DDevice9` (interface:
`native/client/d3d9/include/d3d9.h`, board contract item 11) for task C2-A5:
the resource loaders run against it without a GPU or a window.

Implemented with CPU memory: `CreateTexture` (all mip levels; DXT1 to DXT5
and the uncompressed formats of `format_unit_bytes`), `GetLevelDesc`,
`GetLevelCount`, `GetSurfaceLevel`, `LockRect`/`UnlockRect` (texture and
surface; one lock at a time per level, block-aligned rectangles for DXT),
`CreateOffscreenPlainSurface`, `CreateVertexBuffer`/`CreateIndexBuffer` with
`Lock`/`Unlock`/`GetDesc`, `GetContainer`, COM reference counts,
`TestCooperativeLevel`, `GetDeviceCaps` (no size or power-of-two limit,
`D3DDEVTYPE_NULLREF`).

Not executed: every other method (draw, state, shaders, `Present`, `Reset`,
render targets, private data, ...). Each call is counted in
`statistics().not_executed` and returns `D3DERR_NOTAVAILABLE`, or 0 when the
method returns no `HRESULT`. No caller can take a draw or `Present` as done.

`stubs.inc` is generated: `python3 native/client/device/generate_stubs.py`.
Build and test: `python3 native/client/build/build.py --target device`, then
`build/client/sanitized/c2-empty-device-test` (also in
`native/client/tests/check.sh`).

Behavior that follows Direct3D 9 (critic review 2026-10-15): output
pointers are cleared before validation; `D3DUSAGE_AUTOGENMIPMAP` textures
have one level; render targets, depth buffers and non-dynamic
`D3DPOOL_DEFAULT` textures and surfaces cannot be locked; DXT lock rectangles
cover whole blocks or end at the level edge; `GetSurfaceLevel` returns the
same object for a level while it exists; sizes above 16,384 pixels fail with
`D3DERR_INVALIDCALL` and allocations above 256 MiB or failed allocations with
`D3DERR_OUTOFVIDEOMEMORY`.

Assumed, to check against Windows in C2-A6: the `GetDeviceCaps` values; the
tight row pitch (no 4-byte alignment); which texture formats a Windows
driver accepts; nested buffer locks. Other differences: memory of a new
resource is zero-filled; `AddDirtyRect` and `PreLoad` do nothing.
`statistics()` counts creations, failed creations, created and live bytes,
live objects and each not-executed method by interface and name.
`native/client/tests/check.sh` verifies that `stubs.inc` matches `d3d9.h`.
