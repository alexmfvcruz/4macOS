	if(!bTotal)
		return;

	BYTE bOrder[TOURNAMENT_SLOT] = { 0, 6, 4, 2, 3, 5, 7, 1 };
	LPTNMTPLAYER pTP[TOURNAMENT_SLOT] = { NULL };

	BYTE bSeed = 0;
	MAPTNMTPLAYER::iterator it;

	for(BYTE bSeed=0; bSeed<TOURNAMENT_SLOT; bSeed++)
	{
		LPTNMTPLAYER pChar = NULL;
		LPTNMTPLAYER pCountryChar = NULL;
		BYTE bSlot = bOrder[bSeed];

		for(BYTE bCC=TCONTRY_D; bCC<TCONTRY_COUNT; bCC++)
		{
			for(it=map1st[bCC].begin(); it!=map1st[bCC].end(); it++)
			{
				if(!pChar || !pChar->m_dwMonthRank || (it->second->m_dwMonthRank && pChar->m_dwMonthRank > it->second->m_dwMonthRank))
					pChar = it->second;

				if((bSlot % 2) &&
					pTP[bSlot-1] &&
					pTP[bSlot-1]->m_bCountry != it->second->m_bCountry &&
					(!pCountryChar ||
					 !pCountryChar->m_dwMonthRank ||
					 (it->second->m_dwMonthRank && pCountryChar->m_dwMonthRank > it->second->m_dwMonthRank)))
				{
					pCountryChar = it->second;
				}
			}
		}

		if(pCountryChar)
			pChar = pCountryChar;

		if(pChar)
		{
			pChar->m_bSlotID = bSlot;
			AddTNMTPlayer(pEntry, pChar, TNMTSTEP_MATCH, pChar);
			map1st[pChar->m_bCountry].erase(pChar->m_dwCharID);
			pTP[bSlot] = pChar;
			pChar = NULL;
			continue;
		}

		for(BYTE bCC=TCONTRY_D; bCC<TCONTRY_COUNT; bCC++)
		{
			for(it=mapNormal[bCC].begin(); it!=mapNormal[bCC].end(); it++)
			{
				if(!pChar || !pChar->m_dwMonthRank || (it->second->m_dwMonthRank && pChar->m_dwMonthRank > it->second->m_dwMonthRank))
					pChar = it->second;

				if((bSlot % 2) &&
					pTP[bSlot-1] &&
					pTP[bSlot-1]->m_bCountry != it->second->m_bCountry &&
					(!pCountryChar ||
					 !pCountryChar->m_dwMonthRank ||
					 (it->second->m_dwMonthRank && pCountryChar->m_dwMonthRank > it->second->m_dwMonthRank)))
				{
					pCountryChar = it->second;
				}
			}
		}

		if(pCountryChar)
			pChar = pCountryChar;

		if(pChar)
		{
			pChar->m_bSlotID = bSlot;
			AddTNMTPlayer(pEntry, pChar, TNMTSTEP_MATCH, pChar);
			mapNormal[pChar->m_bCountry].erase(pChar->m_dwCharID);
			pTP[bSlot] = pChar;
			pChar = NULL;
		}
	}
