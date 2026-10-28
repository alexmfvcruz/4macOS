#ifdef FOURSTORY_CLIENT_NATIVE
#pragma once
#include <afx.h>
#include <d3dx9.h>
#include <vector>
#include <map>
#include <queue>
#include <set>
using namespace std;
typedef map<DWORD,LPD3DXPLANE> MAPPLANE, *LPMAPPLANE;
#define TIME_RATIO 4.8f
#define POINTPOS_ONPLANE											((BYTE) 0x00)
#define POINTPOS_FRONT												((BYTE) 0x01)
#define POINTPOS_BACK												((BYTE) 0x02)
using std::min;
using std::max;
class CD3DDevice;
class CTBSPPortal;
class CTBSPNode;
struct tagTEXTURESET;
typedef tagTEXTURESET TEXTURESET, *LPTEXTURESET;
#define T3DFVF_WMESHVERTEX											(D3DFVF_XYZB4|D3DFVF_LASTBETA_UBYTE4|D3DFVF_NORMAL|D3DFVF_TEX1)
#define T3DFVF_MESHVERTEX											(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2)
#define DEF_EPSILON													((FLOAT) 0.005f)
#define WEIGHT_COUNT												3
#define MP_COUNT													4
#define MP_LEFT														((BYTE) 0x00)
#define MP_TOP														((BYTE) 0x01)
#define MP_RIGHT													((BYTE) 0x02)
#define MP_BOTTOM													((BYTE) 0x03)
#define NODEID_NULL													((DWORD) 0xFFFFFFFF)
#define MLT_DIRECTION												((BYTE) 0x00)
#define MLT_POINT													((BYTE) 0x01)
#define MATRIXID_1ST(x)												static_cast<int>((((DWORD) (x)) & 0x000000FF))
#define MATRIXID_2ND(x)												static_cast<int>(((((DWORD) (x)) >> 8) & 0x000000FF))
#define MATRIXID_3RD(x)												static_cast<int>(((((DWORD) (x)) >> 16) & 0x000000FF))
#define MATRIXID_4TH(x)												static_cast<int>(((((DWORD) (x)) >> 24) & 0x000000FF))
typedef struct tagWMESHVERTEX WMESHVERTEX, *LPWMESHVERTEX;
typedef struct tagMESHVERTEX MESHVERTEX, *LPMESHVERTEX;
typedef struct tagLVERTEX LVERTEX, *LPLVERTEX;
typedef struct tagUVKEY UVKEY, *LPUVKEY;
typedef struct tagVERTEXWINDING VERTEXWINDING, *LPVERTEXWINDING;
typedef struct tagTEXWINDING TEXWINDING, *LPTEXWINDING;
typedef struct tagMAPLIGHT MAPLIGHT, *LPMAPLIGHT;
typedef struct tagPOINTWINDING POINTWINDING, *LPPOINTWINDING;
typedef vector<LPMESHVERTEX> VECTORVERTEX, *LPVECTORVERTEX;
typedef vector<LPD3DXVECTOR3> VECTORPOINT, *LPVECTORPOINT;
typedef vector<LPVERTEXWINDING> VECTORVERTEXWINDING, *LPVECTORVERTEXWINDING;
typedef vector<LPPOINTWINDING> VECTORPOINTWINDING, *LPVECTORPOINTWINDING;
typedef vector<CTBSPPortal*> VECTORTBSPPORTAL, *LPVECTORTBSPPORTAL;
typedef map<DWORD,CTBSPNode*> MAPTBSPNODE, *LPMAPTBSPNODE;
typedef map<DWORD,CTBSPPortal*> MAPTBSPPORTAL, *LPMAPTBSPPORTAL;
typedef map<DWORD,LPTEXWINDING> MAPTEXWINDING, *LPMAPTEXWINDING;
typedef map<DWORD,LPTEXTURESET> MAPTEXTURESET, *LPMAPTEXTURESET;
typedef map<DWORD,DWORD> MAPDWORD, *LPMAPDWORD;
typedef vector<WORD> VECTORWORD, *LPVECTORWORD;
typedef vector<LPBYTE> VECTORBYTE, *LPVECTORBYTE;
typedef vector<DWORD> VECTORDWORD, *LPVECTORDWORD;
typedef vector<LPVOID> VECTORPTR, *LPVECTORPTR;
struct tagWMESHVERTEX
{
	FLOAT m_fPosX;
	FLOAT m_fPosY;
	FLOAT m_fPosZ;

	FLOAT m_fWeight[WEIGHT_COUNT];
	DWORD m_dwMatIndex;

	FLOAT m_fNormalX;
	FLOAT m_fNormalY;
	FLOAT m_fNormalZ;

	FLOAT m_fU;
	FLOAT m_fV;
};
struct tagMESHVERTEX
{
	FLOAT m_fPosX;
	FLOAT m_fPosY;
	FLOAT m_fPosZ;

	FLOAT m_fNormalX;
	FLOAT m_fNormalY;
	FLOAT m_fNormalZ;

	FLOAT m_fU1;
	FLOAT m_fV1;
	FLOAT m_fU2;
	FLOAT m_fV2;
};
struct tagLVERTEX
{
	FLOAT m_fPosX;
	FLOAT m_fPosY;
	FLOAT m_fPosZ;

	DWORD m_dwColor;
	FLOAT m_fU;
	FLOAT m_fV;
};
struct tagUVKEY
{
	DWORD m_dwTick;
	FLOAT m_fKeySU;
	FLOAT m_fKeySV;
	FLOAT m_fKeyU;
	FLOAT m_fKeyV;
	FLOAT m_fKeyR;

	tagUVKEY()
	{
		m_dwTick = 0;

		m_fKeySU = 1.0f;
		m_fKeySV = 1.0f;

		m_fKeyU = 0.0f;
		m_fKeyV = 0.0f;
		m_fKeyR = 0.0f;
	};
};
struct tagVERTEXWINDING
{
	VECTORVERTEX m_vPoint;

	D3DXPLANE m_vPlane;
	BOOL m_bCheck;

	int m_nMeshIndex;
	int m_nIndex;
	int m_nCount;

	tagVERTEXWINDING()
	{
		m_vPoint.clear();

		m_nMeshIndex = 0;
		m_nIndex = 0;
		m_nCount = 0;
		m_bCheck = FALSE;
	};

	~tagVERTEXWINDING()
	{
		while(!m_vPoint.empty())
		{
			delete m_vPoint.back();
			m_vPoint.pop_back();
		}

		m_nMeshIndex = 0;
		m_nIndex = 0;
		m_nCount = 0;
		m_bCheck = FALSE;
	};
};
struct tagTEXWINDING
{
	VECTORVERTEXWINDING m_vWINDING;
	LPTEXTURESET m_pTEX;

	DWORD m_dwColor;

	tagTEXWINDING()
	{
		m_vWINDING.clear();

		m_dwColor = 0xFF000000;
		m_pTEX = NULL;
	};

	~tagTEXWINDING()
	{
		while(!m_vWINDING.empty())
		{
			delete m_vWINDING.back();
			m_vWINDING.pop_back();
		}

		m_dwColor = 0xFF000000;
		m_pTEX = NULL;
	};
};
struct tagMAPLIGHT
{
	D3DXVECTOR3 m_vPOS;
	D3DXVECTOR3 m_vDIR;

	FLOAT m_fRange;
	FLOAT m_fROT;

	DWORD m_dwTextureID;
	DWORD m_dwAmbient;
	DWORD m_dwDiffuse;

	BYTE m_bUseLM;
	BYTE m_bUseDL;
	BYTE m_bType;

	tagMAPLIGHT()
	{
		m_vPOS = D3DXVECTOR3( 0.0f, 0.0f, 0.0f);
		m_vDIR = D3DXVECTOR3( 0.0f, 0.0f, 0.0f);

		m_fRange = 0.0f;
		m_fROT = 0.0f;

		m_dwAmbient = 0x00000000;
		m_dwDiffuse = 0x00000000;
		m_dwTextureID = 0;

		m_bType = MLT_DIRECTION;
		m_bUseLM = TRUE;
		m_bUseDL = TRUE;
	};
};

typedef struct tagWEIGHTBUF WEIGHTBUF, *LPWEIGHTBUF;
struct tagWEIGHTBUF
{
	FLOAT m_fWeight;
	int m_nCount;

	tagWEIGHTBUF()
	{
		m_fWeight = 0.0f;
		m_nCount = 0;
	};
};
#define PORTALDIR_FRONT												((BYTE) 0x00)
#define PORTALDIR_BACK												((BYTE) 0x01)
#define WINDINGPOS_NONE												((BYTE) 0x00)
#define WINDINGPOS_FRONT											((BYTE) 0x01)
#define WINDINGPOS_ONFRONT											((BYTE) 0x02)
#define WINDINGPOS_BACK												((BYTE) 0x03)
#define WINDINGPOS_ONBACK											((BYTE) 0x04)
typedef vector<int>													VECTORINT, *LPVECTORINT;
typedef map< int, LPVECTORVERTEXWINDING>							MAPVERTEXWINDING, *LPMAPVERTEXWINDING;
typedef map< FLOAT, VECTORINT *>									MAPWEIGHTINDEXBUF, *LPMAPWEIGHTINDEXBUF;
typedef map< int, LPWEIGHTBUF>										MAPWEIGHTBUF, *LPMAPWEIGHTBUF;
#include "TBitStream.h"
#include "TachyonCompressor.h"
#include "Packet.h"
#include "TBSPPortal.h"
#include "TMath.h"
#include "TBSPNode.h"
#endif
