// Native build (C2): the declarations that TachyonSession.cpp gets from the
// Engine Lib umbrella headers (StdAfx.h, T3D.h) on Windows.
#ifdef FOURSTORY_CLIENT_NATIVE
#pragma once
#include <afx.h>
#include <winsock2.h>
#include <map>
#include "Packet.h"
#include "CryptographyExt.h"
using namespace std; // T3D.h:38
#ifdef FOURSTORY_CLIENT_NATIVE_LOADER
class CTachyonSession;
// The session owner. On Windows CTachyonWnd is the main window (TachyonWnd.h:16,
// a CWnd) and these three virtual members have empty bodies
// (TachyonWnd.cpp:415-425). The C2/C3 programs without a window
// (FOURSTORY_CLIENT_NATIVE_LOADER) need only this part.
class CTachyonWnd {
public:
	virtual ~CTachyonWnd() {}
	virtual void OnSessionMsg( CTachyonSession *pSession, CPacket *pPacket) {}
	virtual void OnConnect( CTachyonSession *pSession, int nErrorCode) {}
	virtual void OnClose( CTachyonSession *pSession, int nErrorCode) {}
};
typedef map< SOCKET, CTachyonSession *> MAPSESSION, *LPMAPSESSION; // T3D.h:666
#else
// The client (C4): the real CTachyonWnd and the full engine header set. The
// stand-in above has a different virtual table and must not be mixed with it.
#include <fourstory_engine_ui.h>
#endif
#endif
