	if(bCountry > TCONTRY_B)
		return FALSE;

	for(BYTE i=1; i<m_bFirstGroupCount; i++)
	{
		if(m_arFirstGradeGroup[bCountry][i].m_dwCharID == dwCharID)
			return TRUE;
	}

	return FALSE;
