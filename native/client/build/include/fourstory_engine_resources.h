// Native build (C2): what the resource loaders get from the Engine Lib
// precompiled header StdAfx.h on Windows (afx.h, afxwin.h, afxsock.h, T3D.h,
// zlib.h). T3D.h has a native branch (Codex, H-B22) that leaves out the
// multimedia, GDI+ and network headers and the classes the loaders do not use.
#ifdef FOURSTORY_CLIENT_NATIVE
#pragma once
#include <afx.h>
#include <afxwin.h>
#include <winsock2.h>
#include <zlib.h>
#include "T3D.h"
// Camera and light: used by the map, mesh, effect and animation code.
#include "D3DCamera.h"
#include "D3DLight.h"
// C4 frame capture before Present (native/client/app/src/capture_native.cpp);
// does nothing unless FOURSTORY_C4_CAPTURE_DIR is set.
void fourstory_capture_frame(LPDIRECT3DDEVICE9 device, const char* kind);
// C6 per-category sample counters (native/client/app/src/capture_native.cpp):
// one D3D9 occlusion query per render category; does nothing unless
// FOURSTORY_C6_PASS_LOG is set. fourstory_c6_pass starts a category (ends the
// previous one); fourstory_c6_note records a per-frame count.
enum { FOURSTORY_C6_NONE = -1, FOURSTORY_C6_SKY, FOURSTORY_C6_BSP, FOURSTORY_C6_TERRAIN,
       FOURSTORY_C6_BSPOBJ, FOURSTORY_C6_OBJECT, FOURSTORY_C6_PC, FOURSTORY_C6_MON, FOURSTORY_C6_NPC,
       FOURSTORY_C6_DECAL, FOURSTORY_C6_BILLBOARD, FOURSTORY_C6_WATER, FOURSTORY_C6_LAVA,
       FOURSTORY_C6_SFX, FOURSTORY_C6_UI, FOURSTORY_C6_MINIMAP, FOURSTORY_C6_COUNT };
void fourstory_c6_pass(LPDIRECT3DDEVICE9 device, int category);
void fourstory_c6_note(const char* key, long value);
int fourstory_c6_object_category(int type);
#endif
