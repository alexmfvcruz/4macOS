	DEFINE_QUERY( &m_db, CTBLQuestChart)
	query->m_dwParentID = pQUEST->m_dwQuestID;

#ifdef FOURSTORY_NATIVE_ODBC
	if(native_query_open(*query))
#else
	if(query->Open())
#endif
	{
#ifdef FOURSTORY_NATIVE_ODBC
		while(native_query_fetch(*query))
#else
		while(query->Fetch())
#endif
		{
#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
			if(state.m_mapQUESTTEMP.count(query->m_dwQuestID)) throw std::runtime_error("Duplicate or cyclic Map quest row");
#elif defined(FOURSTORY_NATIVE_MODULE)
			if(m_mapQUESTTEMP.count(query->m_dwQuestID)) throw std::runtime_error("Duplicate or cyclic Map quest row");
#endif
			LPQUESTTEMP pCHILD = new QUESTTEMP();

			pCHILD->m_dwTriggerID	= query->m_dwTriggerID;
			pCHILD->m_dwParentID	= query->m_dwParentID;
			pCHILD->m_dwQuestID		= query->m_dwQuestID;

			pCHILD->m_bTriggerType	= query->m_bTriggerType;
			pCHILD->m_bForceRun		= query->m_bForceRun;
			pCHILD->m_bCountMax		= query->m_bCountMax;
			pCHILD->m_bType			= query->m_bType;
			pCHILD->m_bConditionCheck = query->m_bConditionCheck;

			pQUEST->m_mapCHILD.insert( MAPQUESTTEMP::value_type( pCHILD->m_dwQuestID, pCHILD));
			AddQuestTemp(pCHILD);
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLQuestConditionChart)
	query->m_dwQuestID = pQUEST->m_dwQuestID;

#ifdef FOURSTORY_NATIVE_ODBC
	if(native_query_open(*query))
#else
	if(query->Open())
#endif
	{
#ifdef FOURSTORY_NATIVE_ODBC
		while(native_query_fetch(*query))
#else
		while(query->Fetch())
#endif
		{
			LPQUESTCONDITION pCONDITION = new QUESTCONDITION();

			pCONDITION->m_dwConditionID	= query->m_dwConditionID;
			pCONDITION->m_bConditionType= query->m_bConditionType;
			pCONDITION->m_bCount		= query->m_bCount;

			pQUEST->m_vCondition.push_back(pCONDITION);
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	DEFINE_QUERY( &m_db, CTBLQuestRewardChart)
	query->m_dwQuestID = pQUEST->m_dwQuestID;

#ifdef FOURSTORY_NATIVE_ODBC
	if(native_query_open(*query))
#else
	if(query->Open())
#endif
	{
#ifdef FOURSTORY_NATIVE_ODBC
		while(native_query_fetch(*query))
#else
		while(query->Fetch())
#endif
		{
			LPQUESTREWARD pREWARD = new QUESTREWARD();

			pREWARD->m_dwRewardID = query->m_dwRewardID;
			pREWARD->m_bRewardType = query->m_bRewardType;
			pREWARD->m_bTakeMethod = query->m_bTakeMethod;
			pREWARD->m_bTakeData = query->m_bTakeData;
			pREWARD->m_bCount = query->m_bCount;

			pQUEST->m_vReward.push_back(pREWARD);
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	for(DWORD i=0; i<pQUEST->m_vReward.size(); i++)
	{
		LPQUESTREWARD pREWARD = pQUEST->m_vReward[i];

		if(pREWARD->m_bRewardType == RT_MAGICITEM)
		{
			MAPTSTORAGEITEM::iterator itQI = m_mapTQuestItem.find(pREWARD->m_dwRewardID);
			if(itQI != m_mapTQuestItem.end())
				pREWARD->m_pMagicItem = (*itQI).second;
		}
	}

	DEFINE_QUERY( &m_db, CTBLQuestTermChart)
	query->m_dwQuestID = pQUEST->m_dwQuestID;

#ifdef FOURSTORY_NATIVE_ODBC
	if(native_query_open(*query))
#else
	if(query->Open())
#endif
	{
#ifdef FOURSTORY_NATIVE_ODBC
		while(native_query_fetch(*query))
#else
		while(query->Fetch())
#endif
		{
			LPQUESTTERM pTERM = new QUESTTERM();

			pTERM->m_dwTermID = query->m_dwTermID;
			pTERM->m_bTermType = query->m_bTermType;
			pTERM->m_bCount = query->m_bCount;

			pQUEST->m_vTerm.push_back(pTERM);

			if(pQUEST->m_bType == QT_REGEN && pQUEST->m_bTriggerType == TT_KILLMON && pTERM->m_bTermType == QTT_MONID)
			{
				LPTMONSTER pMon = FindTMonster(WORD(pQUEST->m_dwTriggerID));
				if(pMon)
					pMon->m_bCheckPartyQuest = TRUE;
			}
		}

		query->Close();
	}
	UNDEFINE_QUERY()
/*
	DEFINE_QUERY( &m_db, CTBLCustomTicksTable )
	if(query->Open())
	{
		BYTE bIndex = 0;
		while(query->Fetch())
		{
			LPTCUSTOMTICKS pTicks = new TCUSTOMTICKS();
			pTicks->m_wFirstSkill = query->m_wSkill;
			pTicks->m_wSkill = query->m_wAffectedSkill;
			pTicks->m_dwTick = query->m_dwTick;

			m_mapCUSTOMTICKS.insert(MAPTCUSTOMTICKS::value_type(++bIndex, pTicks));
		}
	}
	UNDEFINE_QUERY()
	*/
	MAPQUESTTEMP::iterator it;

	for( it = pQUEST->m_mapCHILD.begin(); it != pQUEST->m_mapCHILD.end(); it++)
		LoadQuestTemp((*it).second);
