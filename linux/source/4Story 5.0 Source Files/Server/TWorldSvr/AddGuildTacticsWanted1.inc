	if(!pGuild)
		return GUILD_FAIL;

	LPTGUILDTACTICSWANTED pWanted = FindGuildTacticsWanted(pGuild->m_dwID, dwID);
	if(!pWanted)
	{
		TGUILDTACTICSWANTED wanted;
		wanted.m_dwGuildID = pGuild->m_dwID;
		wanted.m_dwID = dwID;

		LPVTGUILDTACTICSWANTED pList = FindGuildTacticsList(pGuild->m_dwID);
		if(pList)
		{
			if(pList->size() >= MAX_TACTICSWANTED)
				return GUILD_MAXWANTED;

			pList->push_back(wanted);
		}
		else
		{
			VTGUILDTACTICSWANTED list;
			list.push_back(wanted);
			m_mapTGuildTacticsWanted.insert(MAPVTGUILDTACTICSWANTED::value_type(pGuild->m_dwID, list));
		}

		pWanted = FindGuildTacticsWanted(pGuild->m_dwID, wanted.m_dwID);
		if(!pWanted)
			return GUILD_FAIL;
	}

	pWanted->m_bMaxLevel = bMaxLevel;
	pWanted->m_bMinLevel = bMinLevel;
	pWanted->m_bCountry = pGuild->m_bCountry;
	pWanted->m_strName = pGuild->m_strName;
	pWanted->m_strText = strText;
	pWanted->m_strTitle = strTitle;

	pWanted->m_bDay = bDay;
	pWanted->m_dwCooper = dwCooper;
	pWanted->m_dwGold = dwGold;
	pWanted->m_dwSilver = dwSilver;
	pWanted->m_dwPoint = dwPoint;

	if(dlEndTime)
		pWanted->m_dlEndTime = dlEndTime;
	else
		pWanted->m_dlEndTime = m_timeCurrent + GUILDWANTED_PERIOD;

	return GUILD_SUCCESS;
