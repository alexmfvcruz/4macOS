// Original Login startup cleanup body. Requires m_db and original query declarations.
	DEFINE_QUERY(&m_db, CSPClearLoginUser)
	query->Call();
	UNDEFINE_QUERY()
