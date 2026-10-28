# DirectXTex adaptation

`../include/fourstory_bc.h` adapts Microsoft DirectXTex `BC.cpp` and `BC.h`
from commit `1acf4ebe6330e06b7eb52e8db63d4f824ad77427`:
https://github.com/microsoft/DirectXTex/tree/1acf4ebe6330e06b7eb52e8db63d4f824ad77427/DirectXTex

Copyright (c) Microsoft Corporation. MIT license: `DirectXTex-LICENSE.txt`.

The adaptation retains scalar `HDRColorA`, RGB565 quantization, `OptimizeRGB`
and `EncodeBC1`. It removes SAL annotations, SIMD dependencies and unrelated
code. Functions have header linkage in a private namespace. Byte-RGBA and
float-RGBA wrappers encode BC1 and BC2; BC2 adds packed four-bit alpha.
Runtime callers use weighted color, no dithering and BC1 alpha threshold 0.5.
No Windows DLL code is included. Windows readbacks are test evidence only.
The custom DDS decoder and API integration are project code.
