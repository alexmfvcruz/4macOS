	MAPTGUILDTACTICSWANTEDAPP::iterator it = m_mapTGuildTacticsWantedApp.find(dwCharID);
	if(it!=m_mapTGuildTacticsWantedApp.end())
	{
		if((*it).second->m_dwWantedGuildID == dwGuildID)
			return GUILD_SAME;

		return GUILD_AREADYAPPLY;
	}

	LPTGUILDTACTICSWANTED pWanted = FindGuildTacticsWanted(dwGuildID, dwID);
	if(!pWanted)
		return GUILD_FAIL;

	if(pTCHAR &&
		pWanted->m_bCountry != GetWarCountry(pTCHAR))
		return GUILD_FAIL;

	if(pWanted->m_dlEndTime < m_timeCurrent)
		return GUILD_WANTEDEND;

	if(	pWanted->m_bMaxLevel < bLevel ||
		pWanted->m_bMinLevel > bLevel)
		return GUILD_MISMATCHLEVEL;

	if(pTCHAR &&
		pTCHAR->m_pGuild &&
		pTCHAR->m_pGuild->m_dwID == dwGuildID)
		return GUILD_SAMEGUILDTACTICS;

	LPTGUILDTACTICSWANTEDAPP pApp = new TGUILDTACTICSWANTEDAPP();
	pApp->m_dwWantedID = dwID;
	pApp->m_dwWantedGuildID = pWanted->m_dwGuildID;
	pApp->m_dwRegion = 0;
	pApp->m_bClass = bClass;
	pApp->m_bLevel = bLevel;
	pApp->m_dwCharID = dwCharID;
	pApp->m_strName = strNAME;

	pApp->m_bDay = pWanted->m_bDay;
	pApp->m_dwCooper = pWanted->m_dwCooper;
	pApp->m_dwSilver = pWanted->m_dwSilver;
	pApp->m_dwGold = pWanted->m_dwGold;
	pApp->m_dwPoint = pWanted->m_dwPoint;

	pWanted->m_mapTWantedApp.insert(MAPTGUILDTACTICSWANTEDAPP::value_type(pApp->m_dwCharID, pApp));
	m_mapTGuildTacticsWantedApp.insert(MAPTGUILDTACTICSWANTEDAPP::value_type(pApp->m_dwCharID, pApp));

	return GUILD_SUCCESS;
