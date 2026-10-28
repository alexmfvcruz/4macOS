// C2: event semantics of the native CTachyonSession branch against loopback
// peers. Expected behavior is the Winsock behavior that CTachyonApp::Run
// relies on (TachyonApp.cpp:260-275): one FD_CONNECT per Start() with the
// result, FD_READ for data, FD_CLOSE after the data, and no FD_CLOSE after a
// failed connect.
#include <fourstory_engine_session.h>
#include "TachyonSession.h"
#include <string>
#include <vector>

extern INT64 g_4skey[KEY_COUNT];

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

namespace {
struct Listener {
    int fd = -1; WORD port = 0;
    Listener() {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_addr.s_addr = inet_addr("127.0.0.1");
        ::bind(fd, (sockaddr*)&a, sizeof(a)); ::listen(fd, 4);
        socklen_t n = sizeof(a); ::getsockname(fd, (sockaddr*)&a, &n); port = ntohs(a.sin_port);
    }
    ~Listener() { if (fd >= 0) ::close(fd); }
    int accept_one() {
        for (int i = 0; i < 200; ++i) { const int c = ::accept(fd, nullptr, nullptr); if (c >= 0) return c; Sleep(5); }
        return -1;
    }
};
// The server side of CPacket framing (Server/TNetLib/Session.cpp Encrypt
// without the client-to-server RC4 layer).
std::string server_packet(WORD id, DWORD number, DWORD value) {
    CPacket packet;
    packet.SetID(id) << value;
    packet.m_pHeader->m_dwNumber = number;
    const INT64 key = g_4skey[number % KEY_COUNT];
    packet.Encrypt(key);
    packet.EncryptHeader(key);
    return std::string(reinterpret_cast<const char*>(packet.GetBuffer()), packet.GetSize());
}

struct Owner : CTachyonWnd {
    std::vector<std::string> events;
    std::function<void(CTachyonSession*)> on_message;
    void OnSessionMsg(CTachyonSession* s, CPacket* p) override {
        DWORD value = 0; (*p) >> value;
        events.push_back("msg " + std::to_string(p->GetID()) + " " + std::to_string(value));
        if (on_message) on_message(s);
    }
    void OnConnect(CTachyonSession*, int e) override { events.push_back("connect " + std::to_string(e)); }
    void OnClose(CTachyonSession*, int e) override { events.push_back("close " + std::to_string(e)); }
};
void pump(int rounds) { for (int i = 0; i < rounds; ++i) CTachyonSession::DispatchNative(20); }
int count(const std::vector<std::string>& v, const std::string& prefix) {
    int n = 0; for (const auto& e : v) n += e.compare(0, prefix.size(), prefix) == 0; return n;
}

// A refused connect: Start() returns TRUE and the result arrives as one
// FD_CONNECT with an error; no FD_CLOSE follows.
void refused() {
    WORD port; { Listener closed; port = closed.port; }
    Owner owner; CTachyonSession session; session.SetOwner(&owner);
    CHECK(session.Start("127.0.0.1", port));
    pump(20);
    CHECK(owner.events.size() == 1 && owner.events[0] == "connect " + std::to_string(WSAECONNREFUSED));
    CHECK(!session.IsValid());
}

// Data then close: FD_READ delivers each packet, FD_CLOSE comes after them.
void data_then_close() {
    Listener listener;
    Owner owner; CTachyonSession session; session.SetOwner(&owner);
    CHECK(session.Start("127.0.0.1", listener.port));
    const int peer = listener.accept_one();
    CHECK(peer >= 0);
    pump(5);
    CHECK(owner.events.size() == 1 && owner.events[0] == "connect 0" && session.IsValid());
    const std::string two = server_packet(0x1989, 1, 7) + server_packet(0x1989, 2, 8);
    CHECK(::send(peer, two.data(), two.size(), 0) == ssize_t(two.size()));
    ::close(peer);
    pump(10);
    CHECK(owner.events.size() == 4);
    if (owner.events.size() == 4) CHECK(owner.events[1] == "msg 6537 7" && owner.events[2] == "msg 6537 8" && owner.events[3] == "close 0");
    pump(5);
    CHECK(count(owner.events, "close") == 1);
}

// A callback that restarts another session: the restarted session must get
// its own FD_CONNECT, not a stale event of its old connection, even when the
// new socket reuses the descriptor number.
void restart_in_callback() {
    Listener listener;
    Owner first, second;
    CTachyonSession a, b; a.SetOwner(&first); b.SetOwner(&second);
    CHECK(a.Start("127.0.0.1", listener.port)); const int peer_a = listener.accept_one();
    CHECK(b.Start("127.0.0.1", listener.port)); const int peer_b = listener.accept_one();
    pump(5);
    CHECK(count(first.events, "connect 0") == 1 && count(second.events, "connect 0") == 1);
    const SOCKET old_b = b.m_sock;
    int reused = 0;
    first.on_message = [&](CTachyonSession*) {
        b.End();
        b.Start("127.0.0.1", listener.port);
        reused = b.m_sock == old_b;
    };
    // Both sessions readable in the same pass.
    const std::string pa = server_packet(0x1989, 1, 1), pb = server_packet(0x1989, 1, 2);
    ::send(peer_b, pb.data(), pb.size(), 0);
    ::send(peer_a, pa.data(), pa.size(), 0);
    Sleep(50);
    second.events.clear();
    CTachyonSession::DispatchNative(100);
    first.on_message = nullptr;
    const int peer_b2 = listener.accept_one();
    pump(5);
    CHECK(count(second.events, "msg") == 0);          // the old connection's data is not delivered
    CHECK(count(second.events, "connect 0") == 1);    // exactly one FD_CONNECT for the new connection
    CHECK(b.IsValid());
    std::printf("restart: descriptor reused=%d\n", reused);
    ::close(peer_a); ::close(peer_b); if (peer_b2 >= 0) ::close(peer_b2);
}
}

int main() {
    CTachyonSession::InitTachyonSession();
    refused();
    data_then_close();
    restart_in_callback();
    CTachyonSession::ReleaseTachyonSession();
    std::printf(failures ? "FAILED %d checks\n" : "PASS session_test\n", failures);
    return failures ? 1 : 0;
}
