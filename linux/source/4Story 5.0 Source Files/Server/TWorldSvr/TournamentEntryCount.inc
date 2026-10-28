	if(!m_tournament.m_mapEntry)
		return 0;

	BYTE bCount=0;
	MAPTOURNAMENTENTRY::iterator it;
	for(it=m_tournament.m_mapEntry->begin(); it!=m_tournament.m_mapEntry->end(); it++)
	{
		if(m_tournament.m_bGroup == (*it).second->m_bGroup)
			bCount++;
	}

	return bCount;
