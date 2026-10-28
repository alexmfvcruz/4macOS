	MAPTNMTPLAYER::iterator it = m_mapTNMTPlayer.find(dwCharID);
	if(it!=m_mapTNMTPlayer.end())
		return (*it).second;

	return NULL;
