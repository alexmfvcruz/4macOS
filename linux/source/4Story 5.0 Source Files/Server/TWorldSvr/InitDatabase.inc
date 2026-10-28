// Original InitDB function body, shared by Windows and native boundaries.
	if(!m_db.Open( m_szDSN, m_szDBUserID, m_szDBPasswd))
		return EC_INITSERVICE_DBOPENFAILED;

	if(!InitQueryTWorldSvr(&m_db))
		return EC_INITSERVICE_PREPAREQUERY;

	return EC_NOERROR;
