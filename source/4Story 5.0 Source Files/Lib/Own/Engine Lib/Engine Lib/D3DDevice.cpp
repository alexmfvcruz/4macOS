// D3DDevice.cpp: implementation of the CD3DDevice class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "D3DSphere.h"
#ifdef FOURSTORY_CLIENT_NATIVE
#include <fourstory_sdl_backend.h>
#endif
#ifndef FOURSTORY_CLIENT_NATIVE
#include <dxdiag.h>
#endif

DWORD CD3DDevice::m_dwPolyCount = 0;
#ifdef FOURSTORY_CLIENT_MOD
BYTE CD3DDevice::m_bAnisotropic = TRUE;

// Sampler states are lost on Reset, so this runs after device creation and
// after every Reset. Without driver support the option falls back to linear.
void CD3DDevice::ApplyFilterQuality()
{
	if(!m_pDevice)
		return;

	if(!(m_vCAPS.TextureFilterCaps & D3DPTFILTERCAPS_MINFANISOTROPIC) || m_vCAPS.MaxAnisotropy < 2)
	{
		m_bAnisotropic = FALSE;
		return;
	}

	DWORD dwLevel = min( (DWORD) 16, m_vCAPS.MaxAnisotropy);
	for( DWORD i=0; i<8; i++)
		m_pDevice->SetSamplerState( i, D3DSAMP_MAXANISOTROPY, dwLevel);
}

FLOAT CD3DDevice::m_fUIScale = 1.0f;
LPDIRECT3DTEXTURE9 CD3DDevice::m_pUITEX = NULL;
LPDIRECT3DSURFACE9 CD3DDevice::m_pUIBUF = NULL;
LPDIRECT3DSURFACE9 CD3DDevice::m_pUISAVED = NULL;
LPDIRECT3DSURFACE9 CD3DDevice::m_pUIREAL = NULL;
BYTE CD3DDevice::m_bInUIScene = FALSE;

void CD3DDevice::UpdateUIScreenSize()
{
	FLOAT fScale = max( 1.0f, m_fUIScale);

	m_option.m_dwUIScreenX = max( (DWORD) 1, (DWORD)( FLOAT(m_option.m_dwScreenX) / fScale + 0.5f));
	m_option.m_dwUIScreenY = max( (DWORD) 1, (DWORD)( FLOAT(m_option.m_dwScreenY) / fScale + 0.5f));
	CD3DImage::m_dwUIScreenX = m_option.m_dwUIScreenX;
	CD3DImage::m_dwUIScreenY = m_option.m_dwUIScreenY;

	if(!m_bInUIScene)
	{
		CD3DImage::m_dwScreenX = m_option.m_dwScreenX;
		CD3DImage::m_dwScreenY = m_option.m_dwScreenY;

		if(m_pUITEX)
		{
			D3DSURFACE_DESC vDESC;

			if( FAILED(m_pUITEX->GetLevelDesc( 0, &vDESC)) ||
				vDESC.Width != m_option.m_dwUIScreenX ||
				vDESC.Height != m_option.m_dwUIScreenY )
				ReleaseUIScene();
		}
	}
}

void CD3DDevice::ReleaseUIScene()
{
	if(m_bInUIScene)
	{
		// A device reset in the middle of a frame: leave the scene first.
		m_bInUIScene = FALSE;
		m_pRTARGET = m_pUIREAL;
		m_pUIREAL = NULL;
		CD3DImage::m_dwScreenX = m_option.m_dwScreenX;
		CD3DImage::m_dwScreenY = m_option.m_dwScreenY;

		if(m_pDevice)
			m_pDevice->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	}

	if(m_pUISAVED)
	{
		if(m_pDevice)
			m_pDevice->SetRenderTarget( 0, m_pUISAVED);
		m_pUISAVED->Release();
		m_pUISAVED = NULL;
	}

	if(m_pUIBUF)
	{
		m_pUIBUF->Release();
		m_pUIBUF = NULL;
	}

	if(m_pUITEX)
	{
		m_pUITEX->Release();
		m_pUITEX = NULL;
	}
}

void CD3DDevice::BeginUIScene()
{
	if( m_bInUIScene || !m_pDevice || m_fUIScale <= 1.0f )
		return;

	if(!m_pUITEX)
	{
		if( FAILED(D3DXCreateTexture(
			m_pDevice,
			m_option.m_dwUIScreenX,
			m_option.m_dwUIScreenY, 1,
			D3DUSAGE_RENDERTARGET,
			D3DFMT_A8R8G8B8,
			D3DPOOL_DEFAULT,
			&m_pUITEX)) || !m_pUITEX )
		{
			m_pUITEX = NULL;
			return;
		}

		if( FAILED(m_pUITEX->GetSurfaceLevel( 0, &m_pUIBUF)) || !m_pUIBUF )
		{
			ReleaseUIScene();
			return;
		}
	}

	if( FAILED(m_pDevice->GetRenderTarget( 0, &m_pUISAVED)) || !m_pUISAVED )
	{
		m_pUISAVED = NULL;
		return;
	}

	if(FAILED(m_pDevice->SetRenderTarget( 0, m_pUIBUF)))
	{
		m_pUISAVED->Release();
		m_pUISAVED = NULL;
		return;
	}

	// Transparent black; colour is accumulated premultiplied and alpha as coverage.
	m_pDevice->Clear(
		0, NULL,
		D3DCLEAR_TARGET,
		D3DCOLOR_ARGB( 0, 0, 0, 0),
		1.0f, 0);
	m_pDevice->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
	m_pDevice->SetRenderState( D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
	m_pDevice->SetRenderState( D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);

	// Interface code that returns to "the back buffer" must land on the UI texture.
	m_pUIREAL = m_pRTARGET;
	m_pRTARGET = m_pUIBUF;
	CD3DImage::m_dwScreenX = m_option.m_dwUIScreenX;
	CD3DImage::m_dwScreenY = m_option.m_dwUIScreenY;
	m_bInUIScene = TRUE;
}

void CD3DDevice::EndUIScene()
{
	static const D3DRENDERSTATETYPE vRS[] = {
		D3DRS_ZENABLE,
		D3DRS_ALPHATESTENABLE,
		D3DRS_ALPHABLENDENABLE,
		D3DRS_SRCBLEND,
		D3DRS_DESTBLEND,
		D3DRS_FOGENABLE,
		D3DRS_CULLMODE,
		D3DRS_FILLMODE,
		D3DRS_STENCILENABLE};
	static const DWORD vRSVALUE[] = {
		FALSE,
		FALSE,
		TRUE,
		D3DBLEND_ONE,
		D3DBLEND_INVSRCALPHA,
		FALSE,
		D3DCULL_NONE,
		D3DFILL_SOLID,
		FALSE};
	static const D3DSAMPLERSTATETYPE vSS[] = {
		D3DSAMP_MAGFILTER,
		D3DSAMP_MINFILTER,
		D3DSAMP_MIPFILTER,
		D3DSAMP_ADDRESSU,
		D3DSAMP_ADDRESSV};
	static const DWORD vSSVALUE[] = {
		D3DTEXF_LINEAR,
		D3DTEXF_LINEAR,
		D3DTEXF_NONE,
		D3DTADDRESS_CLAMP,
		D3DTADDRESS_CLAMP};
	static const D3DTEXTURESTAGESTATETYPE vTSS[] = {
		D3DTSS_COLOROP,
		D3DTSS_COLORARG1,
		D3DTSS_ALPHAOP,
		D3DTSS_ALPHAARG1,
		D3DTSS_TEXTURETRANSFORMFLAGS};
	static const DWORD vTSSVALUE[] = {
		D3DTOP_SELECTARG1,
		D3DTA_TEXTURE,
		D3DTOP_SELECTARG1,
		D3DTA_TEXTURE,
		D3DTTFF_DISABLE};
	const int nRS = sizeof(vRS) / sizeof(vRS[0]);
	const int nSS = sizeof(vSS) / sizeof(vSS[0]);
	const int nTSS = sizeof(vTSS) / sizeof(vTSS[0]);
	DWORD vOLDRS[sizeof(vRS) / sizeof(vRS[0])];
	DWORD vOLDSS[sizeof(vSS) / sizeof(vSS[0])];
	DWORD vOLDTSS[sizeof(vTSS) / sizeof(vTSS[0])];
	DWORD dwOLDSTAGE1 = D3DTOP_DISABLE;

	if(!m_bInUIScene)
		return;

	m_bInUIScene = FALSE;
	m_pRTARGET = m_pUIREAL;
	m_pUIREAL = NULL;
	CD3DImage::m_dwScreenX = m_option.m_dwScreenX;
	CD3DImage::m_dwScreenY = m_option.m_dwScreenY;
	m_pDevice->SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
	m_pDevice->SetRenderTarget( 0, m_pUISAVED);
	m_pUISAVED->Release();
	m_pUISAVED = NULL;

	// Pre-transformed quad over the whole back buffer; -0.5 aligns texels to pixels.
	struct
	{
		FLOAT m_fPosX;
		FLOAT m_fPosY;
		FLOAT m_fPosZ;
		FLOAT m_fRHW;
		FLOAT m_fU;
		FLOAT m_fV;
	} vQUAD[4];
	FLOAT fRight = FLOAT(m_vPRESENT.BackBufferWidth) - 0.5f;
	FLOAT fBottom = FLOAT(m_vPRESENT.BackBufferHeight) - 0.5f;

	for( int i=0; i<4; i++)
	{
		vQUAD[i].m_fPosX = (i & 1) ? fRight : -0.5f;
		vQUAD[i].m_fPosY = (i & 2) ? fBottom : -0.5f;
		vQUAD[i].m_fPosZ = 0.0f;
		vQUAD[i].m_fRHW = 1.0f;
		vQUAD[i].m_fU = (i & 1) ? 1.0f : 0.0f;
		vQUAD[i].m_fV = (i & 2) ? 1.0f : 0.0f;
	}

	for(auto i=0; i<nRS; i++)
	{
		m_pDevice->GetRenderState( vRS[i], &vOLDRS[i]);
		m_pDevice->SetRenderState( vRS[i], vRSVALUE[i]);
	}

	for(auto i=0; i<nSS; i++)
	{
		m_pDevice->GetSamplerState( 0, vSS[i], &vOLDSS[i]);
		m_pDevice->SetSamplerState( 0, vSS[i], vSSVALUE[i]);
	}

	for(auto i=0; i<nTSS; i++)
	{
		m_pDevice->GetTextureStageState( 0, vTSS[i], &vOLDTSS[i]);
		m_pDevice->SetTextureStageState( 0, vTSS[i], vTSSVALUE[i]);
	}

	m_pDevice->GetTextureStageState( 1, D3DTSS_COLOROP, &dwOLDSTAGE1);
	m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	m_pDevice->SetVertexShader(NULL);
	m_pDevice->SetPixelShader(NULL);
	m_pDevice->SetFVF(T3DFVF_TVERTEX);
	m_pDevice->SetTexture( 0, m_pUITEX);
	m_pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, vQUAD, sizeof(vQUAD[0]));
	m_pDevice->SetTexture( 0, NULL);
	m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP, dwOLDSTAGE1);

	for(auto i=0; i<nTSS; i++)
		m_pDevice->SetTextureStageState( 0, vTSS[i], vOLDTSS[i]);

	for(auto i=0; i<nSS; i++)
		m_pDevice->SetSamplerState( 0, vSS[i], vOLDSS[i]);

	for(auto i=0; i<nRS; i++)
		m_pDevice->SetRenderState( vRS[i], vOLDRS[i]);
}

DWORD CD3DDevice::m_dwLostCount = 0;
FLOAT CD3DDevice::m_fExtBias = 0.000008f;
DWORD CD3DDevice::m_dwGoodScreenX = 0;
DWORD CD3DDevice::m_dwGoodScreenY = 0;

void CD3DDevice::LogGraphicsState( LPCSTR szReason, HRESULT hr)
{
	static BYTE bFirst = TRUE;
	FILE *pFILE = NULL;

	if( fopen_s( &pFILE, "TClient-graphics.log", "a") || !pFILE )
		return;

	SYSTEMTIME st;
	GetLocalTime(&st);

	if(bFirst)
		fprintf( pFILE, "\n==== client start %04d-%02d-%02d ====\n", st.wYear, st.wMonth, st.wDay);

	fprintf( pFILE, "[%02d:%02d:%02d] %s hr=0x%08lX lost-checks-since-last=%lu\n",
		st.wHour, st.wMinute, st.wSecond, szReason, (DWORD) hr, m_dwLostCount);
	m_dwLostCount = 0;

	fprintf( pFILE, "  option: windowed=%d screen=%lux%lu behavior=0x%lX interval=0x%lX\n",
		(int) m_option.m_bWindowedMode, m_option.m_dwScreenX, m_option.m_dwScreenY,
		m_option.m_dwBehavior, m_option.m_dwPresentationInterval);
	fprintf( pFILE, "  present: windowed=%d back=%ux%u format=%d count=%u swap=%d multisample=%d depth=%d(auto=%d) refresh=%u flags=0x%lX\n",
		(int) m_vPRESENT.Windowed, m_vPRESENT.BackBufferWidth, m_vPRESENT.BackBufferHeight,
		(int) m_vPRESENT.BackBufferFormat, m_vPRESENT.BackBufferCount, (int) m_vPRESENT.SwapEffect,
		(int) m_vPRESENT.MultiSampleType, (int) m_vPRESENT.AutoDepthStencilFormat,
		(int) m_vPRESENT.EnableAutoDepthStencil, m_vPRESENT.FullScreen_RefreshRateInHz, m_vPRESENT.Flags);

	if(m_pD3D)
	{
		D3DDISPLAYMODE vMODE;
		D3DADAPTER_IDENTIFIER9 vID;

		if(SUCCEEDED(m_pD3D->GetAdapterDisplayMode( D3DADAPTER_DEFAULT, &vMODE)))
			fprintf( pFILE, "  adapter mode: %ux%u@%u format=%d\n", vMODE.Width, vMODE.Height, vMODE.RefreshRate, (int) vMODE.Format);

		if( bFirst && SUCCEEDED(m_pD3D->GetAdapterIdentifier( D3DADAPTER_DEFAULT, 0, &vID)) )
			fprintf( pFILE, "  adapter: %s driver=%s version=%u.%u.%u.%u\n", vID.Description, vID.Driver,
				HIWORD(vID.DriverVersion.HighPart), LOWORD(vID.DriverVersion.HighPart),
				HIWORD(vID.DriverVersion.LowPart), LOWORD(vID.DriverVersion.LowPart));

		if(bFirst)
		{
			UINT nCount = m_pD3D->GetAdapterModeCount( D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
			UINT nLastX = 0;
			UINT nLastY = 0;

			fprintf( pFILE, "  fullscreen modes (X8R8G8B8, %u entries):", nCount);
			for( UINT i=0; i<nCount; i++)
				if( SUCCEEDED(m_pD3D->EnumAdapterModes( D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8, i, &vMODE)) &&
					(vMODE.Width != nLastX || vMODE.Height != nLastY) )
				{
					fprintf( pFILE, " %ux%u", vMODE.Width, vMODE.Height);
					nLastX = vMODE.Width;
					nLastY = vMODE.Height;
				}
			fprintf( pFILE, "\n");
		}
	}

	if(m_pDevice)
	{
		LPDIRECT3DSURFACE9 pSURFACE = NULL;
		D3DSURFACE_DESC vDESC;
		D3DVIEWPORT9 vVIEW;

		if(SUCCEEDED(m_pDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pSURFACE)) && pSURFACE)
		{
			if(SUCCEEDED(pSURFACE->GetDesc(&vDESC)))
				fprintf( pFILE, "  back buffer surface: %ux%u format=%d multisample=%d\n", vDESC.Width, vDESC.Height, (int) vDESC.Format, (int) vDESC.MultiSampleType);
			pSURFACE->Release();
			pSURFACE = NULL;
		}

		if(SUCCEEDED(m_pDevice->GetDepthStencilSurface(&pSURFACE)) && pSURFACE)
		{
			if(SUCCEEDED(pSURFACE->GetDesc(&vDESC)))
				fprintf( pFILE, "  depth surface: %ux%u format=%d multisample=%d\n", vDESC.Width, vDESC.Height, (int) vDESC.Format, (int) vDESC.MultiSampleType);
			pSURFACE->Release();
		}

		if(SUCCEEDED(m_pDevice->GetViewport(&vVIEW)))
			fprintf( pFILE, "  viewport: %lu,%lu %lux%lu z=%g..%g\n", vVIEW.X, vVIEW.Y, vVIEW.Width, vVIEW.Height, vVIEW.MinZ, vVIEW.MaxZ);

		fprintf( pFILE, "  caps: dev=0x%lX caps2=0x%lX caps3=0x%lX texture=0x%lX filter=0x%lX raster=0x%lX zcmp=0x%lX maxtex=%lux%lu aniso=%lu vs=0x%lX ps=0x%lX clipplanes=%lu\n",
			m_vCAPS.DevCaps, m_vCAPS.Caps2, m_vCAPS.Caps3, m_vCAPS.TextureCaps, m_vCAPS.TextureFilterCaps,
			m_vCAPS.RasterCaps, m_vCAPS.ZCmpCaps, m_vCAPS.MaxTextureWidth, m_vCAPS.MaxTextureHeight,
			m_vCAPS.MaxAnisotropy, m_vCAPS.VertexShaderVersion, m_vCAPS.PixelShaderVersion, m_vCAPS.MaxUserClipPlanes);
		fprintf( pFILE, "  shader path=%d (option=%d) anisotropic=%d depth step=%g videomem=%I64u sysmem=%I64u\n",
			(int) m_bEnableSHADER, (int) m_option.m_bUseSHADER, (int) m_bAnisotropic, m_fExtBias, m_lVIDEOMEM, m_lSYSMEM);
	}

	bFirst = FALSE;
	fclose(pFILE);
}
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CD3DDevice::CD3DDevice()
{
	ZeroMemory( m_vConstantVS, VC_COUNT * sizeof(WORD));
	ZeroMemory( m_vConstantPS, PC_COUNT * sizeof(WORD));
	ZeroMemory( &m_vPRESENT, sizeof(m_vPRESENT));

	for( int i=0; i<VS_COUNT; i++)
	{
		m_pVertexShader[i] = NULL;
		m_pDECL[i] = NULL;

		m_dwVertexShader[i] = 0;
	}

	for(auto i=0; i<PS_COUNT; i++)
	{
		m_pPixelShader[i] = NULL;
		m_dwPixelShader[i] = 0;
	}
	m_strResourceType.Empty();

	m_pGRAYTEX = NULL;
	m_pGLOWTEX = NULL;
	m_pBACKTEX = NULL;
	m_pBACKTEX2 = NULL;
	m_pGRAYBUF = NULL;
	m_pGLOWBUF = NULL;
	m_pBACKBUF = NULL;
	m_pBACKBUF2 = NULL;
	m_pRTARGET = NULL;
	m_pBACKVB = NULL;
	m_pZBUF = NULL;

	InitializeCriticalSection(&m_cs);
	m_bEnableSHADER = TRUE;
	m_bUseTHREAD = FALSE;
	m_bRESET = FALSE;

	m_pEraserVB = NULL;
	m_pEraserIB = NULL;

	m_pMainWnd = NULL;
	m_pDevice = NULL;
	m_pD3D = NULL;

	m_lVIDEOMEM = 0;
	m_lSYSMEM = 0;

	ZeroMemory( &m_vCAPS, sizeof(D3DCAPS9));
}

CD3DDevice::~CD3DDevice()
{
	DeleteCriticalSection(&m_cs);
	ReleaseDevice();
}

void CD3DDevice::SetResetFlag( BYTE bRESET)
{
	SMART_LOCKCS(&m_cs);
	m_bRESET = bRESET;
}

BYTE CD3DDevice::GetResetFlag()
{
	SMART_LOCKCS(&m_cs);
	return m_bRESET;
}

D3DFORMAT CD3DDevice::GetBestZFormat( UINT nAdapter,
									  D3DDEVTYPE nType,
									  D3DFORMAT nFormat)
{
	if( SUCCEEDED(m_pD3D->CheckDeviceFormat(
		nAdapter, nType, nFormat,
		D3DUSAGE_DEPTHSTENCIL,
		D3DRTYPE_SURFACE,
		D3DFMT_D32)) &&
		SUCCEEDED(m_pD3D->CheckDepthStencilMatch(
		nAdapter, nType, nFormat,
		nFormat, D3DFMT_D32)))
		return D3DFMT_D32;

	if( SUCCEEDED(m_pD3D->CheckDeviceFormat(
		nAdapter, nType, nFormat,
		D3DUSAGE_DEPTHSTENCIL,
		D3DRTYPE_SURFACE,
		D3DFMT_D24X4S4)) &&
		SUCCEEDED(m_pD3D->CheckDepthStencilMatch(
		nAdapter, nType, nFormat,
		nFormat, D3DFMT_D24X4S4)))
		return D3DFMT_D24X4S4;

	if( SUCCEEDED(m_pD3D->CheckDeviceFormat(
		nAdapter, nType, nFormat,
		D3DUSAGE_DEPTHSTENCIL,
		D3DRTYPE_SURFACE,
		D3DFMT_D24S8)) &&
		SUCCEEDED(m_pD3D->CheckDepthStencilMatch(
		nAdapter, nType, nFormat,
		nFormat, D3DFMT_D24S8)))
		return D3DFMT_D24S8;

	if( SUCCEEDED(m_pD3D->CheckDeviceFormat(
		nAdapter, nType, nFormat,
		D3DUSAGE_DEPTHSTENCIL,
		D3DRTYPE_SURFACE,
		D3DFMT_D24X8)) &&
		SUCCEEDED(m_pD3D->CheckDepthStencilMatch(
		nAdapter, nType, nFormat,
		nFormat, D3DFMT_D24X8)))
		return D3DFMT_D24X8;

	return D3DFMT_D16;
}

void CD3DDevice::InitBACK( D3DDISPLAYMODE *pMODE)
{
	static PVERTEX vMESH[] = {
		{ -0.5f, 0.0f,  0.5f, 0.0f, 0.0f},
		{  0.5f, 0.0f,  0.5f, 1.0f, 0.0f},
		{ -0.5f, 0.0f, -0.5f, 0.0f, 1.0f},
		{  0.5f, 0.0f, -0.5f, 1.0f, 1.0f}};
	LPVOID pDATA = NULL;

	m_pDevice->CreateVertexBuffer(
		4 * sizeof(PVERTEX),
		0, T3DFVF_PVERTEX,
		D3DPOOL_MANAGED,
		&m_pBACKVB, NULL);

	D3DXCreateTexture(
		m_pDevice,
		pMODE->Width,
		pMODE->Height, 1,
		D3DUSAGE_RENDERTARGET,
		pMODE->Format,
		D3DPOOL_DEFAULT,
		&m_pBACKTEX);

	m_pBACKTEX->GetSurfaceLevel( 0, &m_pBACKBUF);
	m_pDevice->GetDepthStencilSurface(&m_pZBUF);
	m_pDevice->GetRenderTarget( 0, &m_pRTARGET);

	m_pBACKVB->Lock( 0, 0, &pDATA, 0);
	memcpy( pDATA, vMESH, 4 * sizeof(PVERTEX));
	m_pBACKVB->Unlock();

	if(!m_bEnableSHADER)
		return;

	m_pDevice->CreateVertexBuffer(
		SPHERE_VERTEXCOUNT * sizeof(D3DXVECTOR3),
		0, D3DFVF_XYZ,
		D3DPOOL_MANAGED,
		&m_pEraserVB, NULL);

	m_pDevice->CreateIndexBuffer(
		SPHERE_INDEXCOUNT * 2,
		0, D3DFMT_INDEX16,
		D3DPOOL_MANAGED,
		&m_pEraserIB, NULL);

	D3DXCreateTexture(
		m_pDevice,
		pMODE->Width,
		pMODE->Height, 0,
		D3DUSAGE_RENDERTARGET|
		D3DUSAGE_AUTOGENMIPMAP,
		pMODE->Format,
		D3DPOOL_DEFAULT,
		&m_pGLOWTEX);

	D3DXCreateTexture(
		m_pDevice,
		pMODE->Width,
		pMODE->Height, 1,
		D3DUSAGE_RENDERTARGET,
		pMODE->Format,
		D3DPOOL_DEFAULT,
		&m_pGRAYTEX);

	m_pGRAYTEX->GetSurfaceLevel( 0, &m_pGRAYBUF);
	m_pGLOWTEX->GetSurfaceLevel( 0, &m_pGLOWBUF);

	m_pEraserIB->Lock( 0, SPHERE_INDEXCOUNT * 2, &pDATA, 0);
	memcpy( pDATA, g_arrayEraserIndex, SPHERE_INDEXCOUNT * 2);
	m_pEraserIB->Unlock();

	m_pEraserVB->Lock( 0, SPHERE_VERTEXCOUNT * sizeof(D3DXVECTOR3), &pDATA, 0);
	memcpy( pDATA, g_arrayEraserVertex, SPHERE_VERTEXCOUNT * sizeof(D3DXVECTOR3));
	m_pEraserVB->Unlock();
}

void CD3DDevice::ReleaseBACK()
{
#ifdef FOURSTORY_CLIENT_MOD
	ReleaseUIScene();
#endif
	if(m_pEraserVB)
	{
		m_pEraserVB->Release();
		m_pEraserVB = NULL;
	}

	if(m_pEraserIB)
	{
		m_pEraserIB->Release();
		m_pEraserIB = NULL;
	}

	if(m_pGRAYBUF)
	{
		m_pGRAYBUF->Release();
		m_pGRAYBUF = NULL;
	}

	if(m_pGLOWBUF)
	{
		m_pGLOWBUF->Release();
		m_pGLOWBUF = NULL;
	}

	if(m_pBACKBUF)
	{
		m_pBACKBUF->Release();
		m_pBACKBUF = NULL;
	}

	if(m_pBACKBUF2)
	{
		m_pBACKBUF2->Release();
		m_pBACKBUF2 = NULL;
	}

	if(m_pRTARGET)
	{
		m_pRTARGET->Release();
		m_pRTARGET = NULL;
	}

	if(m_pZBUF)
	{
		m_pZBUF->Release();
		m_pZBUF = NULL;
	}

	if(m_pGRAYTEX)
	{
		m_pGRAYTEX->Release();
		m_pGRAYTEX = NULL;
	}

	if(m_pGLOWTEX)
	{
		m_pGLOWTEX->Release();
		m_pGLOWTEX = NULL;
	}

	if(m_pBACKTEX)
	{
		m_pBACKTEX->Release();
		m_pBACKTEX = NULL;
	}

	if(m_pBACKTEX2)
	{
		m_pBACKTEX2->Release();
		m_pBACKTEX2 = NULL;
	}

	if(m_pBACKVB)
	{
		m_pBACKVB->Release();
		m_pBACKVB = NULL;
	}
}

BOOL CD3DDevice::InitDevices( CWnd *pWnd)
{
	if( !pWnd || !pWnd->GetSafeHwnd() )
		return FALSE;

	m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if(!m_pD3D)
		return FALSE;

	D3DDISPLAYMODE d3ddm;
	HRESULT hr = m_pD3D->GetAdapterDisplayMode(
		D3DADAPTER_DEFAULT,
		&d3ddm);

	if(FAILED(hr))
	{
		m_pD3D->Release();
		m_pD3D = NULL;
		return FALSE;
	}

	if(!m_option.m_bWindowedMode)
		d3ddm.Format = D3DFMT_X8R8G8B8;

	UINT nAdapterModeCount = m_pD3D->GetAdapterModeCount(
		D3DADAPTER_DEFAULT,
		d3ddm.Format );

	UINT uLastDeltaX = 0xFFFFFFFF;
	UINT uLastDeltaY = 0xFFFFFFFF;
	D3DDISPLAYMODE LastMode = d3ddm;

	for( UINT i=0 ; i < nAdapterModeCount ; ++i )
	{
		D3DDISPLAYMODE mode;

		m_pD3D->EnumAdapterModes(
			D3DADAPTER_DEFAULT,
			d3ddm.Format,
			i,
			&mode );

		if( d3ddm.Format == mode.Format && d3ddm.RefreshRate == mode.RefreshRate )
		{
			UINT uDeltaX = abs( (int)((mode.Width) - (m_option.m_dwScreenX)) );
			UINT uDeltaY = abs( (int)((mode.Height) - (m_option.m_dwScreenY)) );

			if( uDeltaX <= uLastDeltaX && uDeltaY <= uLastDeltaY )
			{
				LastMode = mode;

				uLastDeltaX = uDeltaX;
				uLastDeltaY = uDeltaY;
			}
		}
	}

	m_option.m_dwScreenX = LastMode.Width;
	m_option.m_dwScreenY = LastMode.Height;

	CD3DImage::m_dwScreenX = d3ddm.Width = m_option.m_dwScreenX;
	CD3DImage::m_dwScreenY = d3ddm.Height = m_option.m_dwScreenY;
	ZeroMemory( &m_vPRESENT, sizeof(m_vPRESENT));

	m_vPRESENT.PresentationInterval = m_option.m_dwPresentationInterval;
	m_vPRESENT.SwapEffect = D3DSWAPEFFECT_COPY;

#ifdef FOURSTORY_CLIENT_NATIVE
	// Native Spaces owns fullscreen; avoid exclusive display-mode changes.
	m_vPRESENT.Windowed = TRUE;
#else
	m_vPRESENT.Windowed = m_option.m_bWindowedMode;
#endif
	m_vPRESENT.BackBufferWidth = d3ddm.Width;
	m_vPRESENT.BackBufferHeight = d3ddm.Height;
	m_vPRESENT.BackBufferFormat = d3ddm.Format;

	m_vPRESENT.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

	if(!m_vPRESENT.Windowed)
		m_vPRESENT.FullScreen_RefreshRateInHz = d3ddm.RefreshRate;

	m_vPRESENT.AutoDepthStencilFormat = GetBestZFormat(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		d3ddm.Format);

	m_vPRESENT.EnableAutoDepthStencil = TRUE;
	m_bEnableSHADER = TRUE;

	D3DDEVTYPE devType = D3DDEVTYPE_HAL;
	UINT iAdapter = D3DADAPTER_DEFAULT;

#ifdef USE_NVPERFHUD
	for( UINT iAdt=0; iAdt<m_pD3D->GetAdapterCount(); ++iAdt)
	{
		D3DADAPTER_IDENTIFIER9 adapterIdentifier;
		m_pD3D->GetAdapterIdentifier( iAdt, 0, &adapterIdentifier );
		if( strstr(adapterIdentifier.Description,"PerfHUD") != 0 )
		{
			iAdapter = iAdt;
			devType = D3DDEVTYPE_REF;

			break;
		}
	}
#endif

	hr = m_pD3D->GetDeviceCaps(
		iAdapter,
		devType,
		&m_vCAPS);

	if(FAILED(hr))
	{
		m_pD3D->Release();
		m_pD3D = NULL;

		return FALSE;
	}

	if (m_vCAPS.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD && m_vCAPS.DevCaps & D3DDEVCAPS_EXECUTEVIDEOMEMORY)
		m_vPRESENT.SwapEffect = D3DSWAPEFFECT_DISCARD;

	DWORD dwBehavior =  m_option.m_dwBehavior;

	dwBehavior &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
	dwBehavior &= ~D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	dwBehavior &= ~D3DCREATE_MIXED_VERTEXPROCESSING;

	if( m_vCAPS.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT )
		dwBehavior |= D3DCREATE_MIXED_VERTEXPROCESSING;
	else
		dwBehavior |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

#if defined(_DEBUG) || defined(TEST_MODE)
	dwBehavior = D3DCREATE_HARDWARE_VERTEXPROCESSING;
#endif

	hr = m_pD3D->CreateDevice(
		iAdapter,
		devType,
#ifdef FOURSTORY_CLIENT_NATIVE
		fourstory_native_window(pWnd->GetSafeHwnd()),
#else
		pWnd->GetSafeHwnd(),
#endif
		dwBehavior,
		&m_vPRESENT,
		&m_pDevice);

	if(FAILED(hr))
	{
		m_pD3D->Release();
		m_pD3D = NULL;

		return FALSE;
	}
#ifdef FOURSTORY_CLIENT_NATIVE
	if(!fourstory::shell::set_game_fullscreen(pWnd->GetSafeHwnd(), !m_option.m_bWindowedMode))
	{
		m_option.m_bWindowedMode = !fourstory::shell::game_fullscreen(pWnd->GetSafeHwnd());
		LogGraphicsState("native fullscreen request failed", E_FAIL);
	}
#endif
	InitCAPS();
#ifdef FOURSTORY_CLIENT_MOD
	ApplyFilterQuality();
	UpdateUIScreenSize();
	m_dwGoodScreenX = m_option.m_dwScreenX;
	m_dwGoodScreenY = m_option.m_dwScreenY;
	LogGraphicsState("device created");
#endif

	if( m_option.m_nTextureDetail == TEXTURE_DETAIL_COUNT )
		if( m_lSYSMEM < 1000000000 )
			m_option.m_nTextureDetail = TEXTURE_DETAIL_LOW;
		else if( m_lSYSMEM < 2000000000 || m_lVIDEOMEM < 256 )
			m_option.m_nTextureDetail = TEXTURE_DETAIL_MED;
		else
			m_option.m_nTextureDetail = TEXTURE_DETAIL_HI;

	if( (m_vCAPS.VertexShaderVersion & 0x0000FFFF) < 0x0300 ||
		(m_vCAPS.PixelShaderVersion & 0x0000FFFF) < 0x0300 ||
		m_lVIDEOMEM < 256 )
		m_option.m_bUseSHADER = FALSE;

	D3DVERTEXELEMENT9 vWMESHDECL[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0},
		{ 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0},
		{ 0, 28, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{ 0, 40, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()};

	D3DVERTEXELEMENT9 vMESHDECL[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		{ 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
		D3DDECL_END()};

	D3DVERTEXELEMENT9 vLDECL[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{ 1, 0, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
		{ 2, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()};

	LPD3DVERTEXELEMENT9 pDECL[] = {
		vWMESHDECL,
		vMESHDECL,
		vLDECL};

	for( int i=0; i<VS_COUNT; i++)
	{
		m_pDevice->CreateVertexDeclaration(
			pDECL[i],
			&m_pDECL[i]);
	}

	if(m_option.m_bUseSHADER)
	{
		for(auto i=0; i<VS_COUNT; i++)
			if(m_dwVertexShader[i])
			{
				HRSRC hResInfo = FindResource( NULL, MAKEINTRESOURCE(m_dwVertexShader[i]), LPCSTR(m_strResourceType));
				HGLOBAL hGlobal = LoadResource( NULL, hResInfo);

				LPD3DXBUFFER pCode = NULL;

				hr = D3DXAssembleShader(
					(LPCSTR) LockResource(hGlobal),
					SizeofResource( NULL, hResInfo),
					NULL,
					NULL, 0,
					&pCode,
					NULL);

				if(SUCCEEDED(hr))
				{
					hr = m_pDevice->CreateVertexShader(
						(LPDWORD) pCode->GetBufferPointer(),
						&m_pVertexShader[i]);

					if(FAILED(hr))
					{
						m_pVertexShader[i] = NULL;
						m_bEnableSHADER = FALSE;
					}
				}
				else
					m_bEnableSHADER = FALSE;

				if(pCode)
					pCode->Release();
			}

		for( int i=0; i<PS_COUNT; i++)
			if(m_dwPixelShader[i])
			{
				HRSRC hResInfo = FindResource( NULL, MAKEINTRESOURCE(m_dwPixelShader[i]), LPCSTR(m_strResourceType));
				HGLOBAL hGlobal = LoadResource( NULL, hResInfo);

				LPD3DXBUFFER pCode = NULL;

				hr = D3DXAssembleShader(
					(LPCSTR) LockResource(hGlobal),
					SizeofResource( NULL, hResInfo),
					NULL,
					NULL, 0,
					&pCode,
					NULL);

				if(SUCCEEDED(hr))
				{
					hr = m_pDevice->CreatePixelShader(
						(LPDWORD) pCode->GetBufferPointer(),
						&m_pPixelShader[i]);

					if(FAILED(hr))
					{
						m_pPixelShader[i] = NULL;
						m_bEnableSHADER = FALSE;
					}
				}
				else
					m_bEnableSHADER = FALSE;

				if(pCode)
					pCode->Release();
			}
	}
	else
		m_bEnableSHADER = FALSE;

	if(!m_bEnableSHADER)
	{
		for(auto i=0; i<VS_COUNT; i++)
			if(m_pVertexShader[i])
			{
				m_pVertexShader[i]->Release();
				m_pVertexShader[i] = NULL;
			}

		for(auto i=0; i<PS_COUNT; i++)
			if(m_pPixelShader[i])
			{
				m_pPixelShader[i]->Release();
				m_pPixelShader[i] = NULL;
			}
	}

	m_pMainWnd = pWnd;
	InitBACK(&d3ddm);

	return TRUE;
}

BOOL CD3DDevice::Reset()
{
	D3DDISPLAYMODE d3ddm;

	if( !m_pD3D || !m_pDevice )
		return FALSE;

#ifdef FOURSTORY_CLIENT_MOD
	if( m_pDevice->TestCooperativeLevel() == D3DERR_DEVICELOST )
	{
		m_dwLostCount++;
		return FALSE;
	}
#else
	if( m_pDevice->TestCooperativeLevel() == D3DERR_DEVICELOST )
		return FALSE;
#endif

	HRESULT hr = m_pD3D->GetAdapterDisplayMode(
		D3DADAPTER_DEFAULT,
		&d3ddm);

	if (FAILED(hr))

	{
		ReleaseDevice();
		return FALSE;
	}




	if(!m_option.m_bWindowedMode)







		d3ddm.Format = D3DFMT_X8R8G8B8;


	ZeroMemory( &m_vPRESENT, sizeof(m_vPRESENT));
	d3ddm.Width = m_option.m_dwScreenX;
	d3ddm.Height = m_option.m_dwScreenY;

	m_vPRESENT.PresentationInterval = m_option.m_dwPresentationInterval;
	m_vPRESENT.SwapEffect = (m_vCAPS.Caps3 & D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD && m_vCAPS.DevCaps & D3DDEVCAPS_EXECUTEVIDEOMEMORY) ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_COPY;


#ifdef FOURSTORY_CLIENT_NATIVE
	// Native Spaces owns fullscreen; avoid exclusive display-mode changes.
	m_vPRESENT.Windowed = TRUE;
#else
	m_vPRESENT.Windowed = m_option.m_bWindowedMode;
#endif
	m_vPRESENT.BackBufferWidth = d3ddm.Width;
	m_vPRESENT.BackBufferHeight = d3ddm.Height;
	m_vPRESENT.BackBufferFormat = d3ddm.Format;

	m_vPRESENT.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

	if (!m_vPRESENT.Windowed)
		m_vPRESENT.FullScreen_RefreshRateInHz = d3ddm.RefreshRate;


	m_vPRESENT.AutoDepthStencilFormat = GetBestZFormat(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		d3ddm.Format);

	m_vPRESENT.EnableAutoDepthStencil = TRUE;
	ReleaseBACK();

#ifdef FOURSTORY_CLIENT_MOD
	HRESULT hrRESET = m_pDevice->Reset(&m_vPRESENT);
	if(FAILED(hrRESET))
	{
		LogGraphicsState( "device reset FAILED", hrRESET);

		// The original releases the device here and the client then crashes.
		// A refused mode (for example a fullscreen size the display no longer
		// offers) falls back once to a window of the last size that worked.
		static BYTE bFALLBACK = FALSE;

		if( !bFALLBACK && m_dwGoodScreenX && m_dwGoodScreenY )
		{
			bFALLBACK = TRUE;
			m_option.m_bWindowedMode = TRUE;
			m_option.m_dwScreenX = m_dwGoodScreenX;
			m_option.m_dwScreenY = m_dwGoodScreenY;

			BOOL bRESULT = Reset();
			bFALLBACK = FALSE;

			if(m_pDevice)
				return bRESULT;
		}
		else if(bFALLBACK)
			return FALSE;

		ReleaseDevice();
#else
	if(FAILED(m_pDevice->Reset(&m_vPRESENT)))
	{
		ReleaseDevice();
#endif
		return FALSE;
	}

	m_pDevice->GetDeviceCaps(&m_vCAPS);
#ifdef FOURSTORY_CLIENT_MOD
	ApplyFilterQuality();
	UpdateUIScreenSize();
	m_dwGoodScreenX = m_option.m_dwScreenX;
	m_dwGoodScreenY = m_option.m_dwScreenY;
	LogGraphicsState("device reset");
#endif
	InitBACK(&d3ddm);

	return TRUE;
}

BOOL CD3DDevice::ResetHost()
{
	if(m_pMainWnd)
		return m_bUseTHREAD ? ((CTachyonWnd *) m_pMainWnd)->ResetThreadDevices() : ((CTachyonWnd *) m_pMainWnd)->ResetDevices();

	return FALSE;
}

void CD3DDevice::ReleaseDevice()
{
	CD3DLight::SetDevice(NULL);
	ReleaseBACK();

	for( int i=0; i<VS_COUNT; i++)
	{
		if(m_pVertexShader[i])
		{
			m_pVertexShader[i]->Release();
			m_pVertexShader[i] = NULL;
		}

		if(m_pDECL[i])
		{
			m_pDECL[i]->Release();
			m_pDECL[i] = NULL;
		}
	}

	for(auto i=0; i<PS_COUNT; i++)
		if(m_pPixelShader[i])
		{
			m_pPixelShader[i]->Release();
			m_pPixelShader[i] = NULL;
		}

	if(m_pDevice)
	{
		m_pDevice->Release();
		m_pDevice = NULL;
	}

	if(m_pD3D)
	{
		m_pD3D->Release();
		m_pD3D = NULL;
	}

	ZeroMemory( m_vConstantVS, VC_COUNT * sizeof(WORD));
	ZeroMemory( m_vConstantPS, PC_COUNT * sizeof(WORD));
	ZeroMemory( &m_vPRESENT, sizeof(m_vPRESENT));
	ZeroMemory( &m_vCAPS, sizeof(D3DCAPS9));
}

LPDIRECT3DTEXTURE9 CD3DDevice::LoadTexture( LPBYTE pResData,
										    UINT nLength,
											UINT nMipLevels)
{
	LPDIRECT3DTEXTURE9 pResult = NULL;

	if( !pResData || nLength <= 0 )
		return pResult;

	D3DXCreateTextureFromFileInMemoryEx(
		m_pDevice,
		pResData,
		nLength,
		D3DX_DEFAULT,
		D3DX_DEFAULT,
		nMipLevels, 0,
		D3DFMT_DXT3,
		D3DPOOL_MANAGED,
		D3DX_FILTER_NONE,
		D3DX_DEFAULT,
		0, NULL,
		NULL,
		&pResult);

	return pResult;
}

void CD3DDevice::BeginGLOWScene()
{
	if(!m_bEnableSHADER)
		return;

	m_pDevice->SetRenderTarget( 0, m_pBACKBUF);
	m_pDevice->Clear(
		0, NULL,
		D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
		D3DCOLOR_ARGB( 0, 0, 0, 0),
		1.0f, 0);
}

void CD3DDevice::EndGLOWScene( CD3DCamera *pCAMERA,
							   FLOAT fGlowRange)
{
	CD3DCamera vCAMERA;
	D3DXMATRIX vWorld;
	FLOAT fMIP = 2.0f;

	if(!m_bEnableSHADER)
		return;

	m_pDevice->SetRenderTarget( 0, m_pRTARGET);
	D3DXMatrixIdentity(&vWorld);

	vCAMERA.InitOrthoCamera(
		m_pDevice,
		-1.0f,
		1.0f,
		1.0f,
		1.0f);

	vCAMERA.SetPosition(
		D3DXVECTOR3( 0.0f, 0.5f, 0.0f),
		D3DXVECTOR3( 0.0f, 0.0f, 0.0f),
		D3DXVECTOR3( 0.0f, 0.0f, 1.0f),
		FALSE);
	vCAMERA.Activate(TRUE);

	m_pDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

	m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	m_pDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	m_pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	m_pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	m_pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	m_pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	m_pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE);
	m_pDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE);
	m_pDevice->SetRenderState( D3DRS_FOGENABLE, FALSE);

	m_pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE);
	m_pDevice->SetRenderState( D3DRS_ZENABLE, FALSE);

	m_pDevice->SetStreamSource( 0, m_pBACKVB, 0, sizeof(PVERTEX));
	m_pDevice->SetTexture( 0, m_pBACKTEX);

	m_pDevice->SetTransform( D3DTS_WORLD, &vWorld);
	m_pDevice->SetFVF(T3DFVF_PVERTEX);

	m_pDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);
	m_pDevice->SetRenderTarget( 0, m_pBACKBUF);
	pCAMERA->Activate(TRUE);

	m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
	m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFF000000);
	m_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW);
	m_pDevice->SetRenderState( D3DRS_ZENABLE, TRUE);

	D3DXMatrixScaling(
		&vWorld,
		fGlowRange,
		fGlowRange,
		fGlowRange);

	vWorld._41 = pCAMERA->m_vTarget.x;
	vWorld._42 = pCAMERA->m_vTarget.y;
	vWorld._43 = pCAMERA->m_vTarget.z;

	m_pDevice->SetStreamSource( 0, m_pEraserVB, 0, sizeof(D3DXVECTOR3));
	m_pDevice->SetIndices(m_pEraserIB);

	m_pDevice->SetTransform( D3DTS_WORLD, &vWorld);
	m_pDevice->SetFVF(D3DFVF_XYZ);

	m_pDevice->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST, 0, 0,
		SPHERE_VERTEXCOUNT, 0,
		SPHERE_INDEXCOUNT / 3);

	m_pDevice->SetRenderTarget( 0, m_pGLOWBUF);
	D3DXMatrixIdentity(&vWorld);
	vCAMERA.Activate(TRUE);

	m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DOTPRODUCT3);
	m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
	m_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW);
	m_pDevice->SetRenderState( D3DRS_ZENABLE, FALSE);

	m_pDevice->SetStreamSource( 0, m_pBACKVB, 0, sizeof(PVERTEX));
	m_pDevice->SetTransform( D3DTS_WORLD, &vWorld);
	m_pDevice->SetTexture( 0, m_pBACKTEX);
	m_pDevice->SetFVF(T3DFVF_PVERTEX);
	m_pDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

	m_pDevice->SetRenderTarget( 0, m_pGRAYBUF);
	m_pDevice->Clear(
		0, NULL,
		D3DCLEAR_TARGET,
		D3DCOLOR_ARGB( 0, 0, 0, 0),
		1.0f, 0);
	m_pGLOWTEX->GenerateMipSubLevels();

	m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pDevice->SetRenderState( D3DRS_BLENDOPALPHA, D3DBLENDOP_MAX);
	m_pDevice->SetRenderState( D3DRS_BLENDFACTOR, 0xFF505050);
	m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE);

	m_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_BLENDFACTOR);
	m_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_BLENDFACTOR);
	m_pDevice->SetTexture( 0, m_pGLOWTEX);

	for( DWORD i=0; i<5; i++)
	{
		m_pDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *((LPDWORD) &fMIP));
		m_pDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

		fMIP += 1.0f;
	}
	fMIP = 1.0f;

	m_pDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *((LPDWORD) &fMIP));
	m_pDevice->SetRenderState( D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
	m_pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE);
	m_pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE);

	m_pDevice->SetRenderTarget( 0, m_pRTARGET);
	m_pDevice->SetTexture( 0, m_pGRAYTEX);
	m_pDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2);

	m_pDevice->SetRenderState( D3DRS_FOGENABLE, TRUE);
	pCAMERA->Activate(TRUE);
}

void CD3DDevice::InitCAPS()
{
#ifdef FOURSTORY_CLIENT_NATIVE
	// No DxDiag on macOS. ullPhysicalMemory is the physical memory in bytes;
	// szDisplayMemoryEnglish ("<n> MB") becomes the device's texture memory in MB.
	MEMORYSTATUSEX vSTATUS;
	vSTATUS.dwLength = sizeof(vSTATUS);
	m_lSYSMEM = GlobalMemoryStatusEx(&vSTATUS) ? vSTATUS.ullTotalPhys : 0;
	m_lVIDEOMEM = m_pDevice ? m_pDevice->GetAvailableTextureMem() / (1024 * 1024) : 0;
#else
	IDxDiagProvider *pProvider = NULL;
	DXDIAG_INIT_PARAMS vPARAM;

	if( FAILED(CoCreateInstance(
		CLSID_DxDiagProvider,
		NULL,
		CLSCTX_INPROC_SERVER,
		IID_IDxDiagProvider,
		(LPVOID *) &pProvider)))
		return;

	ZeroMemory( &vPARAM, sizeof(DXDIAG_INIT_PARAMS));
	vPARAM.dwDxDiagHeaderVersion = DXDIAG_DX9_SDK_VERSION;
	vPARAM.bAllowWHQLChecks = TRUE;
	vPARAM.pReserved = NULL;
	vPARAM.dwSize = sizeof(DXDIAG_INIT_PARAMS);

	if(SUCCEEDED(pProvider->Initialize(&vPARAM)))
	{
		IDxDiagContainer *pROOT = NULL;

		if(SUCCEEDED(pProvider->GetRootContainer(&pROOT)))
		{
			IDxDiagContainer *pContainer = NULL;

			if(SUCCEEDED(pROOT->GetChildContainer( L"DxDiag_SystemInfo", &pContainer)))
			{
				VARIANT vVALUE;
				VariantInit(&vVALUE);

				if(SUCCEEDED(pContainer->GetProp( L"ullPhysicalMemory", &vVALUE)))
					m_lSYSMEM = _wtoi64(vVALUE.bstrVal);

				VariantClear(&vVALUE);
				pContainer->Release();
			}

			if(SUCCEEDED(pROOT->GetChildContainer( L"DxDiag_DisplayDevices", &pContainer)))
			{
				DWORD dwCount = 0;

				if( SUCCEEDED(pContainer->GetNumberOfChildContainers(&dwCount)) && dwCount > 0 )
				{
					WCHAR szNAME[MAX_PATH];

					if(SUCCEEDED(pContainer->EnumChildContainerNames( 0, szNAME, MAX_PATH)))
					{
						IDxDiagContainer *pDisplay = NULL;

						if(SUCCEEDED(pContainer->GetChildContainer( szNAME, &pDisplay)))
						{
							VARIANT vVALUE;
							VariantInit(&vVALUE);

							if(SUCCEEDED(pDisplay->GetProp( L"szDisplayMemoryEnglish", &vVALUE)))
								m_lVIDEOMEM = _wtoi64(vVALUE.bstrVal);

							VariantClear(&vVALUE);
							pDisplay->Release();
						}
					}
				}

				pContainer->Release();
			}

			pROOT->Release();
		}
	}

	pProvider->Release();
#endif
}
