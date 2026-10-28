	m_mapQUESTTEMP.insert( MAPQUESTTEMP::value_type( pQUEST->m_dwQuestID, pQUEST));

	MAPMAPVQUESTTEMP::iterator itTYPE = m_mapTRIGGER.find(pQUEST->m_bTriggerType);
	LPMAPVQUESTTEMP pMAPVQUEST = NULL;

	if( itTYPE == m_mapTRIGGER.end() )
	{
		pMAPVQUEST = new MAPVQUESTTEMP();
		pMAPVQUEST->clear();

		m_mapTRIGGER.insert( MAPMAPVQUESTTEMP::value_type( pQUEST->m_bTriggerType, pMAPVQUEST));
	}
	else
		pMAPVQUEST = (*itTYPE).second;

	MAPVQUESTTEMP::iterator itTRIGGER = pMAPVQUEST->find(pQUEST->m_dwTriggerID);
	LPVQUESTTEMP pVQUEST = NULL;

	if( itTRIGGER == pMAPVQUEST->end() )
	{
		pVQUEST = new VQUESTTEMP();
		pVQUEST->clear();

		pMAPVQUEST->insert( MAPVQUESTTEMP::value_type( pQUEST->m_dwTriggerID, pVQUEST));
	}
	else
		pVQUEST = (*itTRIGGER).second;
	pVQUEST->push_back(pQUEST);
