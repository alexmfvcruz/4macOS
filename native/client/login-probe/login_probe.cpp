// C2 criterion 1: native login probe. The original CTachyonSession, CPacket,
// RC4 encryption (CryptographyExt.cpp) and SHA1 (Client/TClient/SHA1.cpp) run
// unchanged. The CTClientWnd senders and handlers need the window and the 3D
// engine (C4/C5), so this file writes and reads the same fields in the same
// order; each block names its source lines. The probe stops after the
// character list (CS_CHARLIST_ACK), as the acceptance criterion asks.
//
// Usage: c2-login-probe HOST PORT USER CLIENT_EXE [--wrong-password]
//   HOST must not be 127.0.0.1: Login treats loopback peers as Control.
//   The password is read from the terminal without echo (or from stdin when
//   stdin is not a terminal). It is never printed. --wrong-password sends a
//   random password and expects LR_INVALIDPASSWD or LR_NOUSER.
//   CLIENT_EXE is the Windows TClient.exe; its 8-byte XOR is the check value
//   of CS_LOGIN_REQ and CS_HOTSEND_REQ (TClient.cpp:483-503, TClientWnd.cpp:7341-7424).
#include <fourstory_engine_session.h>
#include "TachyonSession.h"
#include <CSProtocol.h>
#include <NetCode.h>
#include "SHA1.h"
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <termios.h>
#include <csignal>

namespace {
int failures = 0;
void fail(const char* text) { std::printf("FAIL %s\n", text); ++failures; }
DWORD now_ms() { return GetTickCount(); }

// TClient.cpp:494-503: XOR of the file as 8-byte words. ReadFile overwrites only
// the bytes it reads, so a short last block keeps the high bytes of the block
// before it (as native/server/client_reference_file.hpp mirrors).
bool check_file_value(const std::string& path, INT64& value) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    unsigned char block[8] = {};
    UINT64 sum = 0;
    for (;;) {
        in.read(reinterpret_cast<char*>(block), 8);
        const std::streamsize got = in.gcount();
        if (got <= 0) break;
        UINT64 word = 0;
        for (int i = 0; i < 8; ++i) word |= UINT64(block[i]) << (8 * i);
        sum ^= word;
        if (got < 8) break;
    }
    value = INT64(sum);
    return true;
}
// TClientWnd.cpp:7408-7417 (bAll FALSE): SetFilePointer(DWORD(dlCP)) with a
// LONG distance and no high part, then one 8-byte ReadFile into dlCheck = 0.
// Returns false when the file cannot be opened: CheckModuleFile then sends
// nothing (TClientWnd.cpp:7394).
bool check_point_value(const std::string& path, INT64 dlCP, INT64& value) {
    value = 0;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    const LONG distance = LONG(DWORD(dlCP));
    if (distance < 0) return true; // INVALID_SET_FILE_POINTER: dlCheck stays 0
    in.seekg(distance);
    unsigned char block[8] = {};
    in.read(reinterpret_cast<char*>(block), 8);
    UINT64 word = 0;
    for (std::streamsize i = 0; i < in.gcount(); ++i) word |= UINT64(block[i]) << (8 * i);
    value = INT64(word);
    return true;
}

struct Group { CString name; BYTE id, type, status, chars; };

class ProbeWnd : public CTachyonWnd {
public:
    ProbeWnd(std::string user, std::string password_hash, std::string exe, bool expect_reject)
        : user_(user), hash_(password_hash), exe_(exe), expect_reject_(expect_reject) {}
    CTachyonSession session;
    bool done = false;
    std::vector<Group> groups;
    size_t channel_group = 0;

    void OnConnect(CTachyonSession*, int nErrorCode) override {
        std::printf("event\tconnect\t%d\n", nErrorCode);
        if (nErrorCode) { fail("connect"); done = true; return; }
        SendLogin();
    }
    void OnClose(CTachyonSession*, int nErrorCode) override {
        std::printf("event\tclose\t%d\n", nErrorCode);
        if (!finished_) fail("server closed the session before the character list");
        done = true;
    }
    void OnSessionMsg(CTachyonSession*, CPacket* pPacket) override {
        const WORD id = pPacket->GetID();
        std::printf("recv\t0x%04X\t%u\n", id, unsigned(pPacket->GetSize()));
        switch (id) {
        case CS_LOGIN_ACK: OnLoginAck(pPacket); break;
        case CS_GROUPLIST_ACK: OnGroupListAck(pPacket); break;
        case CS_CHANNELLIST_ACK: OnChannelListAck(pPacket); break;
        case CS_CHARLIST_ACK: OnCharListAck(pPacket); break;
        default: break; // the client ignores other messages in this phase
        }
    }

private:
    void Say(CPacket& vMSG) {
        std::printf("send\t0x%04X\t%u\n", unsigned(vMSG.GetID()), unsigned(vMSG.GetSize()));
        session.Say(&vMSG);
    }
    // TNetSender.cpp:11-49 (CTNationOption::MODIFY_DIRECTLOGIN is FALSE outside Japan).
    void SendLogin() {
        CPacket vMSG;
        WORD wVersion = TVERSION;
        CString Zombie3 = 0;
        CString Zombie2 = 0;
        CString Zombie1 = 0;
        INT64 key = 0x336c3aebf71a8b08;
        INT64 llChecksum = wVersion * 2 - 500;
        INT64 dwIndex = llChecksum % sizeof(INT64);
        INT64 dwBody = llChecksum / sizeof(INT64);
        for (DWORD i = 0; i < dwIndex; i++) {
            llChecksum ^= dwBody;
            llChecksum = INT64(UINT64(llChecksum) + UINT64(key)); // two's-complement wrap, as compiled for x86
        }
        INT64 dlCheckFile = 0;
        if (!check_file_value(exe_, dlCheckFile)) fail("cannot read CLIENT_EXE");
        vMSG.SetID(CS_LOGIN_REQ)
            << wVersion
            << Zombie3
            << CString(hash_.c_str())
            << Zombie1
            << Zombie2
            << CString(user_.c_str())
            << dlCheckFile
            << llChecksum;
        Say(vMSG);
    }
    void CheckModuleFile(DWORD dwCP) {
        INT64 dlCheck;
        if (check_point_value(exe_, dwCP, dlCheck)) SendHotSend(dlCheck, FALSE);
        else fail("cannot read CLIENT_EXE for CS_HOTSEND_REQ");
    }
    // TNetSender.cpp:152-161
    void SendHotSend(INT64 dlValue, BYTE bAll) {
        CPacket vMSG;
        vMSG.SetID(CS_HOTSEND_REQ) << dlValue << bAll;
        Say(vMSG);
    }
    // TNetSender.cpp:51-57, 133-141, 59-66
    void SendGroupList() { CPacket vMSG; vMSG.SetID(CS_GROUPLIST_REQ); Say(vMSG); }
    void SendChannelList(BYTE bGroupID) { CPacket vMSG; vMSG.SetID(CS_CHANNELLIST_REQ) << bGroupID; Say(vMSG); }
    void SendCharList() { CPacket vMSG; vMSG.SetID(CS_CHARLIST_REQ) << BYTE(1); Say(vMSG); }

    // TNetHandler.cpp:6-30 (fields) and 78-88 (next step).
    void OnLoginAck(CPacket* pPacket) {
        BYTE bResult; DWORD dwUserID, dwKickID, dwKEY, dwAddr; WORD wPort;
        BYTE bCreateCardCnt, bInPCROOM; DWORD dwPrmPCROOM; __time64_t dCurrentTime; INT64 dKey;
        CString strReason; __int64 iDuration; BYTE bEternal;
        (*pPacket) >> bResult >> dwUserID >> dwKickID >> dwKEY >> dwAddr >> wPort >> bCreateCardCnt
                   >> bInPCROOM >> dwPrmPCROOM >> dCurrentTime >> dKey >> strReason >> iDuration >> bEternal;
        std::printf("login\tresult=%u\n", unsigned(bResult));
        if (expect_reject_) {
            if (bResult != LR_INVALIDPASSWD && bResult != LR_NOUSER) fail("expected LR_INVALIDPASSWD or LR_NOUSER");
            finished_ = true; done = true; return;
        }
        if (bResult == LR_NEEDAGREEMENT) { fail("account needs the agreement page (first login); use an account that has logged in before"); done = true; return; }
        if (bResult != LR_SUCCESS) { fail("login rejected"); done = true; return; }
        SendGroupList(); // OnGM_BACKTO_SERVER_DIRECT, TNetGM.cpp:829-834
    }
    // TNetHandler.cpp:315-371; ordering: _compare_group, TNetHandler.cpp:170-200.
    void OnGroupListAck(CPacket* pPacket) {
        BYTE bCount = 0; DWORD dwCP;
        (*pPacket) >> bCount >> dwCP;
        CheckModuleFile(dwCP);
        groups.clear();
        for (BYTE i = 0; i < bCount; i++) {
            Group g;
            (*pPacket) >> g.name >> g.id >> g.type >> g.status >> g.chars;
            groups.push_back(g);
        }
        // _compare_group, TNetHandler.cpp:226-258. Equal character counts and
        // equal status priorities fall back to a random priority in the
        // client; the probe keeps the server order in that case.
        static const BYTE bPRIORITY[] = { 0, 3, 2, 1 };
        std::stable_sort(groups.begin(), groups.end(), [](const Group& l, const Group& r) {
            if (l.status == TSTATUS_SLEEP && r.status != TSTATUS_SLEEP) return false;
            if (r.status == TSTATUS_SLEEP && l.status != TSTATUS_SLEEP) return true;
            if (l.chars != r.chars) return l.chars > r.chars;
            const BYTE pl = l.status < 4 ? bPRIORITY[l.status] : 0, pr = r.status < 4 ? bPRIORITY[r.status] : 0;
            return pl > pr;
        });
        for (const Group& g : groups)
            std::printf("group\t%u\t%s\tstatus=%u\tcharacters=%u\n", g.id, (LPCTSTR)g.name, g.status, g.chars);
        channel_group = 0;
        if (!groups.empty()) SendChannelList(groups[0].id);
        else fail("no server groups");
    }
    // TNetHandler.cpp:713-760 (reads bCount - 1 channels, "BRB" change).
    void OnChannelListAck(CPacket* pPacket) {
        BYTE bCount; DWORD dwCP;
        (*pPacket) >> bCount >> dwCP;
        CheckModuleFile(dwCP);
        // The original computes BYTE bCountEdit = bCount - 1, so zero channels
        // would read 255 entries past the data; the probe reports that case.
        if (!bCount) { fail("group with zero channels (the original client over-reads here)"); done = true; return; }
        BYTE bCountEdit = bCount - 1;
        for (BYTE i = 0; i < bCountEdit; ++i) {
            CString strNAME; BYTE bID, bSTATUS;
            (*pPacket) >> strNAME >> bID >> bSTATUS;
            std::printf("channel\t%u\t%u\t%s\tstatus=%u\n", groups[channel_group].id, bID, (LPCTSTR)strNAME, bSTATUS);
            // OnGM_SELECT_CHANNEL (TNetGM.cpp:350-393) needs a channel with
            // status NORMAL or BUSY before it sends CS_CHARLIST_REQ.
            if (bSTATUS == TSTATUS_NORMAL || bSTATUS == TSTATUS_BUSY) selectable_ = true;
        }
        if (++channel_group < groups.size()) SendChannelList(groups[channel_group].id);
        else if (selectable_) SendCharList(); // the user selects a channel: TNetGM.cpp:393
        else { fail("no channel with status NORMAL or BUSY to select"); done = true; }
    }
    // TNetHandler.cpp:372-440 and CTClientItem::SetItemSlot, TClientItem.cpp:800-804.
    void OnCharListAck(CPacket* pPacket) {
        DWORD dwCP; BYTE bCount;
        (*pPacket) >> dwCP >> bCount;
        for (BYTE i = 0; i < bCount; i++) {
            DWORD dwID, dwRegionID, dwFame, dwFameColor; CString strNAME;
            BYTE bStartAct, bSlotID, bLevel, bClass, bRace, bContry, bSex, bHair, bFace, bBody, bPants, bHand, bFoot, bHelmetHide, bItemCount;
            (*pPacket) >> dwID >> strNAME >> bStartAct >> bSlotID >> bLevel >> bClass >> bRace >> bContry >> bSex
                       >> bHair >> bFace >> bBody >> bPants >> bHand >> bFoot >> dwRegionID >> dwFame >> dwFameColor
                       >> bHelmetHide >> bItemCount;
            for (BYTE j = 0; j < bItemCount; j++) {
                BYTE bSlot, bGrade, bGradeEffect, bRegGuild; WORD wItemID, wColor, wCustomTex, wMoggItemID;
                (*pPacket) >> bSlot >> wItemID >> bGrade >> bGradeEffect >> wColor >> wCustomTex >> bRegGuild >> wMoggItemID;
            }
            std::printf("character\tslot=%u\t%s\tlevel=%u\tclass=%u\trace=%u\tequipped=%u\n",
                        bSlotID, (LPCTSTR)strNAME, bLevel, bClass, bRace, bItemCount);
        }
        std::printf("characters\t%u\n", bCount);
        if (pPacket->GetReadBytes() && !pPacket->IsEOF()) fail("unread bytes after the character list");
        finished_ = true; done = true;
    }

    std::string user_, hash_, exe_;
    bool expect_reject_;
    bool finished_ = false;
    bool selectable_ = false;
};

termios saved_terminal{};
volatile sig_atomic_t terminal_changed = 0;
void restore_terminal(int signal_number) {
    if (terminal_changed) tcsetattr(0, TCSANOW, &saved_terminal);
    std::signal(signal_number, SIG_DFL);
    std::raise(signal_number);
}
std::string read_password() {
    std::string password;
    if (isatty(0)) {
        std::fprintf(stderr, "Password (not shown): ");
        termios quiet{};
        tcgetattr(0, &saved_terminal); quiet = saved_terminal; quiet.c_lflag &= ~tcflag_t(ECHO);
        std::signal(SIGINT, restore_terminal); std::signal(SIGTERM, restore_terminal);
        terminal_changed = 1;
        tcsetattr(0, TCSANOW, &quiet);
        std::getline(std::cin, password);
        tcsetattr(0, TCSANOW, &saved_terminal);
        terminal_changed = 0;
        std::fprintf(stderr, "\n");
    } else {
        std::getline(std::cin, password);
    }
    return password;
}
}

int main(int argc, char** argv) {
    if (argc != 5 && !(argc == 6 && std::string(argv[5]) == "--wrong-password")) {
        std::printf("usage: c2-login-probe HOST PORT USER CLIENT_EXE [--wrong-password]\n");
        return 2;
    }
    const bool wrong = argc == 6;
    std::string password;
    if (wrong) { char buffer[32]; std::snprintf(buffer, sizeof(buffer), "wrong-%u", unsigned(now_ms())); password = buffer; }
    else password = read_password();
    SHA1 hasher; hasher.update(password); // CTClientGame::GetSHA1String, TClientGame.cpp:462-471
    const std::string hash = hasher.final();
    // The plain password is not needed after hashing.
    { volatile char* p = &password[0]; for (size_t i = 0; i < password.size(); ++i) p[i] = 0; }

    CTachyonSession::InitTachyonSession();
    ProbeWnd wnd(argv[3], hash, argv[4], wrong);
    wnd.session.SetOwner(&wnd);
    if (!wnd.session.Start(argv[1], DWORD(std::strtoul(argv[2], nullptr, 10)))) { std::printf("FAIL session start, error %u\n", GetLastError()); return 1; }
    const DWORD start = now_ms();
    while (!wnd.done && now_ms() - start < 20000) CTachyonSession::DispatchNative(100);
    if (!wnd.done) fail("timeout after 20 s");
    wnd.session.End();
    CTachyonSession::ReleaseTachyonSession();
    std::printf(failures ? "FAILED login probe\n" : "PASS login probe\n");
    return failures ? 1 : 0;
}
