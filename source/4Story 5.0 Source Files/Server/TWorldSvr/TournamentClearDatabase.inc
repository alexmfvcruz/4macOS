	DEFINE_QUERY(&m_db, CSPTournamentClear)
	query->m_bClear = TRUE;
#if defined(FOURSTORY_NATIVE_ODBC) && !defined(FOURSTORY_NATIVE_MODULE)
	native_query_succeeded = query->Call();
#else
	query->Call();
#endif
	UNDEFINE_QUERY()

	return EC_NOERROR;
