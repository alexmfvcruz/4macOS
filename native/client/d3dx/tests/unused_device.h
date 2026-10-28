// Test-only device vtable: unrelated methods fail; never a transport/render stub.
#pragma once
#include <d3d9.h>
struct UnusedDeviceMethods : IDirect3DDevice9 {
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {return D3DERR_NOTAVAILABLE;}
    ULONG AddRef() override {return {};}
    ULONG Release() override {return {};}
    HRESULT TestCooperativeLevel() override {return D3DERR_NOTAVAILABLE;}
    UINT GetAvailableTextureMem() override {return {};}
    HRESULT EvictManagedResources() override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetDirect3D(IDirect3D9** ppD3D9) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetDeviceCaps(D3DCAPS9* pCaps) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) override {return D3DERR_NOTAVAILABLE;}
    void SetCursorPosition(int X,int Y, DWORD Flags) override {return;}
    WINBOOL ShowCursor(WINBOOL bShow) override {return {};}
    HRESULT CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) override {return D3DERR_NOTAVAILABLE;}
    UINT GetNumberOfSwapChains() override {return {};}
    HRESULT Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) override {return D3DERR_NOTAVAILABLE;}
    HRESULT Present(const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override, const RGNDATA *dirty_region) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetDialogBoxMode(WINBOOL bEnableDialogs) override {return D3DERR_NOTAVAILABLE;}
    void SetGammaRamp(UINT swapchain_idx, DWORD flags, const D3DGAMMARAMP *ramp) override {return;}
    void GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) override {return;}
    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, WINBOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT UpdateSurface(IDirect3DSurface9 *src_surface, const RECT *src_rect, IDirect3DSurface9 *dst_surface, const POINT *dst_point) override {return D3DERR_NOTAVAILABLE;}
    HRESULT UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) override {return D3DERR_NOTAVAILABLE;}
    HRESULT StretchRect(IDirect3DSurface9 *src_surface, const RECT *src_rect, IDirect3DSurface9 *dst_surface, const RECT *dst_rect, D3DTEXTUREFILTERTYPE filter) override {return D3DERR_NOTAVAILABLE;}
    HRESULT ColorFill(IDirect3DSurface9 *surface, const RECT *rect, D3DCOLOR color) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) override {return D3DERR_NOTAVAILABLE;}
    HRESULT BeginScene() override {return D3DERR_NOTAVAILABLE;}
    HRESULT EndScene() override {return D3DERR_NOTAVAILABLE;}
    HRESULT Clear(DWORD rect_count, const D3DRECT *rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override {return D3DERR_NOTAVAILABLE;}
    HRESULT MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX *matrix) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetViewport(const D3DVIEWPORT9 *viewport) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetViewport(D3DVIEWPORT9* pViewport) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetMaterial(const D3DMATERIAL9 *material) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetMaterial(D3DMATERIAL9* pMaterial) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetLight(DWORD index, const D3DLIGHT9 *light) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetLight(DWORD Index, D3DLIGHT9*) override {return D3DERR_NOTAVAILABLE;}
    HRESULT LightEnable(DWORD Index, WINBOOL Enable) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetLightEnable(DWORD Index, WINBOOL* pEnable) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetClipPlane(DWORD index, const float *plane) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetClipPlane(DWORD Index, float* pPlane) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) override {return D3DERR_NOTAVAILABLE;}
    HRESULT BeginStateBlock() override {return D3DERR_NOTAVAILABLE;}
    HRESULT EndStateBlock(IDirect3DStateBlock9** ppSB) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetClipStatus(const D3DCLIPSTATUS9 *clip_status) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetClipStatus(D3DCLIPSTATUS9* pClipStatus) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override {return D3DERR_NOTAVAILABLE;}
    HRESULT ValidateDevice(DWORD* pNumPasses) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetPaletteEntries(UINT palette_idx, const PALETTEENTRY *entries) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetPaletteEntries(UINT PaletteNumber,PALETTEENTRY* pEntries) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetCurrentTexturePalette(UINT PaletteNumber) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetCurrentTexturePalette(UINT *PaletteNumber) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetScissorRect(const RECT *rect) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetScissorRect(RECT* pRect) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetSoftwareVertexProcessing(WINBOOL bSoftware) override {return D3DERR_NOTAVAILABLE;}
    WINBOOL GetSoftwareVertexProcessing() override {return {};}
    HRESULT SetNPatchMode(float nSegments) override {return D3DERR_NOTAVAILABLE;}
    float GetNPatchMode() override {return {};}
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT primitive_count, const void *data, UINT stride) override {return D3DERR_NOTAVAILABLE;}
    HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT min_vertex_idx, UINT vertex_count, UINT primitive_count, const void *index_data, D3DFORMAT index_format, const void *data, UINT stride) override {return D3DERR_NOTAVAILABLE;}
    HRESULT ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateVertexDeclaration(const D3DVERTEXELEMENT9 *elements, IDirect3DVertexDeclaration9 **declaration) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetFVF(DWORD FVF) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetFVF(DWORD* pFVF) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateVertexShader(const DWORD *byte_code, IDirect3DVertexShader9 **shader) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetVertexShader(IDirect3DVertexShader9* pShader) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetVertexShader(IDirect3DVertexShader9** ppShader) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetVertexShaderConstantF(UINT reg_idx, const float *data, UINT count) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetVertexShaderConstantI(UINT reg_idx, const int *data, UINT count) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetVertexShaderConstantB(UINT reg_idx, const WINBOOL *data, UINT count) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetVertexShaderConstantB(UINT StartRegister, WINBOOL* pConstantData, UINT BoolCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetStreamSourceFreq(UINT StreamNumber, UINT Divider) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetStreamSourceFreq(UINT StreamNumber, UINT* Divider) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetIndices(IDirect3DIndexBuffer9* pIndexData) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetIndices(IDirect3DIndexBuffer9** ppIndexData) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreatePixelShader(const DWORD *byte_code, IDirect3DPixelShader9 **shader) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetPixelShader(IDirect3DPixelShader9* pShader) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetPixelShader(IDirect3DPixelShader9** ppShader) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetPixelShaderConstantF(UINT reg_idx, const float *data, UINT count) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetPixelShaderConstantI(UINT reg_idx, const int *data, UINT count) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT SetPixelShaderConstantB(UINT reg_idx, const WINBOOL *data, UINT count) override {return D3DERR_NOTAVAILABLE;}
    HRESULT GetPixelShaderConstantB(UINT StartRegister, WINBOOL* pConstantData, UINT BoolCount) override {return D3DERR_NOTAVAILABLE;}
    HRESULT DrawRectPatch(UINT handle, const float *segment_count, const D3DRECTPATCH_INFO *patch_info) override {return D3DERR_NOTAVAILABLE;}
    HRESULT DrawTriPatch(UINT handle, const float *segment_count, const D3DTRIPATCH_INFO *patch_info) override {return D3DERR_NOTAVAILABLE;}
    HRESULT DeletePatch(UINT Handle) override {return D3DERR_NOTAVAILABLE;}
    HRESULT CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) override {return D3DERR_NOTAVAILABLE;}
};
