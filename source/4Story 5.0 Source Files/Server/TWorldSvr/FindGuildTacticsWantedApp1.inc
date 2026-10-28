	MAPTGUILDTACTICSWANTEDAPP::iterator it = m_mapTGuildTacticsWantedApp.find(dwCharID);
	if(it!=m_mapTGuildTacticsWantedApp.end())
		return (*it).second;

	return NULL;
