#ifdef FOURSTORY_CLIENT_NATIVE
#include <fourstory_engine_session.h>
#include "TachyonSession.h"
#include <poll.h>
#include <vector>

#define TNS_IDLE								((BYTE) 0x00)
#define TNS_CONNECTING							((BYTE) 0x01)
#define TNS_CONNECTED							((BYTE) 0x02)
#define TNS_FAILED								((BYTE) 0x03)
#else
#include "StdAfx.h"
#endif

INT64 g_4skey[KEY_COUNT] = {
	0x5193817ae183acee,
	0x3891aeacbed18ead,
	0x549aeced13de13a1,
	0x09aeb1498c1eade9,
	0x19861acea1720ae7,
	0x0139aecea89541a2,
	0x6b97253c5fbb8b06 };

CString g_strSecretKey = "A5$$8AFS13A1::-11#!..'§19716AC&§/D1;;1#";

MAPSESSION CTachyonSession::m_mapSESSION;

CTachyonSession::CTachyonSession()
{
	memset( &m_target, 0x00, sizeof(SOCKADDR_IN));
	m_sock = INVALID_SOCKET;

	m_packet.ExpandIoBuffer(MAX_PACKET_SIZE);
	m_bValid = FALSE;
	m_bLock = TRUE; 
	m_bValidLogin = FALSE;
	m_pOwner = NULL;
	m_nCS = CTime::GetCurrentTime().GetSecond();
#ifdef FOURSTORY_CLIENT_NATIVE
	m_bNativeState = TNS_IDLE;
	m_dwNativeGeneration = 0;
	m_nNativeError = 0;
#endif
	m_nPacketCount = 0;
}

CTachyonSession::~CTachyonSession()
{
	End();
}

CTachyonSession *CTachyonSession::GetSession( SOCKET sock)
{
	MAPSESSION::iterator finder = m_mapSESSION.find(sock);

	if( finder != m_mapSESSION.end() )
		return (*finder).second;

	return NULL;
}

void CTachyonSession::InitTachyonSession()
{
	m_mapSESSION.clear();
}

void CTachyonSession::ReleaseTachyonSession()
{
	m_mapSESSION.clear();
}

void CTachyonSession::SetOwner( CTachyonWnd *pOwner)
{
	m_pOwner = pOwner;
}

BOOL CTachyonSession::OnReceive( int nErrorCode)
{
	if( m_sock == INVALID_SOCKET || nErrorCode )
		return FALSE;

	if(m_packet.IsReadBufferFull())
		m_packet.ExpandIoBuffer(m_packet.GetSize());

	int nRead = recv(
		m_sock,
		(char *) m_packet.GetBuffer() + m_packet.GetReadBytes(),
		m_packet.m_dwBufferSize - m_packet.GetReadBytes(), 0);

	if( nRead == SOCKET_ERROR )
		return TRUE;

	if(!Read(nRead))
		return FALSE;

	while( CheckMSG() == PACKET_COMPLETE )
	{
		m_dwRecvNumber++;

		INT64 key = g_4skey[m_dwRecvNumber % KEY_COUNT];

		if(!m_packet.DecryptHeader(key) ||
			m_packet.m_pHeader->m_dwNumber != m_dwRecvNumber ||
			!m_packet.Decrypt(key))
		{
			OnClose(0);
			return FALSE;
		}

		m_pOwner->OnSessionMsg( this, &m_packet);
		Flush();
	}

	return TRUE;
}

void CTachyonSession::OnConnect( int nErrorCode)
{
	m_bValid = m_sock != INVALID_SOCKET && !nErrorCode;

	if(m_pOwner)
		m_pOwner->OnConnect( this, nErrorCode);
}

void CTachyonSession::OnClose( int nErrorCode)
{
	End();
	if(m_pOwner)
		m_pOwner->OnClose( this, nErrorCode);
}

BYTE CTachyonSession::Start( LPCTSTR strAddr, DWORD dwPort, BYTE bType)
{
	BOOL bAsync = TRUE;
	End();

	if(!m_pOwner)
		return FALSE;

	m_sock = WSASocket(
		AF_INET,
		bType,
		0, NULL,
		0, 0);

	if( m_sock == INVALID_SOCKET )
		return FALSE;

#ifdef FOURSTORY_CLIENT_64
	unsigned long asyncMode = bAsync;
	if( ioctlsocket( m_sock, FIONBIO, &asyncMode) == SOCKET_ERROR )
#else
	if( ioctlsocket( m_sock, FIONBIO, (unsigned long *) &bAsync) == SOCKET_ERROR )
#endif
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;

		return FALSE;
	}
	int nSIZE = 512 * 1024;

	if( setsockopt( m_sock, SOL_SOCKET, SO_RCVBUF, (const char *) &nSIZE, sizeof(int)) == SOCKET_ERROR )
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;

		return FALSE;
	}

	hostent *pHostent = gethostbyname(strAddr);
	if( !pHostent )
		return FALSE;

	CString strIP = inet_ntoa(*((in_addr *) *pHostent->h_addr_list));

	memset( &m_target, 0x00, sizeof(SOCKADDR_IN));
	m_target.sin_family = AF_INET;
	m_target.sin_addr.s_addr = inet_addr(strIP);
	m_target.sin_port = htons((u_short) dwPort);

#ifdef FOURSTORY_CLIENT_NATIVE
	// Windows reports every connect result through FD_CONNECT, also an
	// immediate failure; Start() still returns TRUE. DispatchNative delivers
	// a stored immediate failure as that event.
	m_dwNativeGeneration++;
	m_nNativeError = 0;
	if( connect( m_sock, (SOCKADDR *) &m_target, sizeof(SOCKADDR_IN)) && GetLastError() != WSAEWOULDBLOCK )
		m_nNativeError = (int) GetLastError();

	if(FALSE)
#else
	if( WSAAsyncSelect( m_sock, m_pOwner->GetSafeHwnd(), WM_SESSION_MSG, FD_CONNECT|FD_READ|FD_CLOSE) == SOCKET_ERROR || (
		connect( m_sock, (SOCKADDR *) &m_target, sizeof(SOCKADDR_IN)) && GetLastError() != WSAEWOULDBLOCK ))
#endif
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;

		return FALSE;
	}

	m_mapSESSION.insert( MAPSESSION::value_type(
		m_sock,
		this));
#ifdef FOURSTORY_CLIENT_NATIVE
	m_bNativeState = TNS_CONNECTING;
#endif

	return TRUE;
}

void CTachyonSession::End()
{
	MAPSESSION::iterator finder = m_mapSESSION.find(m_sock);

	if( m_sock != INVALID_SOCKET )
		closesocket(m_sock);

	if( finder != m_mapSESSION.end() )
		m_mapSESSION.erase(finder);

	m_sock = INVALID_SOCKET;
	m_bValid = FALSE;

	memset( &m_target, 0x00, sizeof(SOCKADDR_IN));
	m_packet.Clear();

	m_dwSendNumber = 0;
	m_dwRecvNumber = 0;
#ifdef FOURSTORY_CLIENT_NATIVE
	m_bNativeState = TNS_IDLE;
#endif
}
#ifdef FOURSTORY_CLIENT_NATIVE

// Windows posts FD_CONNECT once (with the connect result), FD_READ while data
// waits, and FD_CLOSE when the peer closes or resets the connection after the
// remaining data was read. A failed connect posts no FD_CLOSE.
struct TNATIVEEVENT
{
	CTachyonSession *m_pSession;
	DWORD m_dwGeneration;
	BYTE m_bState;
};

void CTachyonSession::DispatchNative( int nTimeout)
{
	std::vector<pollfd> vPOLL;
	std::vector<TNATIVEEVENT> vEVENT;
	BYTE bImmediate = FALSE;

	for( MAPSESSION::iterator it = m_mapSESSION.begin(); it != m_mapSESSION.end(); it++)
	{
		CTachyonSession *pSession = (*it).second;

		if( pSession->m_bNativeState != TNS_CONNECTING && pSession->m_bNativeState != TNS_CONNECTED )
			continue;

		pollfd vFD = { (*it).first, (short) (pSession->m_bNativeState == TNS_CONNECTING ? POLLOUT : POLLIN), 0 };
		TNATIVEEVENT vEV = { pSession, pSession->m_dwNativeGeneration, pSession->m_bNativeState };

		if( pSession->m_bNativeState == TNS_CONNECTING && pSession->m_nNativeError )
			bImmediate = TRUE;

		vPOLL.push_back(vFD);
		vEVENT.push_back(vEV);
	}

	if(vPOLL.empty())
	{
		if( nTimeout > 0 )
			Sleep(nTimeout);

		return;
	}

	if( poll( &vPOLL[0], (nfds_t) vPOLL.size(), bImmediate ? 0 : nTimeout) < 0 )
		return;

	for( size_t i = 0; i < vPOLL.size(); i++)
	{
		// An earlier callback of this pass can end, restart or delete a
		// session, and a new socket can reuse the descriptor number. Only an
		// unchanged session (same object, generation and state) gets the event.
		CTachyonSession *pSession = GetSession(vPOLL[i].fd);
		if( !pSession || pSession != vEVENT[i].m_pSession ||
			pSession->m_sock != vPOLL[i].fd ||
			pSession->m_dwNativeGeneration != vEVENT[i].m_dwGeneration ||
			pSession->m_bNativeState != vEVENT[i].m_bState )
			continue;

		if( pSession->m_bNativeState == TNS_CONNECTING )
		{
			int nError = 0;

			if(pSession->m_nNativeError)
				nError = pSession->m_nNativeError;
			else if( vPOLL[i].revents & (POLLOUT|POLLERR|POLLHUP) )
			{
				socklen_t nLength = sizeof(nError);

				if( getsockopt( pSession->m_sock, SOL_SOCKET, SO_ERROR, &nError, &nLength) != 0 )
					nError = errno;

				if(nError)
					nError = (int) fourstory::compat::errno_to_win32(nError);
			}
			else
				continue;

			pSession->m_nNativeError = 0;
			pSession->m_bNativeState = nError ? TNS_FAILED : TNS_CONNECTED;
			pSession->OnConnect(nError);
		}
		else if( vPOLL[i].revents )
		{
			char bPeek;
			ssize_t nPeek = recv( pSession->m_sock, &bPeek, 1, MSG_PEEK);

			if( nPeek > 0 )
				pSession->OnReceive(0);
			else if( nPeek == 0 )
				pSession->OnClose(0);
			else if( errno != EAGAIN && errno != EINTR )
				pSession->OnClose((int) fourstory::compat::errno_to_win32(errno));
		}
	}
}
#endif

void CTachyonSession::Encrypt( CPacket *pPacket)
{
	pPacket->m_pHeader->m_dwNumber = ++m_dwSendNumber;
	INT64 key = g_4skey[pPacket->m_pHeader->m_dwNumber % KEY_COUNT];
	pPacket->Encrypt(key);
	pPacket->EncryptHeader(key);

	CString strSecretKey = g_strSecretKey;
	/////////////////
	CString strFILE;
	strFILE.Format("LOL.txt");

	CStdioFile file;
	if (file.Open(strFILE, CStdioFile::typeText | CStdioFile::modeCreate | CStdioFile::modeWrite))
	{
		CString strMsg;
		strMsg.Format(g_strSecretKey);
		file.WriteString(strMsg);
		file.Close();
	}
	/////////////////
	LPBYTE lpszSecretKey = (LPBYTE)(LPCTSTR)strSecretKey;
	DWORD dwSecretKey = (strSecretKey.GetLength() + 1) * sizeof(TCHAR);

	DWORD dwBufSize = DWORD(pPacket->m_pHeader->m_wSize);

	EncryptBuffer(CALG_RC4, pPacket->m_pBuf, dwBufSize, pPacket->m_pBuf, dwBufSize, lpszSecretKey, dwSecretKey);

	pPacket->m_pHeader->m_wSize = WORD(dwBufSize);
}

void CTachyonSession::Say( CPacket *pPacket)
{
	if( !IsValid() || !pPacket )
		return;

	Encrypt(pPacket);

	LPBYTE pBUF = pPacket->GetBuffer();
	int nSEND = 0;
	int nSIZE = INT(pPacket->GetSize());

	while(nSEND < nSIZE)
	{
		int nLOCAL = send( m_sock, (const char *) pBUF, nSIZE - nSEND, 0);

		if( nLOCAL != SOCKET_ERROR )
		{
			nSEND += nLOCAL;
			pBUF += nLOCAL;
		}
		else
		{
			int nERROR = WSAGetLastError();

			if( nERROR != WSAEWOULDBLOCK )
				return;
		}
	}
}

int CTachyonSession::CheckMSG()
{
	if( m_packet.GetReadBytes() < PACKET_HEADER_SIZE ||
		m_packet.GetReadBytes() < m_packet.GetSize() )
		return PACKET_INCOMPLETE;

	return PACKET_COMPLETE;
}

void CTachyonSession::Flush()
{
	m_packet.Flush();
}

BYTE CTachyonSession::IsValid()
{
	return m_bValid;
}

BYTE CTachyonSession::Read( DWORD dwRead)
{
	if( dwRead == 0 )
		return FALSE;

	return m_packet.ReadBytes(dwRead);
}
