// Native Input Method Manager names (C4-P). The native window system has no
// input context: text from macOS input methods, including dead keys, arrives
// as WM_CHAR (fourstory_user32.h, input_text). No WM_IME_* message is sent.
// ImmGetContext returns NULL; the other calls fail. Composition for East Asian
// languages is NOT SUPPORTED.
#pragma once
#include <windows.h>
FOURSTORY_COMPAT_HANDLE(HIMC);
struct CANDIDATELIST { DWORD dwSize, dwStyle, dwCount, dwSelection, dwPageStart, dwPageSize, dwOffset[1]; };
typedef CANDIDATELIST *LPCANDIDATELIST;
struct COMPOSITIONFORM { DWORD dwStyle; POINT ptCurrentPos; RECT rcArea; };
typedef COMPOSITIONFORM *LPCOMPOSITIONFORM;
struct CANDIDATEFORM { DWORD dwIndex, dwStyle; POINT ptCurrentPos; RECT rcArea; };
typedef CANDIDATEFORM *LPCANDIDATEFORM;
#define IMM_ERROR_NODATA (-1)
#define IMM_ERROR_GENERAL (-2)
inline HIMC ImmGetContext(HWND) { return nullptr; }
inline BOOL ImmReleaseContext(HWND, HIMC) { return FALSE; }
inline HIMC ImmAssociateContext(HWND, HIMC) { return nullptr; }
inline LONG ImmGetCompositionString(HIMC, DWORD, LPVOID, DWORD) { return IMM_ERROR_GENERAL; }
inline LONG ImmGetCompositionStringA(HIMC h, DWORD i, LPVOID b, DWORD n) { return ImmGetCompositionString(h, i, b, n); }
inline BOOL ImmSetCompositionString(HIMC, DWORD, LPVOID, DWORD, LPVOID, DWORD) { return FALSE; }
inline BOOL ImmGetConversionStatus(HIMC, LPDWORD conversion, LPDWORD sentence) { if (conversion) *conversion = 0; if (sentence) *sentence = 0; return FALSE; }
inline BOOL ImmSetConversionStatus(HIMC, DWORD, DWORD) { return FALSE; }
inline BOOL ImmGetOpenStatus(HIMC) { return FALSE; }
inline BOOL ImmSetOpenStatus(HIMC, BOOL) { return FALSE; }
inline BOOL ImmNotifyIME(HIMC, DWORD, DWORD, DWORD) { return FALSE; }
inline DWORD ImmGetCandidateList(HIMC, DWORD, LPCANDIDATELIST, DWORD) { return 0; }
inline DWORD ImmGetCandidateListCount(HIMC, LPDWORD count) { if (count) *count = 0; return 0; }
inline BOOL ImmSetCompositionWindow(HIMC, LPCOMPOSITIONFORM) { return FALSE; }
inline BOOL ImmSetCandidateWindow(HIMC, LPCANDIDATEFORM) { return FALSE; }
inline UINT ImmGetVirtualKey(HWND) { return 0; }
inline BOOL ImmIsIME(HKL) { return FALSE; }
inline UINT ImmGetDescription(HKL, LPSTR text, UINT size) { if (text && size) text[0] = 0; return 0; }
inline HWND ImmGetDefaultIMEWnd(HWND) { return nullptr; }
