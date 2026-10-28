// Original InitDB function body, shared by Windows and native boundaries.
	if(!pDB->Open( m_szDSN, m_szDBUserID, m_szDBPasswd))
		return EC_INITSERVICE_DBOPENFAILED;

	if(!InitQueryTLoginSvr(pDB))
		return EC_INITSERVICE_PREPAREQUERY;

	return EC_NOERROR;
