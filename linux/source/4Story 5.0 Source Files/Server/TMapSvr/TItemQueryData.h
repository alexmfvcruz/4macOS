#pragma once
// Original role item fields, shared with native query declarations.
struct tagTITEMQUERY
{
	__int64 m_dlID;
	BYTE m_bItemID;
	WORD m_wItemID;
	BYTE m_bLevel;
	BYTE m_bCount;
	BYTE m_bGLevel;
	DWORD m_dwDuraMax;
	DWORD m_dwDuraCur;
	BYTE m_bRefineCur;
	TIMESTAMP_STRUCT m_dEndTime;
	BYTE m_bGradeEffect;
	BYTE m_bMagic[TMAGIC_MAX];
	WORD m_wValue[TMAGIC_MAX];
	DWORD m_dwExtValue[TMAGIC_MAX];
	BYTE m_bGem;
	WORD m_wMoggItemID;
	

	void Reset()
	{
		m_dlID = 0;
		m_bItemID = 0;
		m_wItemID = 0;
		m_bLevel = 0;
		m_bCount = 0;
		m_bGLevel = 0;
		m_dwDuraMax = 0;
		m_dwDuraCur = 0;
		m_bRefineCur = 0;
		m_bGradeEffect = 0;
		memset(&m_dEndTime, 0, sizeof(TIMESTAMP_STRUCT));
		memset(m_bMagic, 0, sizeof(BYTE)*TMAGIC_MAX);
		memset(m_wValue, 0, sizeof(WORD)*TMAGIC_MAX);
		memset(m_dwExtValue, 0, sizeof(DWORD)*TMAGIC_MAX);
		m_bGem = 0;
		m_wMoggItemID = 0;
	};
};
