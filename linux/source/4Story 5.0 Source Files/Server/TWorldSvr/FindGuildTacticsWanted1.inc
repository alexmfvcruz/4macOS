	LPVTGUILDTACTICSWANTED pList = FindGuildTacticsList(dwGuildID);
	if(!pList)
		return NULL;

	for(DWORD i=0; i<pList->size(); i++)
	{
		if((*pList)[i].m_dwID == dwID)
			return &((*pList)[i]);
	}

	return NULL;
