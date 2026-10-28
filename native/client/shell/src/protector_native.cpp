// Native stand-ins for CTModuleProtector and CTNICEventSink (C4).
// CTModuleProtector (TModuleProtector.cpp) watches the loaded Windows modules
// (PE images) for hack tools; there are no PE modules on macOS, so nothing is
// registered and nothing is watched. CTNICEventSink (TNICEventSink.cpp)
// subscribes to WMI network-adapter events; WMI does not exist on macOS, so
// InitTEVENT fails at the point where the original fails without the WMI
// locator (CoCreateInstance), and no event arrives.
#include <fourstory_engine_ui.h>

CTModuleProtector::CTModuleProtector() : m_hThread(NULL), m_dwVERSION(0), m_dwTICK(0), m_bRUN(FALSE)
{
	InitializeCriticalSection(&m_cs);
}
CTModuleProtector::~CTModuleProtector()
{
	ClearModule();
	DeleteCriticalSection(&m_cs);
}
void CTModuleProtector::ClearModule()
{
	m_vTMODULE.clear();
	m_vBLOCK.clear();
}

CTNICEventSink::CTNICEventSink() : m_pIWBEM(NULL), m_nRef(1) {}
CTNICEventSink::~CTNICEventSink() { ReleaseTEVENT(); }
HRESULT CTNICEventSink::QueryInterface(REFIID, LPVOID* ppv)
{
	if(ppv)
		*ppv = NULL;
	return E_NOINTERFACE;
}
ULONG CTNICEventSink::AddRef() { return ++m_nRef; }
ULONG CTNICEventSink::Release()
{
	if(!--m_nRef)
	{
		delete this;
		return 0;
	}
	return m_nRef;
}
HRESULT CTNICEventSink::Indicate(long, IWbemClassObject**) { return S_OK; }
HRESULT CTNICEventSink::SetStatus(long, HRESULT, BSTR, IWbemClassObject*) { return S_OK; }
void CTNICEventSink::OnMediaDisconnect(IWbemClassObject*) {}
void CTNICEventSink::ReleaseTEVENT() { m_pIWBEM = NULL; }
BYTE CTNICEventSink::InitTEVENT(CString&, int)
{
	ReleaseTEVENT();
	return FALSE;
}
BYTE CTNICEventSink::GetMACAddress(LPBYTE, CString&) { return FALSE; }
