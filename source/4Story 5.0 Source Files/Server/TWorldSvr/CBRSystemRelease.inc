	m_bRunning = FALSE;
	m_bStatus = BS_NORMAL;
	m_dwTick = 0;
	m_dwPeaceTick = 0;
	m_Mode = BR_3V3;

	for (MAPBRTEAMS::iterator itTeam = m_mapBRTeam.begin(); itTeam != m_mapBRTeam.end(); ++itTeam)
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*itTeam).second->m_mapBRPlayer.begin(); itPlayer != (*itTeam).second->m_mapBRPlayer.end(); ++itPlayer)
			delete (*itPlayer).second;
		(*itTeam).second->m_mapBRPlayer.clear();

		delete (*itTeam).second;
	}

	for (MAPBRTEAMS::iterator itPremade = m_mapPremadeTeam.begin(); itPremade != m_mapPremadeTeam.end(); ++itPremade)
	{
		for (MAPBRPLAYERS::iterator itPlayer = (*itPremade).second->m_mapBRPlayer.begin(); itPlayer != (*itPremade).second->m_mapBRPlayer.end(); ++itPlayer)
			delete (*itPlayer).second;
		(*itPremade).second->m_mapBRPlayer.clear();

		delete (*itPremade).second;
	}
	m_mapPremadeTeam.clear();

	for (MAPBRTEMPPLAYER::iterator itReg = m_mapBRREG.begin(); itReg != m_mapBRREG.end(); ++itReg)
		delete (*itReg).second;
	m_mapBRREG.clear();

	m_mapBRTeam.clear();
	m_mapMapVote.clear();
	m_mapVoteMode.clear();
