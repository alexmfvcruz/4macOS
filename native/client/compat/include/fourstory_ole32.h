// Native COM entry points (C4) over the COM base types of <objbase.h>
// (native/client/d3d9/include). fourstory_engine_ui.h includes this header.
#pragma once
#include <objbase.h>
// ---- COM (C4) -----------------------------------------------------------------
// macOS has no COM runtime. Initialization reports E_NOTIMPL (the client does
// not test the result) and no class can be created.
#ifndef REGDB_E_CLASSNOTREG
#define REGDB_E_CLASSNOTREG ((HRESULT)0x80040154L)
#endif
#ifndef CLSCTX_INPROC_SERVER
#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_LOCAL_SERVER 0x4
#endif
#ifndef COINIT_MULTITHREADED
#define COINIT_APARTMENTTHREADED 0x2
#define COINIT_MULTITHREADED 0x0
#endif
inline HRESULT CoInitialize(LPVOID) { return E_NOTIMPL; }
inline HRESULT CoInitializeEx(LPVOID, DWORD) { return E_NOTIMPL; }
inline void CoUninitialize() {}
inline HRESULT CoCreateInstance(const IID&, IUnknown*, DWORD, const IID&, LPVOID* object) { if (object) *object = nullptr; return REGDB_E_CLASSNOTREG; }

