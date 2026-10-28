	MAPTGUILDWANTEDAPP::iterator it = m_mapTGuildWantedApp.find(dwCharID);
	if(it!=m_mapTGuildWantedApp.end())
	{
		if((*it).second->m_dwWantedID == dwID)
			return GUILD_SAME;

		return GUILD_AREADYAPPLY;
	}

	LPTGUILDWANTED pWanted = FindGuildWanted(dwID);
	if(!pWanted)
		return GUILD_FAIL;

	if(pTCHAR &&
		pWanted->m_bCountry != pTCHAR->m_bCountry)
		return GUILD_FAIL;

	if(pWanted->m_dlEndTime < m_timeCurrent)
		return GUILD_WANTEDEND;

	if(pWanted->m_bMaxLevel < bLevel ||
		pWanted->m_bMinLevel >bLevel)
		return GUILD_MISMATCHLEVEL;

	if(pTCHAR &&
		pTCHAR->m_pTactics &&
		pTCHAR->m_pTactics->m_dwID == pWanted->m_dwGuildID)
		return GUILD_SAMEGUILDTACTICS;

	LPTGUILDWANTEDAPP pApp = new TGUILDWANTEDAPP();
	pApp->m_dwWantedID = dwID;
	pApp->m_dwRegion = 0;
	pApp->m_bClass = bClass;
	pApp->m_bLevel = bLevel;
	pApp->m_dwCharID = dwCharID;
	pApp->m_strName = strNAME;

	pWanted->m_mapTWantedApp.insert(MAPTGUILDWANTEDAPP::value_type(pApp->m_dwCharID, pApp));
	m_mapTGuildWantedApp.insert(MAPTGUILDWANTEDAPP::value_type(pApp->m_dwCharID, pApp));

	return GUILD_SUCCESS;
