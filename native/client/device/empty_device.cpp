// C2 empty Direct3D 9 device. See empty_device.h for the contract.
#include "empty_device.h"
#include "stubs.inc"
#include <cstring>
#include <new>
#include <memory>
#include <vector>

namespace fourstory { namespace device {
namespace {
Statistics stats;

HRESULT not_executed(const std::string& name) { ++stats.not_executed[name]; return D3DERR_NOTAVAILABLE; }
HRESULT failed(HRESULT result) { ++stats.failed_creations; return result; }
const UINT max_texture_size = 16384;      // reported in GetDeviceCaps
const UINT max_buffer_bytes = 0x10000000u; // 256 MiB per buffer or surface level
// Allocates zero-filled memory; false when the allocation fails.
bool allocate(std::vector<BYTE>& bytes, size_t count) {
    try { bytes.assign(count, 0); } catch (const std::bad_alloc&) { return false; }
    stats.bytes_created += count; stats.live_bytes += count;
    return true;
}
void free_bytes(std::vector<BYTE>& bytes) { stats.live_bytes -= bytes.size(); std::vector<BYTE>().swap(bytes); }

// Reference counting and the IDirect3DResource9 members shared by all objects.
template<class Interface> class Object : public Interface {
public:
    explicit Object(IDirect3DDevice9* device) : device_(device) { ++stats.live_objects; if (device_) device_->AddRef(); }
    virtual ~Object() { --stats.live_objects; if (device_) device_->Release(); }
    ULONG AddRef() override { return ++references_; }
    ULONG Release() override { const ULONG left = --references_; if (!left) delete this; return left; }
protected:
    IDirect3DDevice9* device_;
    ULONG references_ = 1;
    DWORD priority_ = 0;
};
#define FOURSTORY_RESOURCE_MEMBERS(TYPE) \
    HRESULT GetDevice(struct IDirect3DDevice9** ppDevice) override { \
        if (!ppDevice) return D3DERR_INVALIDCALL; *ppDevice = device_; device_->AddRef(); return D3D_OK; } \
    DWORD SetPriority(DWORD PriorityNew) override { const DWORD old = priority_; priority_ = PriorityNew; return old; } \
    DWORD GetPriority() override { return priority_; } \
    void PreLoad() override {} \
    D3DRESOURCETYPE GetType() override { return TYPE; }

bool interface_is(REFIID riid, std::initializer_list<GUID> ids) {
    for (const GUID& id : ids) if (riid == id) return true;
    return false;
}

struct Level {
    UINT width = 0, height = 0, pitch = 0, rows = 0;
    std::vector<BYTE> bytes;
    bool locked = false;
    ~Level() { free_bytes(bytes); }
};
// Direct3D 9 does not lock render targets, depth buffers or non-dynamic
// textures and surfaces in D3DPOOL_DEFAULT.
bool lockable(D3DPOOL pool, DWORD usage) {
    if (usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) return false;
    return pool != D3DPOOL_DEFAULT || (usage & D3DUSAGE_DYNAMIC);
}

class Texture;

// A surface: a level of a texture (container set) or an offscreen plain surface.
class Surface final : public Object<IDirect3DSurface9> {
public:
    Surface(IDirect3DDevice9* device, Texture* container, Level* level, D3DFORMAT format, D3DPOOL pool, DWORD usage);
    ~Surface() override;
    std::unique_ptr<Level> own;
    UINT level_index = 0;
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (interface_is(riid, {__uuidof(IUnknown), __uuidof(IDirect3DResource9), __uuidof(IDirect3DSurface9)})) {
            AddRef(); *ppvObject = static_cast<IDirect3DSurface9*>(this); return S_OK;
        }
        *ppvObject = nullptr; return E_NOINTERFACE;
    }
    FOURSTORY_RESOURCE_MEMBERS(D3DRTYPE_SURFACE)
    HRESULT GetContainer(REFIID riid, void** ppContainer) override;
    HRESULT GetDesc(D3DSURFACE_DESC* pDesc) override {
        if (!pDesc) return D3DERR_INVALIDCALL;
        std::memset(pDesc, 0, sizeof(*pDesc));
        pDesc->Format = format_; pDesc->Type = D3DRTYPE_SURFACE; pDesc->Usage = usage_; pDesc->Pool = pool_;
        pDesc->MultiSampleType = D3DMULTISAMPLE_NONE; pDesc->Width = level_->width; pDesc->Height = level_->height;
        return D3D_OK;
    }
    HRESULT LockRect(D3DLOCKED_RECT* locked_rect, const RECT* rect, DWORD flags) override;
    void detach() { container_ = nullptr; }
    HRESULT UnlockRect() override {
        if (!level_->locked) return D3DERR_INVALIDCALL;
        level_->locked = false; return D3D_OK;
    }
    FOURSTORY_STUBS_IDirect3DSurface9
private:
    Texture* container_;
    Level* level_;
    D3DFORMAT format_;
    D3DPOOL pool_;
    DWORD usage_;
};

HRESULT lock_level(Level& level, D3DFORMAT format, D3DPOOL pool, DWORD usage, D3DLOCKED_RECT* locked_rect, const RECT* rect) {
    if (!locked_rect) return D3DERR_INVALIDCALL;
    locked_rect->Pitch = 0; locked_rect->pBits = nullptr;
    if (level.locked || !lockable(pool, usage)) return D3DERR_INVALIDCALL;
    bool block = false;
    const UINT unit = format_unit_bytes(format, &block);
    UINT offset = 0;
    if (rect) {
        if (rect->left < 0 || rect->top < 0 || rect->right > LONG(level.width) || rect->bottom > LONG(level.height) ||
            rect->left >= rect->right || rect->top >= rect->bottom) return D3DERR_INVALIDCALL;
        if (block) {
            // DXT rectangles cover whole 4x4 blocks; the right and bottom edges
            // may also end at the level edge.
            if ((rect->left | rect->top) & 3) return D3DERR_INVALIDCALL;
            if ((rect->right & 3) && UINT(rect->right) != level.width) return D3DERR_INVALIDCALL;
            if ((rect->bottom & 3) && UINT(rect->bottom) != level.height) return D3DERR_INVALIDCALL;
            offset = UINT(rect->top / 4) * level.pitch + UINT(rect->left / 4) * unit;
        } else offset = UINT(rect->top) * level.pitch + UINT(rect->left) * unit;
    }
    level.locked = true;
    locked_rect->Pitch = INT(level.pitch);
    locked_rect->pBits = level.bytes.data() + offset;
    return D3D_OK;
}

class Texture final : public Object<IDirect3DTexture9> {
public:
    Texture(IDirect3DDevice9* device, D3DFORMAT format, D3DPOOL pool, DWORD usage)
        : Object<IDirect3DTexture9>(device), format_(format), pool_(pool), usage_(usage) {}
    ~Texture() override { for (Surface* surface : surfaces) if (surface) surface->detach(); }
    std::vector<std::unique_ptr<Level>> levels;
    std::vector<Surface*> surfaces; // one per level while it exists, as GetSurfaceLevel returns the same object
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (interface_is(riid, {__uuidof(IUnknown), __uuidof(IDirect3DResource9), __uuidof(IDirect3DBaseTexture9), __uuidof(IDirect3DTexture9)})) {
            AddRef(); *ppvObject = static_cast<IDirect3DTexture9*>(this); return S_OK;
        }
        *ppvObject = nullptr; return E_NOINTERFACE;
    }
    FOURSTORY_RESOURCE_MEMBERS(D3DRTYPE_TEXTURE)
    DWORD SetLOD(DWORD LODNew) override { const DWORD old = lod_; if (pool_ == D3DPOOL_MANAGED) lod_ = LODNew < levels.size() ? LODNew : DWORD(levels.size() - 1); return old; }
    DWORD GetLOD() override { return lod_; }
    DWORD GetLevelCount() override { return DWORD(levels.size()); }
    HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) override {
        if (!pDesc || Level >= levels.size()) return D3DERR_INVALIDCALL;
        std::memset(pDesc, 0, sizeof(*pDesc));
        pDesc->Format = format_; pDesc->Type = D3DRTYPE_SURFACE; pDesc->Usage = usage_; pDesc->Pool = pool_;
        pDesc->MultiSampleType = D3DMULTISAMPLE_NONE; pDesc->Width = levels[Level]->width; pDesc->Height = levels[Level]->height;
        return D3D_OK;
    }
    HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface9** ppSurfaceLevel) override {
        if (!ppSurfaceLevel) return D3DERR_INVALIDCALL;
        *ppSurfaceLevel = nullptr;
        if (Level >= levels.size()) return D3DERR_INVALIDCALL;
        if (surfaces.size() < levels.size()) surfaces.resize(levels.size(), nullptr);
        if (surfaces[Level]) { surfaces[Level]->AddRef(); *ppSurfaceLevel = surfaces[Level]; return D3D_OK; }
        Surface* surface = new Surface(device_, this, levels[Level].get(), format_, pool_, usage_);
        surface->level_index = Level;
        surfaces[Level] = surface;
        ++stats.surfaces;
        *ppSurfaceLevel = surface;
        return D3D_OK;
    }
    void forget_surface(UINT level) { if (level < surfaces.size()) surfaces[level] = nullptr; }
    HRESULT LockRect(UINT level, D3DLOCKED_RECT* locked_rect, const RECT* rect, DWORD flags) override {
        (void)flags;
        if (level >= levels.size()) { if (locked_rect) { locked_rect->Pitch = 0; locked_rect->pBits = nullptr; } return D3DERR_INVALIDCALL; }
        return lock_level(*levels[level], format_, pool_, usage_, locked_rect, rect);
    }
    HRESULT UnlockRect(UINT Level) override {
        if (Level >= levels.size() || !levels[Level]->locked) return D3DERR_INVALIDCALL;
        levels[Level]->locked = false; return D3D_OK;
    }
    HRESULT AddDirtyRect(const RECT* dirty_rect) override { (void)dirty_rect; return D3D_OK; } // no GPU copy to mark
    FOURSTORY_STUBS_IDirect3DTexture9
private:
    D3DFORMAT format_;
    D3DPOOL pool_;
    DWORD usage_;
    DWORD lod_ = 0;
};

Surface::Surface(IDirect3DDevice9* device, Texture* container, Level* level, D3DFORMAT format, D3DPOOL pool, DWORD usage)
    : Object<IDirect3DSurface9>(device), container_(container), level_(level), format_(format), pool_(pool), usage_(usage) {
    if (container_) container_->AddRef();
}
Surface::~Surface() { if (container_) { container_->forget_surface(level_index); container_->Release(); } }
HRESULT Surface::GetContainer(REFIID riid, void** ppContainer) {
    if (!ppContainer) return D3DERR_INVALIDCALL;
    if (container_) return container_->QueryInterface(riid, ppContainer);
    *ppContainer = nullptr; return E_NOINTERFACE;
}
HRESULT Surface::LockRect(D3DLOCKED_RECT* locked_rect, const RECT* rect, DWORD flags) {
    (void)flags;
    return lock_level(*level_, format_, pool_, usage_, locked_rect, rect);
}

template<class Interface, class Desc, D3DRESOURCETYPE Type> class Buffer final : public Object<Interface> {
public:
    Buffer(IDirect3DDevice9* device, DWORD usage, D3DFORMAT format, DWORD fvf, D3DPOOL pool)
        : Object<Interface>(device), usage_(usage), format_(format), fvf_(fvf), pool_(pool) {}
    ~Buffer() override { free_bytes(bytes_); }
    bool allocate_bytes(UINT length) { return allocate(bytes_, length); }
    std::string stub_name(const char* method) const {
        return std::string(Type == D3DRTYPE_VERTEXBUFFER ? "IDirect3DVertexBuffer9::" : "IDirect3DIndexBuffer9::") + method;
    }
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (interface_is(riid, {__uuidof(IUnknown), __uuidof(IDirect3DResource9), __uuidof(Interface)})) {
            this->AddRef(); *ppvObject = static_cast<Interface*>(this); return S_OK;
        }
        *ppvObject = nullptr; return E_NOINTERFACE;
    }
    HRESULT GetDevice(struct IDirect3DDevice9** ppDevice) override {
        if (!ppDevice) return D3DERR_INVALIDCALL; *ppDevice = this->device_; this->device_->AddRef(); return D3D_OK;
    }
    DWORD SetPriority(DWORD PriorityNew) override { const DWORD old = this->priority_; this->priority_ = PriorityNew; return old; }
    DWORD GetPriority() override { return this->priority_; }
    void PreLoad() override {}
    D3DRESOURCETYPE GetType() override { return Type; }
    // Windows allows nested locks of a buffer; the empty device does too.
    HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, DWORD Flags) override {
        (void)Flags;
        if (!ppbData) return D3DERR_INVALIDCALL;
        *ppbData = nullptr;
        if (OffsetToLock > bytes_.size() || (SizeToLock && SizeToLock > bytes_.size() - OffsetToLock)) return D3DERR_INVALIDCALL;
        ++locks_; *ppbData = bytes_.data() + OffsetToLock; return D3D_OK;
    }
    HRESULT Unlock() override { if (!locks_) return D3DERR_INVALIDCALL; --locks_; return D3D_OK; }
    HRESULT GetDesc(Desc* pDesc) override;
    FOURSTORY_STUBS_Buffer
private:
    std::vector<BYTE> bytes_;
    unsigned locks_ = 0;
    DWORD usage_;
    D3DFORMAT format_;
    DWORD fvf_;
    D3DPOOL pool_;
};
}

UINT format_unit_bytes(D3DFORMAT format, bool* block_compressed) {
    bool block = false; UINT unit = 0;
    switch (format) {
    case D3DFMT_DXT1: block = true; unit = 8; break;
    case D3DFMT_DXT2: case D3DFMT_DXT3: case D3DFMT_DXT4: case D3DFMT_DXT5: block = true; unit = 16; break;
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_X8B8G8R8:
    case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10: case D3DFMT_G16R16: case D3DFMT_R32F: case D3DFMT_INDEX32: unit = 4; break;
    case D3DFMT_R8G8B8: unit = 3; break;
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4:
    case D3DFMT_A8R3G3B2: case D3DFMT_A8L8: case D3DFMT_L16: case D3DFMT_R16F: case D3DFMT_INDEX16: unit = 2; break;
    case D3DFMT_A8: case D3DFMT_L8: case D3DFMT_R3G3B2: case D3DFMT_A4L4: case D3DFMT_P8: unit = 1; break;
    case D3DFMT_A16B16G16R16: case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F: unit = 8; break;
    case D3DFMT_A32B32G32R32F: unit = 16; break;
    default: unit = 0; break;
    }
    if (block_compressed) *block_compressed = block;
    return unit;
}

bool level_layout(D3DFORMAT format, UINT width, UINT height, UINT* pitch, UINT* rows) {
    bool block = false;
    const UINT unit = format_unit_bytes(format, &block);
    if (!unit || !width || !height) return false;
    const unsigned long long p = block ? (unsigned long long)((width + 3) / 4) * unit : (unsigned long long)width * unit;
    const UINT r = block ? (height + 3) / 4 : height;
    if (p > 0x7fffffffull) return false;
    *pitch = UINT(p); *rows = r;
    return true;
}

namespace {
template<class Interface, class Desc, D3DRESOURCETYPE Type>
HRESULT Buffer<Interface, Desc, Type>::GetDesc(Desc* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    std::memset(pDesc, 0, sizeof(*pDesc));
    pDesc->Format = format_; pDesc->Type = Type; pDesc->Usage = usage_; pDesc->Pool = pool_; pDesc->Size = UINT(bytes_.size());
    return D3D_OK;
}
template<> HRESULT Buffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC, D3DRTYPE_VERTEXBUFFER>::GetDesc(D3DVERTEXBUFFER_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    std::memset(pDesc, 0, sizeof(*pDesc));
    pDesc->Format = D3DFMT_VERTEXDATA; pDesc->Type = D3DRTYPE_VERTEXBUFFER; pDesc->Usage = usage_; pDesc->Pool = pool_;
    pDesc->Size = UINT(bytes_.size()); pDesc->FVF = fvf_;
    return D3D_OK;
}
typedef Buffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC, D3DRTYPE_VERTEXBUFFER> VertexBuffer;
typedef Buffer<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC, D3DRTYPE_INDEXBUFFER> IndexBuffer;

class Device final : public IDirect3DDevice9 {
public:
    Device() { ++stats.live_objects; }
    ~Device() { --stats.live_objects; }
    HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
        if (!ppvObject) return E_POINTER;
        if (interface_is(riid, {__uuidof(IUnknown), __uuidof(IDirect3DDevice9)})) { AddRef(); *ppvObject = static_cast<IDirect3DDevice9*>(this); return S_OK; }
        *ppvObject = nullptr; return E_NOINTERFACE;
    }
    ULONG AddRef() override { return ++references_; }
    ULONG Release() override { const ULONG left = --references_; if (!left) delete this; return left; }
    HRESULT TestCooperativeLevel() override { return D3D_OK; }
    UINT GetAvailableTextureMem() override { return 0x7ff00000u; }
    HRESULT EvictManagedResources() override { return D3D_OK; } // nothing is on a GPU
    // Capabilities of a device without a size or power-of-two limit for the
    // formats of format_unit_bytes. Fields not set are 0.
    HRESULT GetDeviceCaps(D3DCAPS9* pCaps) override {
        if (!pCaps) return D3DERR_INVALIDCALL;
        std::memset(pCaps, 0, sizeof(*pCaps));
        pCaps->DeviceType = D3DDEVTYPE_NULLREF;
        pCaps->MaxTextureWidth = pCaps->MaxTextureHeight = max_texture_size;
        pCaps->MaxTextureAspectRatio = max_texture_size;
        pCaps->MaxVertexIndex = 0xffffffffu;
        pCaps->MaxPrimitiveCount = 0xffffffffu;
        return D3D_OK;
    }
    HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                          IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) override {
        if (!ppTexture) return failed(D3DERR_INVALIDCALL);
        *ppTexture = nullptr;
        if (pSharedHandle || !Width || !Height || Width > max_texture_size || Height > max_texture_size) return failed(D3DERR_INVALIDCALL);
        UINT full = 1; for (UINT w = Width, h = Height; w > 1 || h > 1; w = w > 1 ? w / 2 : 1, h = h > 1 ? h / 2 : 1) ++full;
        if (Levels > full) return failed(D3DERR_INVALIDCALL);
        // An automatically generated mip chain is one level for the application.
        const UINT count = (Usage & D3DUSAGE_AUTOGENMIPMAP) ? 1 : (Levels ? Levels : full);
        std::unique_ptr<Texture> texture(new Texture(this, Format, Pool, Usage));
        UINT w = Width, h = Height; unsigned long long total = 0;
        for (UINT i = 0; i < count; ++i) {
            std::unique_ptr<Level> level(new Level());
            level->width = w; level->height = h;
            if (!level_layout(Format, w, h, &level->pitch, &level->rows)) return failed(D3DERR_INVALIDCALL);
            const unsigned long long size = (unsigned long long)level->pitch * level->rows;
            if (size > max_buffer_bytes) return failed(D3DERR_OUTOFVIDEOMEMORY);
            if (!allocate(level->bytes, size_t(size))) return failed(D3DERR_OUTOFVIDEOMEMORY);
            total += level->bytes.size();
            texture->levels.push_back(std::move(level));
            w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1;
        }
        ++stats.textures; (void)total;
        *ppTexture = texture.release();
        return D3D_OK;
    }
    HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) override {
        if (!ppVertexBuffer) return failed(D3DERR_INVALIDCALL);
        *ppVertexBuffer = nullptr;
        if (pSharedHandle || !Length) return failed(D3DERR_INVALIDCALL);
        if (Length > max_buffer_bytes) return failed(D3DERR_OUTOFVIDEOMEMORY);
        std::unique_ptr<VertexBuffer> buffer(new VertexBuffer(this, Usage, D3DFMT_VERTEXDATA, FVF, Pool));
        if (!buffer->allocate_bytes(Length)) return failed(D3DERR_OUTOFVIDEOMEMORY);
        *ppVertexBuffer = buffer.release();
        ++stats.vertex_buffers;
        return D3D_OK;
    }
    HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) override {
        if (!ppIndexBuffer) return failed(D3DERR_INVALIDCALL);
        *ppIndexBuffer = nullptr;
        if (pSharedHandle || !Length || (Format != D3DFMT_INDEX16 && Format != D3DFMT_INDEX32)) return failed(D3DERR_INVALIDCALL);
        if (Length > max_buffer_bytes) return failed(D3DERR_OUTOFVIDEOMEMORY);
        std::unique_ptr<IndexBuffer> buffer(new IndexBuffer(this, Usage, Format, 0, Pool));
        if (!buffer->allocate_bytes(Length)) return failed(D3DERR_OUTOFVIDEOMEMORY);
        *ppIndexBuffer = buffer.release();
        ++stats.index_buffers;
        return D3D_OK;
    }
    HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override {
        if (!ppSurface) return failed(D3DERR_INVALIDCALL);
        *ppSurface = nullptr;
        if (pSharedHandle || !Width || !Height || Width > max_texture_size || Height > max_texture_size) return failed(D3DERR_INVALIDCALL);
        std::unique_ptr<Level> level(new Level());
        level->width = Width; level->height = Height;
        if (!level_layout(Format, Width, Height, &level->pitch, &level->rows)) return failed(D3DERR_INVALIDCALL);
        const unsigned long long size = (unsigned long long)level->pitch * level->rows;
        if (size > max_buffer_bytes || !allocate(level->bytes, size_t(size))) return failed(D3DERR_OUTOFVIDEOMEMORY);
        Surface* surface = new Surface(this, nullptr, level.get(), Format, Pool, 0);
        surface->own = std::move(level);
        ++stats.surfaces;
        *ppSurface = surface;
        return D3D_OK;
    }
    FOURSTORY_STUBS_IDirect3DDevice9
private:
    ULONG references_ = 1;
};
}

HRESULT CreateEmptyDevice(IDirect3DDevice9** device) {
    if (!device) return D3DERR_INVALIDCALL;
    *device = new Device();
    return D3D_OK;
}
const Statistics& statistics() { return stats; }
void reset_statistics() { const long long live = stats.live_objects, bytes = stats.live_bytes; stats = Statistics(); stats.live_objects = live; stats.live_bytes = bytes; }
}}
