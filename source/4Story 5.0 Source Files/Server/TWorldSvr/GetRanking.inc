	dwRank = dwMonthRank = 0;

	MAPDWORD::iterator it = m_mapRank.find(dwCharID);
	if(it!=m_mapRank.end())
		dwRank = (*it).second;

	it = m_mapMonthRank.find(dwCharID);
	if(it!=m_mapMonthRank.end())
		dwMonthRank = (*it).second;
