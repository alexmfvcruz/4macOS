	LPTITEM pItem = new TITEM();
	pItem->m_dlID = item.m_dlID;
	pItem->m_dwItemID = item.m_dwItemID;
	pItem->m_wItemID = item.m_wItemID;
	pItem->m_bLevel = item.m_bLevel;
	pItem->m_bGem = item.m_bGem;
	pItem->m_wMoggItemID = item.m_wMoggItemID;
	pItem->m_bCount = item.m_bCount;
	pItem->m_bGLevel = item.m_bGLevel;
	pItem->m_dwDuraMax = item.m_dwDuraMax;
	pItem->m_dwDuraCur = item.m_dwDuraCur;
	pItem->m_bRefineCur = item.m_bRefineCur;
	pItem->m_dEndTime = item.m_dEndTime;
	pItem->m_bGradeEffect = item.m_bGradeEffect;
	pItem->m_dwExtValue[IEV_ELD] = item.m_dwExtValue[IEV_ELD];
	pItem->m_dwExtValue[IEV_WRAP] = item.m_dwExtValue[IEV_WRAP];
	pItem->m_dwExtValue[IEV_COLOR] = item.m_dwExtValue[IEV_COLOR];
	pItem->m_dwExtValue[IEV_GUILD] = item.m_dwExtValue[IEV_GUILD];
	for(BYTE i=0; i<TMAGIC_MAX; i++)
	{
		if(item.m_bMagic[i])
		{
			pItem->m_bMagic[i] = item.m_bMagic[i];
			pItem->m_wValue[i] = item.m_wValue[i];
			pItem->m_bMagicCount++;
		}
		else
			break;
	}

	return pItem;
