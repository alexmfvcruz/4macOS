	LPMAPTOURNAMENTENTRY pTour = GetTournament(m_tournament.m_wID);
	if(!pTour)
		return;

	m_tournament.m_bSelected = TRUE;

	MAPTNMTPLAYER::iterator itTNMTP;
	MAPTNMTPLAYER tp = m_mapTNMTPlayer;
	m_mapTNMTPlayer.clear();

	MAPTOURNAMENTENTRY::iterator it;
	for(it=pTour->begin(); it!=pTour->end(); it++)
	{
		LPTOURNAMENTENTRY pEntry = (*it).second;
		pEntry->m_mapPlayer.clear();

		MAPTNMTPLAYER::iterator itTP;
		BYTE bCountry[TCONTRY_COUNT] = {0};
		BYTE bSlot[TOURNAMENT_SLOT] = {0};

		MAPTNMTPLAYER mapCPL1[TCONTRY_COUNT];
		MAPTNMTPLAYER mapCPLN[TCONTRY_COUNT];
		MAPDWORD mapPL[TCONTRY_COUNT];
		for(BYTE cc=0; cc<TCONTRY_COUNT; cc++)
		{
			mapCPL1[cc].clear();
			mapCPLN[cc].clear();
			mapPL[cc].clear();
		}

		for(itTP=pEntry->m_map1st.begin(); itTP!=pEntry->m_map1st.end(); itTP++)
		{
			LPTNMTPLAYER pPlayer = (*itTP).second;
			bCountry[pPlayer->m_bCountry]++;
			mapCPL1[pPlayer->m_bCountry].insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
			tp.erase(pPlayer->m_dwCharID);
		}
		pEntry->m_map1st.clear();

		DWORD dwLevel[TCONTRY_COUNT] = {0};
		for(itTP=pEntry->m_mapNormal.begin(); itTP!=pEntry->m_mapNormal.end(); itTP++)
		{
			LPTNMTPLAYER pPlayer = (*itTP).second;
			dwLevel[pPlayer->m_bCountry] += pPlayer->m_bLevel;
			mapPL[pPlayer->m_bCountry].insert(MAPDWORD::value_type(dwLevel[pPlayer->m_bCountry], pPlayer->m_dwCharID));
		}
		pEntry->m_mapNormal.clear();

		BYTE bNC[TCONTRY_COUNT] = {0};
		BYTE bSum = BYTE(mapCPL1[TCONTRY_D].size() + mapCPL1[TCONTRY_C].size() + mapCPL1[TCONTRY_B].size());

		for(BYTE sm = 0; sm < TOURNAMENT_SLOT; sm++)
		{
			if(bSum >= TOURNAMENT_SLOT)
				break;

			if(bSum > sm)
				continue;

			BYTE bMinC = TCONTRY_COUNT;
			for(BYTE bM = TCONTRY_D; bM <= TCONTRY_B; bM++)
			{
				if(mapPL[bM].empty())
					continue;

				if(bMinC == TCONTRY_COUNT || mapCPL1[bM].size() + bNC[bM] < mapCPL1[bMinC].size() + bNC[bMinC])
					bMinC = bM;
				else if(mapCPL1[bM].size() + bNC[bM] == mapCPL1[bMinC].size() + bNC[bMinC])
					bMinC = rand() % 2 ? bM : bMinC;
			}

			if(bMinC == TCONTRY_COUNT)
				break;

			MAPDWORD mapCalc;
			DWORD dwCalc = 0;
			mapCalc.clear();

			DWORD dwRand = TRand(dwLevel[bMinC]);
			MAPDWORD::iterator itSt;
			BYTE bSelected = FALSE;

			for(itSt=mapPL[bMinC].begin(); itSt!=mapPL[bMinC].end(); itSt++)
			{
				itTNMTP = tp.find((*itSt).second);
				if(itTNMTP!=tp.end())
				{
					LPTNMTPLAYER pPlayer = (*itTNMTP).second;
					
					if((*itSt).first > dwRand && !bSelected)
					{
						mapCPLN[pPlayer->m_bCountry].insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
						tp.erase(pPlayer->m_dwCharID);
						bSelected = TRUE;
						continue;
					}
					else
					{
						dwCalc += pPlayer->m_bLevel;
						mapCalc.insert(MAPDWORD::value_type(dwCalc, pPlayer->m_dwCharID));
					}
				}
			}

			dwLevel[bMinC] = dwCalc;
			mapPL[bMinC] = mapCalc;

			bNC[bMinC]++;
			bSum++;
		}

		TNMTMatch(pEntry, mapCPL1, mapCPLN, bSum);
	}

	for(itTNMTP=tp.begin(); itTNMTP!=tp.end(); itTNMTP++)
	{
		LPTOURNAMENTENTRY pEntry = GetCurrentTournamentEntry((*itTNMTP).second->m_bEntryID);
		if(pEntry)
			SendDM_TOURNAMENTPAYBACK_REQ((*itTNMTP).second->m_dwCharID, pEntry->m_dwFeeBack);

		m_mapTNMTPlayer.erase((*itTNMTP).second->m_dwCharID);
		delete (*itTNMTP).second;
	}
	tp.clear();

	for(itTNMTP=m_mapTNMTPlayer.begin(); itTNMTP!=m_mapTNMTPlayer.end();)
	{
		if((*itTNMTP).second->m_bSlotID == TOURNAMENT_SLOT)
		{
			delete (*itTNMTP).second;
			itTNMTP = m_mapTNMTPlayer.erase(itTNMTP);
		}
		else
			 itTNMTP++;
	}
