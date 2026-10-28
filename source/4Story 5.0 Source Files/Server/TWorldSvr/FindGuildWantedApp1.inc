	MAPTGUILDWANTEDAPP::iterator it = m_mapTGuildWantedApp.find(dwCharID);
	if(it!=m_mapTGuildWantedApp.end())
		return (*it).second;

	return NULL;
