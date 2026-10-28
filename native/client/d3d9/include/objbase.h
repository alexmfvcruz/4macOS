// Local C++ COM declaration bridge for the unmodified DXVK Direct3D 9 headers.
// Adapted from DXVK include/native/windows/windows_base.h and unknwn.h;
// see ../licenses/DXVK-LICENSE.txt. Uses the project's existing Win32 types.
#pragma once
#ifndef __cplusplus
#error "The 4Story D3D9 bridge supports C++ interfaces only"
#endif
#include <windows.h>
#include <cstring>
#ifndef GUID_DEFINED
#define GUID_DEFINED
struct GUID { DWORD Data1; WORD Data2, Data3; BYTE Data4[8]; };
#endif
typedef GUID IID;
typedef const IID& REFIID;
typedef const GUID& REFGUID;
typedef BOOL WINBOOL;
typedef HANDLE HMONITOR;
struct LUID { DWORD LowPart; LONG HighPart; };
struct PALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; };
struct RGNDATAHEADER { DWORD dwSize, iType, nCount, nRgnSize; RECT rcBound; };
struct RGNDATA { RGNDATAHEADER rdh; char Buffer[1]; };
#define STDMETHODCALLTYPE
#define CONST const
#define CONST_VTBL const
#define interface struct
#define MIDL_INTERFACE(x) struct
#define STDMETHOD(name) virtual HRESULT name
#define STDMETHOD_(type, name) virtual type name
#define THIS_
#define THIS
#define DECLARE_INTERFACE(x) struct x
#define DECLARE_INTERFACE_(x,y) struct x : public y
#define BEGIN_INTERFACE
#define END_INTERFACE
#define PURE = 0
#define __MSABI_LONG(x) x
#define MAKE_HRESULT(sev,fac,code) ((HRESULT)((DWORD(sev)<<31)|(DWORD(fac)<<16)|DWORD(code)))
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define E_POINTER ((HRESULT)0x80004003)
#define E_ABORT ((HRESULT)0x80004004)
#define DUMMYUNIONNAME
#define DUMMYSTRUCTNAME
#define DUMMYUNIONNAME1
#define DUMMYSTRUCTNAME1
#define __C89_NAMELESS
#define __C89_NAMELESSSTRUCTNAME
#define __C89_NAMELESSUNIONNAME
#define __C89_NAMELESSUNIONNAME1
#define __C89_NAMELESSUNIONNAME2
#define __C89_NAMELESSUNIONNAME3
#define __C89_NAMELESSUNIONNAME4
#define DEFINE_GUID(iid,a,b,c,d,e,f,g,h,i,j,k) constexpr GUID iid={a,b,c,{d,e,f,g,h,i,j,k}};
template<class T> constexpr GUID __uuidof_helper();
#define __uuidof(T) __uuidof_helper<T>()
#define DECLARE_UUIDOF_HELPER(type,a,b,c,d,e,f,g,h,i,j,k) \
  extern "C++" { template<> constexpr GUID __uuidof_helper<type>() { return GUID{a,b,c,{d,e,f,g,h,i,j,k}}; } \
  template<> constexpr GUID __uuidof_helper<type*>() { return __uuidof_helper<type>(); } \
  template<> constexpr GUID __uuidof_helper<const type*>() { return __uuidof_helper<type>(); } \
  template<> constexpr GUID __uuidof_helper<type&>() { return __uuidof_helper<type>(); } \
  template<> constexpr GUID __uuidof_helper<const type&>() { return __uuidof_helper<type>(); } }
#define __CRT_UUID_DECL(type,a,b,c,d,e,f,g,h,i,j,k) DECLARE_UUIDOF_HELPER(type,a,b,c,d,e,f,g,h,i,j,k)
inline bool operator==(const GUID& a,const GUID& b){return std::memcmp(&a,&b,sizeof(GUID))==0;}
inline bool operator!=(const GUID& a,const GUID& b){return !(a==b);}
DEFINE_GUID(IID_IUnknown,0,0,0,0xc0,0,0,0,0,0,0,0x46)
struct IUnknown {
    virtual HRESULT QueryInterface(REFIID,void**)=0;
    template<class Q> HRESULT QueryInterface(Q** p){return QueryInterface(__uuidof(Q),reinterpret_cast<void**>(p));}
    virtual ULONG AddRef()=0;
    virtual ULONG Release()=0;
};
DECLARE_UUIDOF_HELPER(IUnknown,0,0,0,0xc0,0,0,0,0,0,0,0x46)
