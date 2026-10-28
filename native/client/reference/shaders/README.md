# Original client shader bytecode (Windows reference)

The 27 client shaders (`Client/TClient/res/*.vsh`, `*.psh`; resource type
`SHADER` in `English_TClient.rc`) assembled with the call of
`CD3DDevice` (`Engine Lib/D3DDevice.cpp:883-889`, `:918`): the resource bytes
and size, no defines, no include handler, flags 0. The assembler is the
`D3DX9_43.dll` of the game release (file version 9.29.952.3111). Program:
`assemble-windows.cpp`; script `runtime/legacy/win/c3-shaders.ps1`.

`manifest.json` records, for each shader, the source size and SHA-256, the
`HRESULT` and the bytecode size and SHA-256. All 27 returned `S_OK`
(VERIFIED (RUNTIME), 2026-10-14/15). Only the x86 build of the assembler
program ran; the bytecode does not depend on the calling architecture
(INFERRED: D3DX assembles to a fixed token format).
