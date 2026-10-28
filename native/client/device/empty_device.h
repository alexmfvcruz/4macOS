// C2 empty Direct3D 9 device (task C2-A5). It implements the resource part of
// IDirect3DDevice9 with CPU memory: textures with all mip levels, surfaces of
// texture levels, offscreen plain surfaces, vertex and index buffers, Lock and
// Unlock, descriptions and COM reference counts. It draws nothing. Every other
// method is counted and fails with D3DERR_NOTAVAILABLE (or returns 0), so no
// caller can take a draw, state change or Present as done.
// Interface: native/client/d3d9/include/d3d9.h (board contract item 11).
#pragma once
#include <d3d9.h>
#include <map>
#include <string>

namespace fourstory { namespace device {

struct Statistics {
    // Calls of methods that the empty device does not execute, by name.
    std::map<std::string, unsigned long long> not_executed;
    // Resources created successfully (a texture level surface counts once).
    unsigned long long textures = 0, surfaces = 0, vertex_buffers = 0, index_buffers = 0;
    // CPU bytes allocated in total, and bytes of resources that still exist.
    unsigned long long bytes_created = 0;
    long long live_bytes = 0;
    // Creation requests that failed (unsupported format, invalid size, ...).
    unsigned long long failed_creations = 0;
    // Objects that are still referenced.
    long long live_objects = 0;
};

// Creates a device with reference count 1. Release it with Release().
// Assumed values, not Windows readbacks (check in C2-A6): GetDeviceCaps
// (16384 texture limit, D3DDEVTYPE_NULLREF, unlimited index and primitive
// counts, other fields 0); tight row pitch (width x bytes per pixel, no
// 4-byte alignment); the accepted texture formats of format_unit_bytes.
HRESULT CreateEmptyDevice(IDirect3DDevice9** device);

const Statistics& statistics();
void reset_statistics();

// Bytes per pixel (uncompressed) or per 4x4 block (DXT); 0 if unsupported.
UINT format_unit_bytes(D3DFORMAT format, bool* block_compressed);
// Row pitch and row count of one level in bytes and rows.
bool level_layout(D3DFORMAT format, UINT width, UINT height, UINT* pitch, UINT* rows);

}}
