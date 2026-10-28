// MFC OLE automation is not provided natively (C4-P). The interface units that
// use it (THttpCtrl, THtmlDocWnd, TOccManager: the embedded Internet Explorer
// control) are not in the native build. The header exists because TComp.h
// includes it.
#pragma once
#include "afxwin.h"
