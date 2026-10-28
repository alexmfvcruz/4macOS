// Native build (C4-P): what the interface units get from their precompiled
// headers on Windows. TComp/StdAfx.h and the Engine Lib units TachyonApp.cpp,
// TachyonWnd.cpp and D3DFont.cpp include this file in their native branch.
// The C2 header gives afx.h, afxwin.h, winsock2.h, zlib.h and the native
// subset of T3D.h (loader classes, camera, light). This header adds GDI+
// (T3D.h: "using namespace Gdiplus"), the IME names, the session and the
// interface classes that T3D.h includes on Windows.
#ifdef FOURSTORY_CLIENT_NATIVE
#pragma once
#include <gdiplus.h>
using namespace Gdiplus;
#include <imm.h>
#include <fourstory_ole32.h>
#include <shlwapi.h>
#include <fourstory_engine_resources.h>
// The rest of the T3D.h include list of Windows. Windows-only types come
// from declaration-only compatibility headers (mmsystem.h, dsound.h, dshow.h,
// Wbemidl.h). Implementations that need Windows services are native stand-ins
// in native/client/shell/src (*_native.cpp) or are not built (README).
#include <mmsystem.h>
#include <dsound.h>
#include <dshow.h>
#include <Wbemidl.h>
// T3D.h leaves these sound containers out of the native build.
typedef vector<LPDIRECTSOUND3DBUFFER> VDIRECTSOUND3DBUFFER, *LPVDIRECTSOUND3DBUFFER;
typedef vector<LPDIRECTSOUNDBUFFER> VDIRECTSOUNDBUFFER, *LPVDIRECTSOUNDBUFFER;
#include "TConsole.h"
#include "TProfile.h"
#include "D3DSound.h"
#include "T3DMusic.h"
#include "TShow.h"
#include "TNICEventSink.h"
#include "TModuleProtector.h"
#include "TachyonMedia.h"
#include "TachyonSession.h"
#include "D3DFont.h"
#include "TComp.h"
#include "TachyonApp.h"
#include "TachyonWnd.h"
#include "FrameGroup.h"
#include "TDynamicBillboard.h"
#endif
