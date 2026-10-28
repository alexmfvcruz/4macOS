// Original InitDB function body, shared by Windows and native boundaries.
	if(!m_db.Open( m_szGameDSN, m_szDBUserID, m_szGamePasswd))
		return EC_INITSERVICE_DBOPENFAILED;

	if(!InitQueryTMapSvr(&m_db))
		return EC_INITSERVICE_PREPAREQUERY;

	return EC_NOERROR;
