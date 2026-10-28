	for (BYTE i = 0; i < BOW_TEAM_COUNT; ++i)
		m_bPoints[i] = BOW_MAX_POINTS / BOW_TEAM_COUNT;

	m_bRunning = FALSE;
	m_bStatus = BS_NORMAL;
	m_dwTick = 0;
	m_dwPeaceTick = 0;

	for (MAPBOWTEMPPLAYER::iterator itReg = m_mapBOWREG.begin(); itReg != m_mapBOWREG.end(); ++itReg)
		delete (*itReg).second;

	for (MAPBOWPLAYER::iterator itPlayer = m_mapBOWPLAYERS.begin(); itPlayer != m_mapBOWPLAYERS.end(); ++itPlayer)
		delete (*itPlayer).second;

	m_mapBOWREG.clear();
	m_mapBOWPLAYERS.clear();
	m_mapGuildMember.clear();
