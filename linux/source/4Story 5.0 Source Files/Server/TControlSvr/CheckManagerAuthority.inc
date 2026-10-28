	
	if( m_bAuthority <= m_bClass)
		return TRUE; 
	else if( m_bAuthority <= MANAGER_SERVICE )
	{		
		SendCT_AUTHORITY_ACK();
	}

	return FALSE;
