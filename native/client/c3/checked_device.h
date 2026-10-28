#pragma once
#include <d3d9.h>
#ifdef _WIN32
using WINBOOL=BOOL; // Canonical header spelling, identical SDK ABI.
#endif
#include <cstdint>
#include <functional>
#include <sstream>
#include <stdexcept>
// Observe original engine calls, including HRESULTs that the engine ignores.
// All operations execute on the supplied real device; this is not an empty device.
class CheckedDevice final : public IDirect3DDevice9 {
    IDirect3DDevice9* real_;
    ULONG refs_=1;
    HRESULT first_=S_OK;
    const char* operation_=nullptr;
public:
    std::uint64_t failures=0,draw_calls=0;
    std::function<void(IDirect3DDevice9*)> before_draw;
    explicit CheckedDevice(IDirect3DDevice9* real):real_(real){real_->AddRef();}
    ~CheckedDevice(){real_->Release();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IDirect3DDevice9)){*out=static_cast<IDirect3DDevice9*>(this);AddRef();return S_OK;}
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override {ULONG n=--refs_;if(!n)delete this;return n;}
    void observe(HRESULT hr,const char* operation){if(FAILED(hr)){if(!failures){first_=hr;operation_=operation;}++failures;}}
    void ensure() const {if(failures){std::ostringstream s;s<<"original engine device call "<<operation_<<" failed: 0x"<<std::hex<<unsigned(first_);throw std::runtime_error(s.str());}}
#include "checked_device.inc"
};
