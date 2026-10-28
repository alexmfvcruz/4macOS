#pragma once
#define SMART_LOCKCS(x)				CCSLock lock_object(x);
class CCSLock
{
public:
	CCSLock( CRITICAL_SECTION *pCS)
	{
		EnterCriticalSection(pCS);
		m_pCS = pCS;
	};

	virtual ~CCSLock()
	{
		LeaveCriticalSection(m_pCS);
	};

protected:
	CRITICAL_SECTION *m_pCS;
};
