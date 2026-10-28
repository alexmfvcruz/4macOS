// Windows multimedia names used by the engine audio headers (C4-P): types
// only. The audio implementations (D3DSound.cpp, T3DMusic.cpp, TachyonMedia.cpp)
// are not in the native build; audio is a later step (feasibility report, C7).
#pragma once
#include <windows.h>
FOURSTORY_COMPAT_HANDLE(HMMIO);
typedef DWORD FOURCC;
struct WAVEFORMATEX { WORD wFormatTag, nChannels; DWORD nSamplesPerSec, nAvgBytesPerSec; WORD nBlockAlign, wBitsPerSample, cbSize; };
typedef WAVEFORMATEX *PWAVEFORMATEX, *LPWAVEFORMATEX;
typedef const WAVEFORMATEX *LPCWAVEFORMATEX;
struct MMCKINFO { FOURCC ckid; DWORD cksize; FOURCC fccType; DWORD dwDataOffset, dwFlags; };
typedef MMCKINFO *LPMMCKINFO;
struct WAVEHDR { LPSTR lpData; DWORD dwBufferLength, dwBytesRecorded; DWORD_PTR dwUser; DWORD dwFlags, dwLoops; WAVEHDR* lpNext; DWORD_PTR reserved; };
typedef WAVEHDR *LPWAVEHDR;
#define WAVE_FORMAT_PCM 1
