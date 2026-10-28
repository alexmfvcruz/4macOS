	MAPTGUILDWANTED::iterator it = m_mapTGuildWanted.find(dwGuildID);
	if(it==m_mapTGuildWanted.end())
		return NULL;

	return &((*it).second);
