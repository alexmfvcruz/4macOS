#ifdef FOURSTORY_NATIVE_MODULE
	{ fourstory::server::quest_bulk::Scope questCache(&m_db);
#endif
	DEFINE_QUERY( &m_db, CTBLQuestChart)
	query->m_dwParentID = 0;

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
			LPQUESTTEMP pQUEST = new QUESTTEMP();

			pQUEST->m_dwTriggerID	= query->m_dwTriggerID;
			pQUEST->m_dwParentID	= query->m_dwParentID;
			pQUEST->m_dwQuestID		= query->m_dwQuestID;

			pQUEST->m_bTriggerType	= query->m_bTriggerType;
			pQUEST->m_bForceRun		= query->m_bForceRun;
			pQUEST->m_bCountMax		= query->m_bCountMax;
			pQUEST->m_bType			= query->m_bType;
			pQUEST->m_bConditionCheck = query->m_bConditionCheck;

			m_vQUESTTEMP.push_back(pQUEST);
			AddQuestTemp(pQUEST);
		}

		query->Close();
	}
	UNDEFINE_QUERY()

	for( int i=0; i<INT(m_vQUESTTEMP.size()); i++)
		LoadQuestTemp(m_vQUESTTEMP[i]);
#ifdef FOURSTORY_NATIVE_MODULE
	}
#endif
