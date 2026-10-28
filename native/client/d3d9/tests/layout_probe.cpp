// Compare public data layouts against the selected DXVK Native header bundle.
#include <d3d9.h>
#include <cstddef>
#include <iostream>
#include <type_traits>
static_assert(sizeof(DWORD)==4 && sizeof(BOOL)==4 && sizeof(GUID)==16,"Win32 widths");
static_assert(std::is_abstract<IDirect3DDevice9>::value,"COM interface must remain abstract");
static_assert(!std::has_virtual_destructor<IUnknown>::value,"IUnknown has three slots, no destructor slot");
static_assert(std::is_same<decltype(&IDirect3DDevice9::CreateTexture),
    HRESULT(IDirect3DDevice9::*)(UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**,HANDLE*)>::value,"CreateTexture signature");
static_assert(D3DERR_INVALIDCALL==HRESULT(0x8876086c),"D3D HRESULT macros");
static_assert(D3D_OK==0 && E_NOINTERFACE==HRESULT(0x80004002) && E_NOTIMPL==HRESULT(0x80004001) && E_POINTER==HRESULT(0x80004003),"COM failure constants");
int main(){
#define SIZE(T) std::cout<<#T<<'\t'<<sizeof(T)<<'\t'<<alignof(T)<<'\n'
#define OFFSET(T,F) std::cout<<#T "." #F<<'\t'<<offsetof(T,F)<<'\n'
    SIZE(GUID);SIZE(LUID);SIZE(RECT);SIZE(PALETTEENTRY);SIZE(RGNDATAHEADER);
    SIZE(D3DVECTOR);SIZE(D3DMATRIX);SIZE(D3DVIEWPORT9);SIZE(D3DMATERIAL9);SIZE(D3DLIGHT9);
    SIZE(D3DVERTEXELEMENT9);SIZE(D3DCAPS9);SIZE(D3DADAPTER_IDENTIFIER9);SIZE(D3DPRESENT_PARAMETERS);
    SIZE(D3DSURFACE_DESC);SIZE(D3DVOLUME_DESC);SIZE(D3DLOCKED_RECT);SIZE(D3DLOCKED_BOX);
    OFFSET(D3DPRESENT_PARAMETERS,hDeviceWindow);OFFSET(D3DPRESENT_PARAMETERS,Windowed);
    OFFSET(D3DLOCKED_RECT,pBits);OFFSET(D3DADAPTER_IDENTIFIER9,DriverVersion);
    std::cout<<"IDirect3DDevice9 GUID\t"<<std::hex<<IID_IDirect3DDevice9.Data1<<'\n';
}
