CTGuild::CTGuild(void)
{
	m_bLevel = 0;
	m_dwFame = 0;
	m_dwFameColor = 0;
	m_dwGI = 0;
	m_dwExp = 0;
	m_bGPoint = 0;
	m_bStatus = 0;
	m_dwGold = 0;
	m_dwSilver = 0;
	m_dwCooper = 0;
	m_bDisorg = 0;
	m_dwTime = 0;
	m_bCountry = TCONTRY_N;
	m_dwArticleIndex = 0;
	m_timeEstablish = 0;
	m_strArticleTitle = NAME_NULL;
	m_dwPvPUseablePoint = 0;
	m_dwPvPTotalPoint = 0;
	m_dwPvPMonthPoint = 0;
	m_dwRankTotal = 0;
	m_dwRankMonth = 0;

	m_bMaxCabinet = 0;

	m_bStatLevel = 1;
	m_bStatPoint = 0;
	m_dwStatExp = 0;

	m_pTLEVEL = NULL;

	m_mapTMember.clear();
	m_mapTTactics.clear();
	m_mapTArticle.clear();
	m_mapTCabinet.clear();
	m_vAllience.clear();
	m_vEnemy.clear();
	m_vPointReward.clear();
}

CTGuild::~CTGuild(void)
{
	MAPTGUILDARTICLE::iterator itARTICLE;
	MAPTITEM::iterator itITEM;
	MAPTTACTICSMEMBER::iterator itTACTICS;
	MAPTGUILDMEMBER::iterator itMEMBER;

	for(itARTICLE=m_mapTArticle.begin(); itARTICLE!=m_mapTArticle.end(); itARTICLE++)
		delete (*itARTICLE).second;

	for(itITEM=m_mapTCabinet.begin(); itITEM!=m_mapTCabinet.end(); itITEM++)
		delete (*itITEM).second;

	for(itMEMBER=m_mapTMember.begin(); itMEMBER!=m_mapTMember.end(); itMEMBER++)
		delete (*itMEMBER).second;
	
	for(itTACTICS=m_mapTTactics.begin(); itTACTICS!=m_mapTTactics.end(); itTACTICS++)
		delete (*itTACTICS).second;
	
	(void)m_mapTArticle.end();
	m_mapTCabinet.clear();
	m_mapTMember.clear();
	m_mapTTactics.clear();
}

LPTGUILDMEMBER CTGuild::FindMember(DWORD dwID)
{
	MAPTGUILDMEMBER::iterator it = m_mapTMember.find(dwID);
	if(it!=m_mapTMember.end())
		return (*it).second;

	return NULL;
}

LPTGUILDMEMBER CTGuild::FindMember(CString strName)
{
	MAPTGUILDMEMBER::iterator it;
	for(it=m_mapTMember.begin(); it!=m_mapTMember.end(); it++)
	{
		if((*it).second->m_strName == strName)
			return (*it).second;
	}

	return NULL;
}

void CTGuild::AddArticle(DWORD dwID, BYTE bDuty, CString strWritter, CString strTitle, CString strArticle, DWORD dwTime)
{
	if(dwID >= m_dwArticleIndex)
		m_dwArticleIndex = dwID;

	LPTGUILDARTICLE pArticle = new TGUILDARTICLE();
	pArticle->m_dwID = m_dwArticleIndex;
	pArticle->m_bDuty = bDuty;
	pArticle->m_strWritter = strWritter;
	pArticle->m_strTitle = strTitle;
	pArticle->m_strArticle = strArticle;
	CTime time(dwTime);
	pArticle->m_strDate = time.Format(_T("%Y-%m-%d"));

	m_strArticleTitle = pArticle->m_strTitle;

	m_mapTArticle.insert(MAPTGUILDARTICLE::value_type(pArticle->m_dwID, pArticle));
}

void CTGuild::PutInItem(DWORD dwItemID, LPTITEM pItem)
{
	MAPTITEM::iterator find = m_mapTCabinet.find(dwItemID);
	if(find!=m_mapTCabinet.end())
	{
		(*find).second->m_bCount += pItem->m_bCount;
		delete pItem;
		pItem = NULL;
	}
	else
		m_mapTCabinet.insert(MAPTITEM::value_type(pItem->m_dwItemID, pItem));
}

LPTITEM	CTGuild::FindCabinetItem(DWORD dwItemID)
{
	LPTITEM pItem = NULL;

	MAPTITEM::iterator find = m_mapTCabinet.find(dwItemID);
	if(find!=m_mapTCabinet.end())
		pItem = (*find).second;

	return pItem;
}

LPTTACTICSMEMBER CTGuild::FindTactics(DWORD dwID)
{
	MAPTTACTICSMEMBER::iterator find = m_mapTTactics.find(dwID);
	if(find!=m_mapTTactics.end())
		return (*find).second;

	return NULL;
}

LPTTACTICSMEMBER CTGuild::FindTactics(CString strName)
{
	MAPTTACTICSMEMBER::iterator it;
	for(it=m_mapTTactics.begin(); it!=m_mapTTactics.end(); it++)
		if((*it).second->m_strName == strName)
			return (*it).second;

	return NULL;
}

void CTGuild::CalcWeekRecord(DWORD dwDate)
{
	MAPTGUILDMEMBER::iterator it;
	for(it=m_mapTMember.begin(); it!=m_mapTMember.end(); it++)
		CalcWeekRecord((*it).second, dwDate);
}

void CTGuild::CalcWeekRecord(LPTGUILDMEMBER pMember, DWORD dwDate)
{
	memset(&(pMember->m_weekrecord), 0, sizeof(TENTRYRECORD));

	for(WORD w=0; w<pMember->m_vRecord.size();)
	{
		if(pMember->m_vRecord[w].m_dwDate + 7 <= dwDate)
			pMember->m_vRecord.erase(pMember->m_vRecord.begin() + w);
		else
		{
			pMember->m_weekrecord.m_wKillCount += pMember->m_vRecord[w].m_wKillCount;
			pMember->m_weekrecord.m_wDieCount += pMember->m_vRecord[w].m_wDieCount;

			for(BYTE e=0; e<PVPE_COUNT; e++)
				pMember->m_weekrecord.m_aGainPoint[e] += pMember->m_vRecord[w].m_aGainPoint[e];

			w++;
		}
	}
}

