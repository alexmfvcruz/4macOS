// Test-only texture vtable: unrelated methods fail explicitly.
#pragma once
#include <d3d9.h>
struct UnusedTextureMethods : IDirect3DTexture9 {
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
    DWORD SetLOD(DWORD LODNew) override {return {};}
    DWORD GetLOD() override {return {};}
    DWORD GetLevelCount() override {return {};}
    HRESULT SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override {return D3DERR_NOTAVAILABLE;}
    D3DTEXTUREFILTERTYPE GetAutoGenFilterType() override {return {};}
    void GenerateMipSubLevels() override {return;}
    HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) override {return D3DERR_NOTAVAILABLE;}
    HRESULT LockRect(UINT level, D3DLOCKED_RECT *locked_rect, const RECT *rect, DWORD flags) override {return D3DERR_NOTAVAILABLE;}
    HRESULT UnlockRect(UINT Level) override {return D3DERR_NOTAVAILABLE;}
    HRESULT AddDirtyRect(const RECT *dirty_rect) override {return D3DERR_NOTAVAILABLE;}
};
