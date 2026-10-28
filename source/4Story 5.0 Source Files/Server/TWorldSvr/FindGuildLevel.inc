	LPTGUILDLEVEL pLevel = NULL;
	
	MAPTGUILDLEVEL::iterator itLevel = m_mapTGuildLevel.find(bLevel);
	if(itLevel != m_mapTGuildLevel.end())
		pLevel = (*itLevel).second;

	return pLevel;
