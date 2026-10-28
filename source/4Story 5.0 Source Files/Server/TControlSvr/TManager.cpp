#include "StdAfx.h"


CTManager::CTManager()
{	
	m_bUpload = FALSE;
	m_bAuthority = 0;
	m_strID = _T("");
	m_bLock = TRUE;
}

CTManager::~CTManager()
{
}

// 현승룡 권한을 검사하여 결과를 리턴
BYTE CTManager::CheckAuthority(BYTE m_bClass) 
{
#include "CheckManagerAuthority.inc"
}

