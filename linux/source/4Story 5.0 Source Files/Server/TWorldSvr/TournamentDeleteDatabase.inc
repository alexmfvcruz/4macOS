	WORD wTID;
	pBUF->m_packet
		>> wTID;

	DEFINE_QUERY(&m_db, CSPTnmtEventDel)
	query->m_wTourID = wTID;
#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
	native_query_succeeded = query->Call();
#else
	query->Call();
#endif
	UNDEFINE_QUERY()

	return EC_NOERROR;
