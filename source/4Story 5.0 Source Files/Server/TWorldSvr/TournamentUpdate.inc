	WORD wTournamentID;
	LPMAPTOURNAMENTSTEP pTournament = NULL;
	MAPTOURNAMENTSCHEDULE::iterator itTS;
	MAPTOURNAMENTSTEP::iterator itStart;

	for(itTS=m_mapTournamentSchedule.begin(); itTS!=m_mapTournamentSchedule.end(); itTS++)
	{
		LPMAPTOURNAMENTSTEP pMapStep = (*itTS).second.m_mapStep;
		itStart = pMapStep->begin();

		if(itStart != pMapStep->end())
		{
			if(wTourID == (*itTS).first)
				(*itTS).second.m_bEnable = TRUE;

			TOURNAMENTSTEP startTM = (*itStart).second;

			if(!pTournament || startTM.m_dStart < pTournament->begin()->second.m_dStart)
			{
				pTournament = pMapStep;
				wTournamentID = (*itTS).first;
			}
		}
	}

	if(!pTournament)
	{
		LPPACKETBUF pMSG = new PACKETBUF();
		pMSG->m_packet.SetID(SM_TOURNAMENTUPDATE_REQ)
			<< WORD(0)
			<< BYTE(0);

		m_TournamentSchedule.m_wID = 0;
		m_TournamentSchedule.m_mapStep = NULL;

		SayToBATCH(pMSG);
		SendDM_TOURNAMENTCLEAR_REQ();
		return;
	}

	if(wTourID == wTournamentID ||
		wTournamentID != m_tournament.m_wID)
	{
		m_TournamentSchedule.m_wID = wTournamentID;
		m_TournamentSchedule.m_mapStep = pTournament;

		LPPACKETBUF pMSG = new PACKETBUF();
		pMSG->m_packet.SetID(SM_TOURNAMENTUPDATE_REQ)
			<< wTournamentID
			<< BYTE(pTournament->size());

		for(itStart=pTournament->begin(); itStart!=pTournament->end(); itStart++)
		{
			pMSG->m_packet
				<< (*itStart).second.m_bGroup
				<< (*itStart).second.m_bStep
				<< (*itStart).second.m_dwPeriod
				<< (*itStart).second.m_dStart;
		}

		SayToBATCH(pMSG);

		SendDM_TOURNAMENTCLEAR_REQ();
	}
