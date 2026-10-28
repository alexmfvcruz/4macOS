	MAPTOURNAMENT::iterator it = m_mapTournament.find(wTournamentID);
	if(it == m_mapTournament.end())
		return NULL;
	else
		return &((*it).second);
