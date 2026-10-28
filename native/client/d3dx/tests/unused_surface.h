// Test-only surface vtable; unrelated operations explicitly fail.
#pragma once
#include <d3d9.h>
struct UnusedSurfaceMethods : IDirect3DSurface9 {
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {return D3DERR_NOTAVAILABLE;}
    ULONG AddRef() override {return {};}
    ULONG Release() override {return {};}
    HRESULT GetDevice(struct IDirect3DDevice9** ppDevice) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetPrivateData(REFGUID guid, const void *data, DWORD data_size, DWORD flags) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) override {return D3DERR_NOTAVAILABLE;}
    HRESULT FreePrivateData(REFGUID refguid) override {return D3DERR_NOTAVAILABLE;}
    DWORD SetPriority(DWORD PriorityNew) override {return {};}
    DWORD GetPriority() override {return {};}
    void PreLoad() override {return;}
    D3DRESOURCETYPE GetType() override {return {};}
    HRESULT GetContainer(REFIID riid, void** ppContainer) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetDesc(D3DSURFACE_DESC* pDesc) override {return D3DERR_NOTAVAILABLE;}
    HRESULT LockRect(D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) override {return D3DERR_NOTAVAILABLE;}
    HRESULT UnlockRect() override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetDC(HDC* phdc) override {return D3DERR_NOTAVAILABLE;}
    HRESULT ReleaseDC(HDC hdc) override {return D3DERR_NOTAVAILABLE;}
};
