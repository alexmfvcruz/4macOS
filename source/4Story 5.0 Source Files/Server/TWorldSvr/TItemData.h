#pragma once
// Original role item fields, shared with native query declarations.
struct tagTITEM
{
	__int64 m_dlID;
	DWORD m_dwItemID;
	BYTE m_bItemID;
	WORD m_wItemID;
	BYTE m_bType;
	BYTE m_bKind;
	BYTE m_bLevel;
	BYTE m_bGem;
	WORD m_wMoggItemID;
	BYTE m_bCount;
	BYTE m_bGLevel;
	DWORD m_dwDuraMax;
	DWORD m_dwDuraCur;
	BYTE m_bRefineCur;
	__time64_t m_dEndTime;
	BYTE m_bGradeEffect;
	BYTE m_bMagicCount;
	BYTE m_bMagic[TMAGIC_MAX];
	WORD m_wValue[TMAGIC_MAX];
	DWORD m_dwExtValue[TMAGIC_MAX];

	tagTITEM()
	{
		m_dlID = 0;
		m_dwItemID = 0;
		m_bType = 0;
		m_bKind = 0;
		m_dEndTime = 0;
		m_bMagicCount = 0;
		m_bGradeEffect = 0;

		for(BYTE i=0; i<TMAGIC_MAX; i++)
		{
			m_bMagic[i] = 0;
			m_wValue[i] = 0;
			m_dwExtValue[i] = 0;
		}
	};
};
