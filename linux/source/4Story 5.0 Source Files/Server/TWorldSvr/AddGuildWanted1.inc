	if(!pGuild)
		return GUILD_FAIL;

	LPTGUILDWANTED pWanted = FindGuildWanted(pGuild->m_dwID);
	if(!pWanted)
	{
		TGUILDWANTED wanted;
		wanted.m_dwGuildID = pGuild->m_dwID;

		m_mapTGuildWanted.insert(MAPTGUILDWANTED::value_type(pGuild->m_dwID, wanted));
		pWanted = FindGuildWanted(pGuild->m_dwID);
		if(!pWanted)
			return GUILD_FAIL;
	}

	pWanted->m_bMaxLevel = bMaxLevel;
	pWanted->m_bMinLevel = bMinLevel;
	pWanted->m_bCountry = pGuild->m_bCountry;
	pWanted->m_strName = pGuild->m_strName;
	pWanted->m_strText = strText;
	pWanted->m_strTitle = strTitle;

	if(dlEndTime)
		pWanted->m_dlEndTime = dlEndTime;
	else
		pWanted->m_dlEndTime = m_timeCurrent + GUILDWANTED_PERIOD;

	return GUILD_SUCCESS;
