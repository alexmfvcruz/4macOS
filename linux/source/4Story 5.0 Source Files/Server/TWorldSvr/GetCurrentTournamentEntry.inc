	if(m_tournament.m_mapEntry)
	{
		MAPTOURNAMENTENTRY::iterator it = m_tournament.m_mapEntry->find(bEntry);
		if(it!=m_tournament.m_mapEntry->end())
			return (*it).second;
	}

	return NULL;
