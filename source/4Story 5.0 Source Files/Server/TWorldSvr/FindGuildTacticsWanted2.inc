	MAPVTGUILDTACTICSWANTED::iterator itGW;
	for(itGW=m_mapTGuildTacticsWanted.begin(); itGW!=m_mapTGuildTacticsWanted.end(); itGW++)
	{
		LPVTGUILDTACTICSWANTED pList = &((*itGW).second);

		for(DWORD i=0; i<pList->size(); i++)
		{
			if((*pList)[i].m_dwID == dwID)
				return &((*pList)[i]);
		}
	}

	return NULL;
