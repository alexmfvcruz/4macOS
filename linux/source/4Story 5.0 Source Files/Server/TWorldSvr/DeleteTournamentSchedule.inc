	m_mapTournamentTime.erase(wTourID);
	MAPTOURNAMENTSCHEDULE::iterator it = m_mapTournamentSchedule.find(wTourID);
	if(it!=m_mapTournamentSchedule.end())
		delete (*it).second.m_mapStep;

	m_mapTournamentSchedule.erase(wTourID);

	TournamentUpdate(wTourID);

	SendDM_TNMTEVENTSCHEDULEDEL_REQ(wTourID);
