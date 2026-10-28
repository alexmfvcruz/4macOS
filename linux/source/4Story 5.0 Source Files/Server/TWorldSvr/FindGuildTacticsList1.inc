	MAPVTGUILDTACTICSWANTED::iterator it = m_mapTGuildTacticsWanted.find(dwGuildID);
	if(it!=m_mapTGuildTacticsWanted.end())
		return &((*it).second);

	return NULL;
