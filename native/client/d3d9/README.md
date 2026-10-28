# Shared Direct3D 9 interface (C3-B1)

`include/d3d9.h` is the unmodified complete Direct3D 9 / 9Ex interface used
by the selected DXVK Native macOS candidate. It contains declarations, not a
device implementation. C2's empty device and C3's future real backend share
this interface. No backend was built or installed for this delivery.

## Origin and licences

- Backend: [generalsx-project/dxvk](https://github.com/generalsx-project/dxvk/tree/f66da559fd48cd55ddfebca38ed0f708c0f76a24),
  commit `f66da559fd48cd55ddfebca38ed0f708c0f76a24`, version 2.6.0.
  No release tag identifies this patched commit. GeneralsX's cmake/dx8.cmake
  pins it. Upstream 3.1.1 uses the same DirectX-header submodule revision.
- DirectX headers: [Joshua-Ashton/mingw-directx-headers](https://github.com/Joshua-Ashton/mingw-directx-headers/tree/9df86f2341616ef1888ae59919feaa6d4fad693d),
  commit `9df86f2341616ef1888ae59919feaa6d4fad693d`.
  `d3d9.h`, `d3d9caps.h`, `upstream/d3d9types.h` are byte-for-byte copies.
  Their original copyright notices remain. Licence: LGPL-2.1-or-later.
- `pshpack4.h`/`poppack.h` come from the selected DXVK native Windows shim.
  DXVK and the adapted COM declaration bridge use the zlib/libpng licence.
- `licenses/` contains the upstream MinGW notices, LGPL 2.1 text and DXVK
  licence. `origin.json` records exact URLs and SHA-256 values.

## Native include contract

Put `native/client/d3d9/include` BEFORE `native/client/compat/include`, then
`native/client/d3dx/include`. Keep the existing native project macros/prefix.
All translation units sharing D3D values must use the same include order;
do not mix already-included compatibility d3d9types.h with the full header.
The full header supplies the Direct3D value types as well as interfaces.

```
clang++ -std=c++14 -DFOURSTORY_CLIENT_NATIVE \
  -DFOURSTORY_CLIENT_64 -DFOURSTORY_CLIENT_MOD \
  -include fourstory_client_prefix.h \
  -I native/client/d3d9/include -I native/client/compat/include \
  -I native/client/d3dx/include ...
```

The local `objbase.h` is a C++ declaration bridge adapted from DXVK's
windows_base.h/unknwn.h. It reuses the project's Win32 types and supplies
GUID, COM declarations and auxiliary D3D types. Importing DXVK's entire
windows.h alongside the project windows.h would redefine structures and
pointer types. The small d3d9types.h wrapper adds these prerequisites before
the unmodified upstream value header; this permits D3DX-first includes.
C and CINTERFACE mode are not supported by this bridge. C++ COM has no
virtual destructor, matching DXVK's three-slot IUnknown. No successful
operation stub is present.

Project HWND/HDC remain typed pointer handles, whereas DXVK uses void*.
Their pointer ABI and tested D3D structure offsets agree on ARM64. The
project WCHAR is 16-bit; DXVK's native shim uses 32-bit wchar_t. Consequently
D3DPERF wide-string entry points are outside this bridge's supported ABI.
Do not call those exports through this header without an explicit conversion
boundary. Device methods and the tested D3D value structures use no WCHAR.

## Verification

```
sh native/client/d3d9/check.sh
```

Uses only the existing Apple Clang. Compares data sizes, alignment, selected
pointer-field offsets and device IID against the original selected DXVK
native shim; checks CreateTexture's member signature and IUnknown's lack of
a destructor slot. Compiles/executes both project MFC/D3DX include orders.
The original source checkout must be at the recorded pin (optional first
argument selects its directory). This is header verification, not device,
linker, Windows, shader or rendering verification. Windows build NOT TESTED.

## Apple sampler LOD bias

`patches/sampler-lod-bias.patch` targets the pinned DXVK checkout. The C3 build
applies and verifies it before building. `d3d9.emulateSamplerLodBias` defaults to
true on Apple, false elsewhere. Shader sampling implements material LOD bias
when Metal cannot apply Vulkan sampler bias. Disable only for diagnostic controls.
See `research/c3-texture-sharpness.md` for exact native/Windows oracle results,
original-scene evidence and remaining coverage limits. This patch changes no
original game source or textures.

## Command-list recycling (MoltenVK completion crash, Q-B35)

`patches/dxvk-keep-command-lists.patch` targets the DXVK checkout that builds
`deps/dxvk` (`c3-sources/dxvk-macos`). MoltenVK returns temporary Metal
buffers to the command pool from Metal completion handlers; its timeline
semaphore signals on the GPU (`encodeSignalEvent`), so DXVK can see a
submission finish before those handlers run. DXVK's 16-entry recycler then
destroyed surplus command lists and their `VkCommandPool`, and a late handler
locked a destroyed mutex (SIGABRT in `MVKMTLBufferAllocationPool::returnAllocation`).
The patch keeps every returned command list for reuse (growth bounded by the
peak number in flight) and prints `DXVK_RECYCLER kept=N` for each new peak
above 16. Remaining: the same race can still occur once at device teardown
(game exit); a MoltenVK-side fix would remove it.
Rebuild: `PATH="deps/tools/ninja:deps/vulkan-sdk/macOS/bin:$PATH" ninja -C deps/dxvk src/d3d9/libdxvk_d3d9.0.dylib`.
