	if(!pTour || !battletime.m_bWeek || pTour->empty())
		return 0;

#ifdef FOURSTORY_NATIVE_SERVER
	CTime cur(current_time);
#else
	CTime cur = CTime::GetCurrentTime().GetTime();
#endif

	CTime start(0);
	BYTE bDayCount = 0;
	INT64 dStart = 0;

	WORD wYear = cur.GetYear();

	for(BYTE m=bMonthBase; m<=1; m++)
	{
		BYTE bMonth = cur.GetMonth() + m;
		if(bMonth > 12)
		{
			wYear = cur.GetYear() + 1;
			bMonth = 1;
		}

		BYTE bWeek = 0;

		for(BYTE i=1; i<=battletime.m_bWeek*7;)
		{
			if(i>31)
				break;

			CTime set(wYear, bMonth, i, 0, 0, 0);
			if(set.GetDayOfWeek() == battletime.m_bDay)
			{
				bWeek++;
				if(bWeek == battletime.m_bWeek)
				{
					start = set;
					break;
				}
				else
					i+=7;
			}
			else
				i++;
		}

		if(start.GetTime())
		{
			dStart = start.GetTime() + battletime.m_dwBattleStart;
			MAPTOURNAMENTSTEP::iterator it;
			for(it=pTour->begin(); it != pTour->end(); it++)
			{
				LPTOURNAMENTSTEP pSc = &((*it).second);

				pSc->m_dStart = dStart;
				pSc->m_dEnd = dStart + pSc->m_dwPeriod;
				dStart = pSc->m_dEnd;
			}

			if(dStart > cur.GetTime())
				break;
		}
	}

	if(dStart <= cur.GetTime())
	{
		MAPTOURNAMENTSCHEDULE::iterator itTour;
		for(itTour=m_mapTournamentSchedule.begin(); itTour!=m_mapTournamentSchedule.end(); itTour++)
			if((*itTour).second.m_mapStep == pTour)
			{
				LPPACKETBUF pMSG = new PACKETBUF();
				pMSG->m_packet.SetID(SM_TOURNAMENTEVENT_ACK)
					<< DWORD(0)
					<< BYTE(TET_SCHEDULEDEL)
					<< WORD((*itTour).first);

				SayToBATCH(pMSG);

				DelTournamentSchedule((*itTour).first);
				break;
			}

		return 0;
	}

	MAPTOURNAMENTSCHEDULE::iterator itTour;
	INT64 dNS = pTour->begin()->second.m_dStart;
	INT64 dNE = pTour->rbegin()->second.m_dEnd;

	for(itTour=m_mapTournamentSchedule.begin(); itTour!=m_mapTournamentSchedule.end(); itTour++)
	{
		LPMAPTOURNAMENTSTEP pTM = (*itTour).second.m_mapStep;
		if(pTM->empty() || (*itTour).first == wTourID)
			continue;

		INT64 dCS = pTM->begin()->second.m_dStart;
		INT64 dCE = pTM->rbegin()->second.m_dEnd;
		if((dNS >= dCS && dNS <= dCE) ||
			(dNE >= dCS && dNE <= dCE))
		{
			if(wTourID == 1)
			{
				LPPACKETBUF pMSG = new PACKETBUF();
				pMSG->m_packet.SetID(SM_TOURNAMENTEVENT_ACK)
					<< DWORD(0)
					<< BYTE(TET_SCHEDULEDEL)
					<< WORD((*itTour).first);

				SayToBATCH(pMSG);

				DelTournamentSchedule((*itTour).first);
			}
			return 0;
		}
	}

	TOURNAMENTSCHEDULE tosc;
	tosc.m_bEnable = bEnable;
	tosc.m_mapStep = pTour;

	if(!wTourID)
	{
		tosc.m_wID = ++m_wTournamentID;
		m_mapTournamentSchedule.insert(MAPTOURNAMENTSCHEDULE::value_type(tosc.m_wID, tosc));
		m_mapTournamentTime.insert(MAPTOURNAMENTTIME::value_type(tosc.m_wID, battletime));
		wTourID = tosc.m_wID;
	}
	else
	{
		MAPTOURNAMENTSCHEDULE::iterator itTs = m_mapTournamentSchedule.find(wTourID);
		if(itTs == m_mapTournamentSchedule.end())
		{
			tosc.m_wID = wTourID;
			m_mapTournamentSchedule.insert(MAPTOURNAMENTSCHEDULE::value_type(tosc.m_wID, tosc));
			m_mapTournamentTime.insert(MAPTOURNAMENTTIME::value_type(tosc.m_wID, battletime));

			if(m_wTournamentID < wTourID)
				m_wTournamentID = wTourID;
		}
	}

	if(wTourID == wTnmtID && bTnmtStep != TNMTSTEP_READY)
	{
		BYTE bEdit = FALSE;

		if(bTnmtStep >= TNMTSTEP_FENTER)
		{
			bTnmtStep = TNMTSTEP_FENTER;
			bEdit = TRUE;
		}
		else if(bTnmtStep >= TNMTSTEP_SFENTER)
		{
			bTnmtStep = TNMTSTEP_SFENTER;
			bEdit = TRUE;
		}
		else if(bTnmtStep == TNMTSTEP_ENTER)
		{
			bTnmtStep = TNMTSTEP_ENTER;
			bEdit = TRUE;
		}

		if(bEdit)
			dStart = cur.GetTime();

		m_tournament.m_wID = m_wTournamentID;
		m_tournament.m_bGroup = bTnmtGroup;
		m_tournament.m_bStep = bTnmtStep;

		m_TournamentSchedule.m_wID = m_wTournamentID;
		m_TournamentSchedule.m_mapStep = pTour;

		MAPTOURNAMENTSTEP::iterator it;
		for(it=pTour->begin(); it != pTour->end(); it++)
		{
			LPTOURNAMENTSTEP pSc = &((*it).second);
			WORD wAddTime = 0;

			if(bEdit)
			{
				if(MAKEWORD(pSc->m_bStep, pSc->m_bGroup) >= MAKEWORD(bTnmtStep, bTnmtGroup) &&
					pSc->m_dwPeriod)
				{
					if(pSc->m_bGroup == bTnmtGroup && pSc->m_bStep == bTnmtStep)
						wAddTime = 60*10;

					pSc->m_dStart = dStart;
					pSc->m_dEnd = dStart + pSc->m_dwPeriod + wAddTime;
					dStart = pSc->m_dEnd;
				}
				else
				{
					pSc->m_dStart = 0;
					pSc->m_dEnd = 0;
				}
			}

			TOURNAMENTSTEP step;
			step.m_bGroup = pSc->m_bGroup;
			step.m_bStep = pSc->m_bStep;
			step.m_dwPeriod = pSc->m_dwPeriod + wAddTime;
			step.m_dStart = pSc->m_dStart;
			m_tournament.m_mapStep.insert(MAPTOURNAMENTSTEP::value_type(MAKEWORD(step.m_bStep, step.m_bGroup), step));
		}
	}

	return wTourID;
