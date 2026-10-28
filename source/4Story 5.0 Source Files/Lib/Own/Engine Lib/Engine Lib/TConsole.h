#pragma once

class CTConsole
{
private:
	HANDLE m_hConsole;

public:
	bool Create(LPCSTR szTitle, bool bNoClose = false);
	
	void Color(WORD wColor = NULL);
	void Output(LPCSTR szOutput = NULL, ...);
	
	void SetTitle(LPCSTR szTitle);
	char* GetTitle();

	HWND GetHWND();
	HANDLE GetHandle();

	void Show(bool bShow = true);
	void DisableClose();
	void Clear();

	void Close();

	bool IsValid() const { return m_hConsole? true: false; }

public:
	CTConsole();
	~CTConsole();

	static CTConsole* GetInstance();
};

#ifdef TRACE
#undef TRACE
#endif

#ifdef FOURSTORY_CLIENT_NATIVE
// MSVC passes a CString through "..." as its text pointer; the template
// converts the arguments as CTChart::Format does (fourstory::compat::printf_arg).
void TRACE_NATIVE(LPCSTR szOutput, ...);
inline void TRACE() { TRACE_NATIVE(NULL); }
template<class... Args>
inline void TRACE(LPCSTR szOutput, const Args&... args) { TRACE_NATIVE(szOutput, fourstory::compat::printf_arg(args)...); }
#else
void TRACE(LPCSTR szOutput = NULL, ...);
#endif
