// Native D3DX functions that are not header-only (C4): ID3DXBuffer and
// D3DXAssembleShader over the Windows-assembled bytecode table
// (gen/shaders_generated.cpp; see native/client/d3dx/include/d3dx9tex.h).
#include <d3d9.h>
#include <d3dx9.h>
#include <CommonCrypto/CommonDigest.h>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

namespace fourstory { namespace d3dx {
struct ShaderCode { unsigned char source_sha256[32]; const unsigned char* code; std::size_t size; const char* name; };
extern const ShaderCode shader_codes[];
extern const std::size_t shader_code_count;
}}

namespace {
class Buffer final : public ID3DXBuffer {
public:
    explicit Buffer(std::size_t size) : data_(size) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** out) override { if (out) *out = nullptr; return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = --refs_; if (!n) delete this; return n; }
    void* STDMETHODCALLTYPE GetBufferPointer() override { return data_.empty() ? nullptr : data_.data(); }
    DWORD STDMETHODCALLTYPE GetBufferSize() override { return static_cast<DWORD>(data_.size()); }
private:
    std::vector<unsigned char> data_;
    std::atomic<ULONG> refs_{1};
};
LPD3DXBUFFER make_buffer(const void* data, std::size_t size) {
    Buffer* b = new Buffer(size);
    if (size) std::memcpy(b->GetBufferPointer(), data, size);
    return b;
}
}

HRESULT D3DXCreateBuffer(DWORD size, LPD3DXBUFFER* buffer) {
    if (!buffer) return D3DERR_INVALIDCALL;
    *buffer = new Buffer(size);
    return D3D_OK;
}

// Defines, include handlers and flags change the assembler output on Windows;
// the captured bytecode was made with none of them, so other values fail.
HRESULT D3DXAssembleShader(const char* source, UINT size, const D3DXMACRO* defines, LPD3DXINCLUDE include, DWORD flags,
                           LPD3DXBUFFER* shader, LPD3DXBUFFER* errors) {
    if (shader) *shader = nullptr;
    if (errors) *errors = nullptr;
    auto fail = [&](const std::string& text) {
        if (errors) *errors = make_buffer(text.c_str(), text.size() + 1);
        return D3DXERR_INVALIDDATA;
    };
    if (!source || !shader) return D3DERR_INVALIDCALL;
    if ((defines && defines->Name) || include || flags) return fail("native D3DXAssembleShader: defines, include and flags are not supported\n");
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(source, size, digest);
    for (std::size_t i = 0; i < fourstory::d3dx::shader_code_count; ++i) {
        const fourstory::d3dx::ShaderCode& c = fourstory::d3dx::shader_codes[i];
        if (std::memcmp(c.source_sha256, digest, sizeof(digest)) == 0) { *shader = make_buffer(c.code, c.size); return D3D_OK; }
    }
    return fail("native D3DXAssembleShader: source is not in the captured Windows bytecode table\n");
}
