	m_bStatus = BS_NORMAL;
	m_dwStart = 0;
	m_dwAlarmDur = dwAlarmDur;
	m_dwBuyTimeDur = dwBuyTimeDur;
	m_dwBattleDur = dwBattleDur;
	m_bMinPlayerCount = bMinPlayerCount;
	m_dwTick = 0;
	m_bType = BR_TEAM;
	m_bRunning = FALSE;
	m_wMapID = DEFAULT_BR_MAP;
	m_dwPeaceTick = 0;

	m_pBRServer = NULL;
	m_vTIMES.clear();
	m_vBattleType.clear();

	AddMap(706, "Hod");
	AddMap(710, "Blonea");
	AddMap(703, "Colossus");
	AddMap(708, "Tyconteroga");

	m_Mode = BR_3V3;

	Release();
