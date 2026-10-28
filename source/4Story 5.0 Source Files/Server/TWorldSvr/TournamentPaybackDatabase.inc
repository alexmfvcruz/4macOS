	DWORD dwCharID;
	DWORD dwBack;
	DWORD dwPostID = 0;

	pBUF->m_packet
		>> dwCharID
		>> dwBack;

	DWORD dwGold, dwSilver, dwCooper;
	CalcMoney(dwBack, dwGold, dwSilver, dwCooper);

	DEFINE_QUERY(&m_db, CSPTournamentPayback)
	query->m_dwCharID = dwCharID;
	query->m_dwGold = dwGold;
	query->m_dwSilver = dwSilver;
	query->m_dwCooper = dwCooper;
#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
	native_query_succeeded = query->Call();
	if(native_query_succeeded)
#else
	if(query->Call())
#endif
		dwPostID = query->m_dwPostID;
	UNDEFINE_QUERY()

	if(dwPostID)
	{
		LPPACKETBUF  pMsg = new PACKETBUF();
		pMsg->m_packet.SetID(DM_TOURNAMENTPAYBACK_ACK)
			<< dwPostID
			<< dwCharID
			<< dwGold
			<< dwSilver
			<< dwCooper;

		SayToBATCH(pMsg);
	}

	return EC_NOERROR;
