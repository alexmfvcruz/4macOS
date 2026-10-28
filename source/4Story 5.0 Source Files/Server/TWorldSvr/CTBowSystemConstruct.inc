	m_wMapID = wMapID;
	m_bMinPlayersCount = bMinPlayersCount;
	m_bMaxNationDifference = bMaxNationDifference;
	m_bStatus = BS_PEACE;
	m_bRunning = FALSE;
	m_dwStart = 0;
	m_dwTick = 0;
	m_dwPeaceTick = 0;
	m_pBOWServer = NULL;
	m_bWinner = TCONTRY_N;
	m_vTIMES.clear();

	Release();
