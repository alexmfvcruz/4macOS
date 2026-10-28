// MFC header name (C4). The WinINet classes are not provided. URLDownloadToFile
// (urlmon) is declared because TClientCustomCloak.cpp calls it after this
// include: the native function downloads nothing and returns E_NOTIMPL, so the
// custom cloak texture is not fetched (the caller then finds no cache file).
#pragma once
#include "afxwin.h"
struct IBindStatusCallback;
typedef IBindStatusCallback* LPBINDSTATUSCALLBACK;
inline HRESULT URLDownloadToFile(IUnknown*, LPCSTR, LPCSTR, DWORD, LPBINDSTATUSCALLBACK) { return E_NOTIMPL; }
