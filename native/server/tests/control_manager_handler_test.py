#!/usr/bin/env python3
"""Compile the actual native OPLOGIN handler with a reused fake SQL query.

No database, listener, credentials or running server are used. The fake Call
can deliberately leave its return output unchanged to exercise stale values.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'source/4Story 5.0 Source Files/Server/TControlSvr/Handler.cpp').read_bytes().decode('latin1')
start = source.index('DWORD CTControlSvrModule::OnCT_OPLOGIN_REQ(')
end = source.index('DWORD CTControlSvrModule::OnCT_STLOGIN_REQ(', start)
handler = source[start:end]
netcode = (root / 'source/4Story 5.0 Source Files/Lib/Own/TProtocol/include/NetCode.h').read_bytes().decode('latin1')
name_define = next(line for line in netcode.splitlines() if line.startswith('#define MAX_NAME\t'))
scaffold = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <arpa/inet.h>
using BYTE = unsigned char;
using DWORD = unsigned int;
using DWORD_PTR = std::uintptr_t;
constexpr DWORD EC_NOERROR = 0, EC_SESSION_INVALIDCHAR = 1;
constexpr int MANAGER_ALL = 1, MANAGER_COUNT = 8;
#define TRUE true
#define FALSE false
class CString : public std::string {
public:
 using std::string::string;
 operator const char*() const { return c_str(); }
 int GetLength() const { return static_cast<int>(size()); }
 void Format(const char*, const CString&, const char*) {}
};
void lstrcpy(char* dest, const CString& src) { std::strcpy(dest, src.c_str()); }
struct CSPOPLogin {
 int m_nRET = 1, next = 1, calls = 0;
 char m_szID[MAX_NAME+1]{}, m_szPW[MAX_NAME+1]{};
 bool ok = true, write_output = true;
 bool Call() { ++calls; if(write_output) m_nRET = next; return ok; }
};
struct Database {
 CSPOPLogin query;
 bool available = true;
 CSPOPLogin* GetQuery(const char*) { return available ? &query : nullptr; }
};
struct CTControlSession {};
struct CTManager : CTControlSession {
 sockaddr_in m_addr{};
 bool m_bManager = false, m_bLock = true;
 BYTE m_bAuthority = 0;
 DWORD m_dwID = 0;
 CString m_strID;
 int replies = 0, result = -1, lists = 0;
 void SendCT_OPLOGIN_ACK(BYTE ret, BYTE, DWORD = 0) { ++replies; result = ret; }
 void SendCT_GROUPLIST_ACK(int*) { ++lists; }
 void SendCT_MACHINELIST_ACK(int*) { ++lists; }
 void SendCT_SVRTYPELIST_ACK(int*) { ++lists; }
 void SendCT_SERVICEAUTOSTART_ACK(bool) { ++lists; }
};
struct Packet {
 CString id = "operator", password = "test-only";
 bool first = true;
 Packet& operator>>(CString& value) { value = first ? id : password; first = false; return *this; }
};
struct PacketBuf { CTManager* m_pSESSION; Packet m_packet; };
using LPPACKETBUF = PacketBuf*;
using MAPTMANAGER = std::map<DWORD_PTR, CTManager*>;
struct CTControlSvrModule {
 Database m_db;
 DWORD m_dwManagerSeq = 0;
 int m_mapTGroup = 0, m_mapTMachine = 0, m_mapTSvrType = 0;
 bool m_bAutoStart = false;
 MAPTMANAGER m_mapMANAGER;
 CTManager* FindManager(const CString&) { return nullptr; }
 void LogSmartly(const CString&) {}
 void CloseSession(CTControlSession*) {}
 DWORD OnCT_OPLOGIN_REQ(LPPACKETBUF);
};
#define DEFINE_QUERY(database, type) { type* query = (database)->GetQuery(#type); if(query) {
#define UNDEFINE_QUERY() }}
'''
cases = r'''
int main() {
 CTControlSvrModule server;
 // Reuse one query with authority 1 across distinct VM sessions.
 for(int i = 0; i != 2; ++i) {
  CTManager manager;
  manager.m_addr.sin_addr.s_addr = inet_addr("192.0.2.2");
  PacketBuf request{&manager, {}};
  assert(server.OnCT_OPLOGIN_REQ(&request) == EC_NOERROR);
  assert(manager.replies == 1 && manager.result == 0 && manager.lists == 4);
  assert(manager.m_bManager && manager.m_bAuthority == MANAGER_ALL);
  server.m_mapMANAGER.clear(); // Simulate a completed disconnect.
 }
 assert(server.m_db.query.calls == 2);
 auto reject = [&](int result, bool ok, bool write_output) {
  CTManager manager;
  PacketBuf request{&manager, {}};
  server.m_db.query.m_nRET = MANAGER_ALL;
  server.m_db.query.next = result;
  server.m_db.query.ok = ok;
  server.m_db.query.write_output = write_output;
  assert(server.OnCT_OPLOGIN_REQ(&request) == EC_NOERROR);
  assert(manager.replies == 1 && manager.result == 1 && manager.lists == 0);
  assert(!manager.m_bManager && server.m_mapMANAGER.empty());
 };
 reject(0, true, true); // Invalid credentials after a prior success.
 reject(0, false, false); // Failed Call must not authenticate stale authority.
 reject(0, true, false); // Successful Call without an output must also fail.
 reject(-1, true, true);
 reject(MANAGER_COUNT, true, true);
 reject(257, true, true); // Do not truncate an out-of-range authority to BYTE 1.
 server.m_db.available = false;
 CTManager missing;
 PacketBuf no_query{&missing, {}};
 assert(server.OnCT_OPLOGIN_REQ(&no_query) == EC_NOERROR);
 assert(missing.replies == 1 && missing.result == 1);
 server.m_db.available = true;
 server.m_db.query.ok = true;
 server.m_db.query.write_output = true;
 server.m_db.query.next = MANAGER_ALL;
 // Both fields at the maximum accepted length must fit, including NUL.
 CTManager boundary;
 PacketBuf boundary_request{&boundary, {}};
 boundary_request.m_packet.id = CString(sizeof(server.m_db.query.m_szID) - 1, 'x');
 boundary_request.m_packet.password = CString(sizeof(server.m_db.query.m_szPW) - 1, 'y');
 assert(server.OnCT_OPLOGIN_REQ(&boundary_request) == EC_NOERROR);
 assert(boundary.replies == 1 && boundary.result == 0 && boundary.m_bManager);
 server.m_mapMANAGER.clear();
 for(bool id : {true, false}) {
  CTManager manager;
  PacketBuf request{&manager, {}};
  if(id) request.m_packet.id = CString(sizeof(server.m_db.query.m_szID), 'x');
  else request.m_packet.password = CString(sizeof(server.m_db.query.m_szPW), 'x');
  const int calls = server.m_db.query.calls;
  assert(server.OnCT_OPLOGIN_REQ(&request) == EC_NOERROR);
  assert(manager.replies == 1 && manager.result == 1 && !manager.m_bManager);
  assert(server.m_db.query.calls == calls);
 }
}
'''
# Keep test build artifacts in the isolated checkout.
build = root / 'runtime/legacy' / 'control-handler-test'
build.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(dir=build) as temporary:
    folder = Path(temporary)
    cpp = folder / 'handler.cpp'
    cpp.write_text(name_define + '\n' + scaffold + handler + cases, encoding='latin1')
    executable = folder / 'handler-test'
    subprocess.run(['clang++', '-std=c++17', '-DFOURSTORY_NATIVE_MODULE',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(cpp), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('PASS native OPLOGIN reused-query regression (ASan/UBSan)')
