// C2 CPU host for the original CD3DDevice data members. No renderer is started.
#include <fourstory_engine_resources.h>
DWORD CD3DDevice::m_dwPolyCount=0;
#ifdef FOURSTORY_CLIENT_MOD
BYTE CD3DDevice::m_bAnisotropic=FALSE;
FLOAT CD3DDevice::m_fExtBias=0;
FLOAT CD3DDevice::m_fUIScale=1;
DWORD CD3DDevice::m_dwLostCount=0;
DWORD CD3DDevice::m_dwGoodScreenX=0,CD3DDevice::m_dwGoodScreenY=0;
LPDIRECT3DTEXTURE9 CD3DDevice::m_pUITEX=nullptr;
LPDIRECT3DSURFACE9 CD3DDevice::m_pUIBUF=nullptr,CD3DDevice::m_pUISAVED=nullptr,CD3DDevice::m_pUIREAL=nullptr;
BYTE CD3DDevice::m_bInUIScene=FALSE;
#endif
CD3DDevice::CD3DDevice():m_bUseTHREAD(FALSE),m_bRESET(FALSE),m_pDevice(nullptr),m_pD3D(nullptr),m_pVertexShader{},m_pPixelShader{},m_pDECL{},m_bEnableSHADER(FALSE),m_dwVertexShader{},m_dwPixelShader{},m_vConstantVS{},m_vConstantPS{},m_vPRESENT{},m_option{},m_vCAPS{},m_lVIDEOMEM(0),m_lSYSMEM(0),m_pBACKVB(nullptr),m_pGRAYTEX(nullptr),m_pGLOWTEX(nullptr),m_pBACKTEX(nullptr),m_pBACKTEX2(nullptr),m_pGRAYBUF(nullptr),m_pGLOWBUF(nullptr),m_pBACKBUF(nullptr),m_pBACKBUF2(nullptr),m_pRTARGET(nullptr),m_pZBUF(nullptr),m_pEraserVB(nullptr),m_pEraserIB(nullptr),m_pMainWnd(nullptr){InitializeCriticalSection(&m_cs);}
CD3DDevice::~CD3DDevice(){if(m_pDevice)m_pDevice->Release();if(m_pD3D)m_pD3D->Release();DeleteCriticalSection(&m_cs);}
