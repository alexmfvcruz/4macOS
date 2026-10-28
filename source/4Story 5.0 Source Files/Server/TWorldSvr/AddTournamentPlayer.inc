	pPlayer->m_bEntryID = pEntry->m_bEntryID;
	pPlayer->m_dwChiefID = pChief->m_dwCharID;
	pPlayer->m_bSlotID = pChief->m_bSlotID;

	if(bStep == TNMTSTEP_1st)
		pEntry->m_map1st.insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
	else if(bStep == TNMTSTEP_NORMAL)
		pEntry->m_mapNormal.insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
	else if(bStep == TNMTSTEP_PARTY)
		pChief->m_mapParty.insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
	else if(bStep == TNMTSTEP_MATCH)
		pEntry->m_mapPlayer.insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
	else
		return;

	m_mapTNMTPlayer.insert(MAPTNMTPLAYER::value_type(pPlayer->m_dwCharID, pPlayer));
