// DirectSound interface names used by the engine audio headers (C4-P):
// declarations only, no implementation (see mmsystem.h).
#pragma once
#include <windows.h>
#include "mmsystem.h"
struct IDirectSound; struct IDirectSound8; struct IDirectSoundBuffer; struct IDirectSoundBuffer8;
struct IDirectSound3DBuffer; struct IDirectSound3DListener; typedef IDirectSound3DListener IDirectSound3DListener8;
typedef IDirectSound* LPDIRECTSOUND; typedef IDirectSound8* LPDIRECTSOUND8;
typedef IDirectSoundBuffer* LPDIRECTSOUNDBUFFER; typedef IDirectSoundBuffer8* LPDIRECTSOUNDBUFFER8;
typedef IDirectSound3DBuffer* LPDIRECTSOUND3DBUFFER; typedef IDirectSound3DBuffer* LPDIRECTSOUND3DBUFFER8;
typedef IDirectSound3DListener* LPDIRECTSOUND3DLISTENER; typedef IDirectSound3DListener8* LPDIRECTSOUND3DLISTENER8;
struct DS3DVECTOR_COMPAT { float x, y, z; };
struct DS3DLISTENER { DWORD dwSize; DS3DVECTOR_COMPAT vPosition, vVelocity, vOrientFront, vOrientTop; float flDistanceFactor, flRolloffFactor, flDopplerFactor; };
typedef DS3DLISTENER *LPDS3DLISTENER;
struct DS3DBUFFER { DWORD dwSize; DS3DVECTOR_COMPAT vPosition, vVelocity; DWORD dwInsideConeAngle, dwOutsideConeAngle; DS3DVECTOR_COMPAT vConeOrientation; LONG lConeOutsideVolume; float flMinDistance, flMaxDistance; DWORD dwMode; };
typedef DS3DBUFFER *LPDS3DBUFFER;
#ifndef DSSCL_EXCLUSIVE
#define DSSCL_EXCLUSIVE 0x00000003
#endif
#define DSSCL_NORMAL 0x00000001
#define DSSCL_PRIORITY 0x00000002
#define DS3D_IMMEDIATE 0x00000000
#define DS3D_DEFERRED 0x00000001
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0

// IDirectSound3DBuffer in the method order of the DirectX SDK dsound.h (C4):
// the client calls SetPosition on buffers that CD3DSound::GetDS3D returns.
// The native sound stand-in returns no buffer, so no object implements it.
#if __has_include(<objbase.h>)
#include <objbase.h>
struct IDirectSound3DBuffer : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetAllParameters(LPDS3DBUFFER) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConeAngles(LPDWORD, LPDWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConeOrientation(D3DVECTOR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConeOutsideVolume(LPLONG) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMaxDistance(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMinDistance(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMode(LPDWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPosition(D3DVECTOR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVelocity(D3DVECTOR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetAllParameters(const DS3DBUFFER*, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetConeAngles(DWORD, DWORD, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetConeOrientation(float, float, float, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetConeOutsideVolume(LONG, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMaxDistance(float, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMinDistance(float, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMode(DWORD, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPosition(float, float, float, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVelocity(float, float, float, DWORD) = 0;
};
#endif
