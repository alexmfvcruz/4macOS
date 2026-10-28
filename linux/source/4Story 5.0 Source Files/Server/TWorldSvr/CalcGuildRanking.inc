	MAPTGUILD::iterator it, itComp;
	for(it=m_mapTGuild.begin(); it!=m_mapTGuild.end(); it++)
	{
		CTGuild * pGuild = it->second;
		pGuild->m_dwRankTotal = 0;
		pGuild->m_dwRankMonth = 0;

		if(!pGuild->m_dwPvPTotalPoint && !pGuild->m_dwPvPMonthPoint)
			continue;

		for(itComp=m_mapTGuild.begin(); itComp!=m_mapTGuild.end(); itComp++)
		{
			if(!itComp->second->m_dwPvPTotalPoint && !itComp->second->m_dwPvPMonthPoint)
				continue;

			if(pGuild->m_dwPvPTotalPoint && pGuild->m_dwPvPTotalPoint < itComp->second->m_dwPvPTotalPoint)
				pGuild->m_dwRankTotal++;

			if(pGuild->m_dwPvPMonthPoint && pGuild->m_dwPvPMonthPoint < itComp->second->m_dwPvPMonthPoint)
				pGuild->m_dwRankMonth++;
		}

		if(pGuild->m_dwPvPTotalPoint)
			pGuild->m_dwRankTotal++;

		if(pGuild->m_dwPvPMonthPoint)
			pGuild->m_dwRankMonth++;
	}
