// Unit test of the C2 empty device (ASan/UBSan in native/client/build/build.py).
#include "../empty_device.h"
#include <cstdio>
#include <cstring>
using namespace fourstory::device;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)
int main() {
    IDirect3DDevice9* device = nullptr;
    CHECK(SUCCEEDED(CreateEmptyDevice(&device)) && device);
    // Texture with the full mip chain, DXT1 layout and locked bytes.
    IDirect3DTexture9* texture = nullptr;
    CHECK(device->CreateTexture(64, 16, 0, 0, D3DFMT_DXT1, D3DPOOL_MANAGED, &texture, nullptr) == D3D_OK);
    CHECK(texture->GetLevelCount() == 7);
    D3DSURFACE_DESC desc; CHECK(texture->GetLevelDesc(6, &desc) == D3D_OK && desc.Width == 1 && desc.Height == 1 && desc.Format == D3DFMT_DXT1);
    D3DLOCKED_RECT locked;
    CHECK(texture->LockRect(0, &locked, nullptr, 0) == D3D_OK && locked.Pitch == 16 * 8);
    std::memset(locked.pBits, 0xAB, size_t(locked.Pitch) * 4);
    CHECK(texture->LockRect(0, &locked, nullptr, 0) == D3DERR_INVALIDCALL); // already locked
    CHECK(texture->UnlockRect(0) == D3D_OK && texture->UnlockRect(0) == D3DERR_INVALIDCALL);
    RECT r = {4, 4, 8, 8};
    CHECK(texture->LockRect(0, &locked, &r, 0) == D3D_OK && reinterpret_cast<BYTE*>(locked.pBits)[0] == 0xAB); texture->UnlockRect(0);
    RECT bad = {1, 0, 4, 4}; CHECK(texture->LockRect(0, &locked, &bad, 0) == D3DERR_INVALIDCALL);
    // Surface of a level keeps the texture alive and sees the same bytes.
    IDirect3DSurface9* surface = nullptr;
    CHECK(texture->GetSurfaceLevel(0, &surface) == D3D_OK);
    CHECK(texture->Release() == 1);
    CHECK(surface->LockRect(&locked, nullptr, 0) == D3D_OK && reinterpret_cast<BYTE*>(locked.pBits)[0] == 0xAB && surface->UnlockRect() == D3D_OK);
    IDirect3DTexture9* container = nullptr;
    CHECK(surface->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&container)) == D3D_OK && container);
    container->Release();
    CHECK(surface->Release() == 0);
    // Uncompressed pitch, mip sizes and invalid requests.
    CHECK(device->CreateTexture(5, 3, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr) == D3D_OK);
    CHECK(texture->GetLevelCount() == 3 && texture->GetLevelDesc(1, &desc) == D3D_OK && desc.Width == 2 && desc.Height == 1);
    CHECK(texture->LockRect(2, &locked, nullptr, 0) == D3D_OK && locked.Pitch == 4); texture->UnlockRect(2);
    texture->Release();
    CHECK(device->CreateTexture(4, 4, 9, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr) == D3DERR_INVALIDCALL);
    CHECK(device->CreateTexture(4, 4, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED, &texture, nullptr) == D3DERR_INVALIDCALL && !texture);
    // Buffers.
    IDirect3DVertexBuffer9* vb = nullptr; void* data = nullptr;
    CHECK(device->CreateVertexBuffer(48, 0, D3DFVF_XYZ, D3DPOOL_MANAGED, &vb, nullptr) == D3D_OK);
    CHECK(vb->Lock(0, 0, &data, 0) == D3D_OK && data && vb->Unlock() == D3D_OK && vb->Unlock() == D3DERR_INVALIDCALL);
    CHECK(vb->Lock(40, 16, &data, 0) == D3DERR_INVALIDCALL);
    D3DVERTEXBUFFER_DESC vdesc; CHECK(vb->GetDesc(&vdesc) == D3D_OK && vdesc.Size == 48 && vdesc.FVF == D3DFVF_XYZ);
    vb->Release();
    IDirect3DIndexBuffer9* ib = nullptr;
    CHECK(device->CreateIndexBuffer(12, 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, nullptr) == D3D_OK);
    D3DINDEXBUFFER_DESC idesc; CHECK(ib->GetDesc(&idesc) == D3D_OK && idesc.Size == 12 && idesc.Format == D3DFMT_INDEX16);
    ib->Release();
    CHECK(device->CreateIndexBuffer(12, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &ib, nullptr) == D3DERR_INVALIDCALL);
    IDirect3DSurface9* plain = nullptr;
    CHECK(device->CreateOffscreenPlainSurface(3, 2, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &plain, nullptr) == D3D_OK);
    CHECK(plain->GetContainer(__uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&container)) == E_NOINTERFACE);
    plain->Release();
    // Draw, state and Present are not executed and are counted.
    CHECK(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0) == D3DERR_NOTAVAILABLE);
    CHECK(device->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_NOTAVAILABLE);
    CHECK(device->SetRenderState(D3DRS_ZENABLE, FALSE) == D3DERR_NOTAVAILABLE);
    CHECK(statistics().not_executed.at("IDirect3DDevice9::Clear") == 1 && statistics().not_executed.at("IDirect3DDevice9::Present") == 1);
    CHECK(statistics().textures == 2 && statistics().vertex_buffers == 1 && statistics().index_buffers == 1 && statistics().failed_creations == 3);
    // Review fixes: same level surface, outputs cleared, size limits, pools.
    CHECK(device->CreateTexture(8, 8, 0, 0, D3DFMT_DXT5, D3DPOOL_MANAGED, &texture, nullptr) == D3D_OK);
    IDirect3DSurface9 *first = nullptr, *second = nullptr;
    CHECK(texture->GetSurfaceLevel(1, &first) == D3D_OK && texture->GetSurfaceLevel(1, &second) == D3D_OK && first == second);
    CHECK(first->Release() == 1 && second->Release() == 0);
    CHECK(texture->GetSurfaceLevel(9, &first) == D3DERR_INVALIDCALL && first == nullptr);
    RECT edge = {4, 4, 8, 8}, ragged = {0, 0, 5, 5};
    CHECK(texture->LockRect(0, &locked, &edge, 0) == D3D_OK); texture->UnlockRect(0);
    CHECK(texture->LockRect(0, &locked, &ragged, 0) == D3DERR_INVALIDCALL && locked.pBits == nullptr);
    texture->Release();
    texture = reinterpret_cast<IDirect3DTexture9*>(0x1);
    CHECK(device->CreateTexture(65535, 65535, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr) == D3DERR_INVALIDCALL && texture == nullptr);
    CHECK(device->CreateTexture(16384, 16384, 1, 0, D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &texture, nullptr) == D3DERR_OUTOFVIDEOMEMORY && texture == nullptr);
    vb = reinterpret_cast<IDirect3DVertexBuffer9*>(0x1);
    CHECK(device->CreateVertexBuffer(0xFFFFFFFFu, 0, 0, D3DPOOL_MANAGED, &vb, nullptr) == D3DERR_OUTOFVIDEOMEMORY && vb == nullptr);
    CHECK(device->CreateTexture(64, 32, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr) == D3D_OK);
    CHECK(texture->GetLevelCount() == 1 && texture->LockRect(0, &locked, nullptr, 0) == D3DERR_INVALIDCALL);
    texture->Release();
    CHECK(device->CreateTexture(4, 4, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, nullptr) == D3D_OK);
    CHECK(texture->LockRect(0, &locked, nullptr, 0) == D3D_OK && texture->UnlockRect(0) == D3D_OK);
    texture->Release();
    CHECK(device->CreateIndexBuffer(8, 0, D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib, nullptr) == D3D_OK);
    CHECK(ib->SetPrivateData(__uuidof(IUnknown), nullptr, 0, 0) == D3DERR_NOTAVAILABLE);
    CHECK(statistics().not_executed.count("IDirect3DIndexBuffer9::SetPrivateData") == 1);
    ib->Release();
    CHECK(statistics().live_bytes == 0 && statistics().bytes_created > 0);
    CHECK(device->Release() == 0);
    CHECK(statistics().live_objects == 0);
    std::printf(failures ? "FAILED %d checks\n" : "PASS empty_device_test\n", failures);
    return failures ? 1 : 0;
}
