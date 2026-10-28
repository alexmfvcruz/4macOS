// Unit tests for native/client/compat. Expected values follow the documented
// behavior of MFC 14 (MBCS build) and the Microsoft CRT.
// The original Engine Lib headers put "using namespace std" before <wincrypt.h>
// (T3D.h); the layer must compile under it.
#include <functional>
using namespace std;
#include <afx.h>
#include <afxwin.h>
#include <afxsock.h>
#include <d3d9types.h>
#include <wincrypt.h>
#include <Security.h>
#include <nb30.h>
#include <poll.h>
#include <fstream>
#include <sstream>
#include <io.h>
#include <malloc.h>
#include <vector>

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

static void widths() {
    CHECK(sizeof(BYTE) == 1 && sizeof(WORD) == 2 && sizeof(DWORD) == 4 && sizeof(LONG) == 4 && sizeof(ULONG) == 4);
    CHECK(sizeof(BOOL) == 4 && sizeof(INT) == 4 && sizeof(UINT) == 4 && sizeof(FLOAT) == 4 && sizeof(WCHAR) == 2);
    CHECK(sizeof(INT64) == 8 && sizeof(__int64) == 8 && sizeof(LONGLONG) == 8);
    CHECK(__int64(5) * 1000000000 == 5000000000ll && (__int64)1 << 40 == 1099511627776ll);
    CHECK(_fileno(stdout) == 1);
    CHECK(sizeof(DWORD_PTR) == sizeof(void*) && sizeof(LPARAM) == sizeof(void*) && sizeof(WPARAM) == sizeof(void*) && sizeof(HANDLE) == sizeof(void*));
    CHECK(sizeof(RECT) == 16 && sizeof(POINT) == 8 && sizeof(SIZE) == 8 && sizeof(CRect) == 16 && sizeof(CPoint) == 8);
    PINT64 wide_pointer = nullptr; CHECK(sizeof(*wide_pointer) == 8);
    CHECK((D3DFVF_XYZB4 | D3DFVF_LASTBETA_UBYTE4 | D3DFVF_NORMAL | D3DFVF_TEX1) == 0x111c && (D3DFVF_XYZ | D3DFVF_TEX2) == 0x202);
    CHECK(sizeof(CString) == sizeof(void*) && sizeof(INT_PTR) == 8 && sizeof(SIZE_T) == 8 && sizeof(LRESULT) == 8);
    CHECK(MAKEWORD(0x34, 0x12) == 0x1234 && MAKELONG(0x5678, 0x1234) == 0x12345678 && LOWORD(0x12345678) == 0x5678 && HIWORD(0x12345678) == 0x1234);
    CHECK(RGB(1, 2, 3) == 0x00030201u && D3DCOLOR_XRGB(1, 2, 3) == 0xFF010203u && D3DCOLOR_ARGB(4, 1, 2, 3) == 0x04010203u);
}

static void strings() {
    CString empty;
    CString zero = 0; CHECK(zero.IsEmpty());
    CHECK(empty.IsEmpty() && empty.GetLength() == 0 && std::strcmp(empty, "") == 0);
    CString a("Hello"), b(a), c;
    c = a; c += ", "; c += CString("World"); c += '!';
    CHECK(c == "Hello, World!" && c.GetLength() == 13 && b == a && a != c && a < c);
    CHECK(c.Left(5) == "Hello" && c.Right(6) == "World!" && c.Mid(7) == "World!" && c.Mid(7, 5) == "World");
    CHECK(c.Left(-1) == "" && c.Left(100) == c && c.Mid(100) == "" && c.Mid(-3, 2) == "He" && c.Right(100) == c);
    CHECK(c.Find('o') == 4 && c.Find('o', 5) == 8 && c.Find("World") == 7 && c.Find("xyz") == -1 && c.Find('H', 99) == -1);
    CHECK(c.ReverseFind('o') == 8 && c.ReverseFind('z') == -1 && c.FindOneOf("!,") == 5 && c.FindOneOf("xyz") == -1);
    CHECK(c[0] == 'H' && c[(DWORD)1] == 'e' && c[(size_t)2] == 'l' && c.GetAt(12) == '!');
    CString self("abc"); const CString& alias = self; self = alias; self += self;
    CHECK(self == "abcabc");
    self = self.GetString() + 2;
    CHECK(self == "cabc");

    CString f;
    f.Format("%d-%s-%05.1f-%u", -7, "x", 2.5, 4000000000u);
    CHECK(f == "-7-x-002.5-4000000000");
    f.Format("[%s|%s|%d]", c, CString("y"), 3); // CString arguments
    CHECK(f == "[Hello, World!|y|3]");
    f.Format("%s%s", f, f); // the argument is the target
    CHECK(f == "[Hello, World!|y|3][Hello, World!|y|3]");
    f.Format("");
    CHECK(f.IsEmpty());
    f.AppendFormat("%c%c", 'o', 'k');
    CHECK(f == "ok");
    // Microsoft length modifiers: LONG and DWORD are 32 bits with %ld and %lu.
    f.Format("[%ld][%lu][%lx][%5ld][%I64d][%I64u][%I32d][%Iu][%lld][%hd][%%ld][%s]", (LONG)-5, (DWORD)4000000000u, (DWORD)0xABCDEF01u,
             (LONG)42, (INT64)-9000000000ll, (UINT64)18000000000000000000ull, 7, (DWORD_PTR)0x100000000ull, -3ll, (short)-2, "l");
    CHECK(f == "[-5][4000000000][abcdef01][   42][-9000000000][18000000000000000000][7][4294967296][-3][-2][%ld][l]");
    f.Format("%.2f|%lf|%c|%-4d|%04X", 1.5, 2.25, 'c', 3, 255);
    CHECK(f == "1.50|2.250000|c|3   |00FF");
    char big[3000]; std::memset(big, 'z', sizeof(big) - 1); big[sizeof(big) - 1] = 0;
    f.Format("%s", big);
    CHECK(f.GetLength() == 2999 && f[2998] == 'z');

    CString t("  \t x y \r\n");
    t.TrimLeft(); CHECK(t == "x y \r\n");
    t.TrimRight(); CHECK(t == "x y");
    CString u("  pad  "); u.Trim(); CHECK(u == "pad");
    CString v("xxaxx"); v.Trim('x'); CHECK(v == "a");
    CString w("--=a=--"); w.Trim("-="); CHECK(w == "a");
    CString only("   "); only.Trim(); CHECK(only.IsEmpty());

    CString r("a.b.c");
    CHECK(r.Replace('.', '/') == 2 && r == "a/b/c" && r.Replace('x', 'y') == 0);
    CHECK(r.Replace("/", "::") == 2 && r == "a::b::c" && r.Replace("::", "") == 2 && r == "abc");
    CHECK(r.Replace("", "x") == 0 && r.Replace("zz", "x") == 0 && r == "abc");
    CString rm("banana"); CHECK(rm.Remove('a') == 3 && rm == "bnn");
    CString ins("ad"); CHECK(ins.Insert(1, "bc") == 4 && ins == "abcd" && ins.Insert(99, 'e') == 5 && ins == "abcde");
    CHECK(ins.Delete(1, 2) == 3 && ins == "ade" && ins.Delete(2, 50) == 2 && ins == "ad" && ins.Delete(9) == 2);
    CString up("abcXYZ1\xE9"); up.MakeUpper(); CHECK(up == "ABCXYZ1\xE9");
    up.MakeLower(); CHECK(up == "abcxyz1\xE9");
    CHECK(CString("abc") <= "abc" && CString("abc") >= "abb" && "abc" <= CString("abd") && "abd" >= CString("abc") && !(CString("b") <= "a"));
    CHECK(CString("abc").CompareNoCase("ABC") == 0 && CString("abc").Compare("abd") < 0);

    CString list(";a;;b c;"); int position = 0; std::vector<CString> tokens;
    for (CString token = list.Tokenize("; ", position); position != -1; token = list.Tokenize("; ", position)) tokens.push_back(token);
    CHECK(tokens.size() == 3 && tokens[0] == "a" && tokens[1] == "b" && tokens[2] == "c");

    CString g;
    char* buffer = g.GetBuffer(16);
    std::strcpy(buffer, "direct");
    g.ReleaseBuffer();
    CHECK(g == "direct" && g.GetLength() == 6);
    g.ReleaseBuffer(3); CHECK(g == "dir");
    CHECK(std::atoi(g.GetBuffer()) == 0);
    g.Empty(); CHECK(g.IsEmpty() && g == "");
    CString z; char* zb = z.GetBuffer(0); // MFC gives room for 7 characters
    std::strcpy(zb, "1234567"); z.ReleaseBuffer();
    CHECK(z == "1234567" && z.GetLength() == 7);
    CString never; never.ReleaseBuffer(); never.TrimLeft(); never.MakeUpper(); never.Remove('x'); never.Delete(0, 3);
    CHECK(never.IsEmpty() && CString().IsEmpty());
    CHECK(CString('x', 3) == "xxx" && CString("abcdef", 3) == "abc" && (CString("a") + "b" + 'c' + CString("d")) == "abcd" && ("x" + CString("y")) == "xy");
    CString high("\xC0\xE9\xFF");
    CHECK(high.GetLength() == 3 && (unsigned char)high[2] == 0xFF);
}

static void crt() {
    char small[4], big[16];
    CHECK(strcpy_s(big, "abc") == 0 && std::strcmp(big, "abc") == 0);
    std::strcpy(small, "zz");
    CHECK(strcpy_s(small, "abcd") == ERANGE && small[0] == 0);
    CHECK(strcat_s(big, "def") == 0 && std::strcmp(big, "abcdef") == 0);
    CHECK(strncpy_s(small, "abcdef", _TRUNCATE) == 80 && std::strcmp(small, "abc") == 0);
    CHECK(strncpy_s(big, "abcdef", 2) == 0 && std::strcmp(big, "ab") == 0);
    CHECK(sprintf_s(big, "%d:%s", 5, CString("q")) == 3 && std::strcmp(big, "5:q") == 0);
    CHECK(sprintf_s(small, "%d", 12345) == -1 && small[0] == 0);
    CHECK(sprintf_s(big, "%ld/%I64d", (LONG)-1, (INT64)5) == 4 && std::strcmp(big, "-1/5") == 0);
    char full[3] = {'a', 'b', 'c'};
    CHECK(strcat_s(full, 3, "x") == EINVAL && full[0] == 0);
    char exact[4] = {'#', '#', '#', '#'};
    CHECK(_snprintf(exact, 3, "%s", "abc") == 3 && exact[2] == 'c' && exact[3] == '#');
    CHECK(_snprintf(exact, 3, "%s", "abcdef") == -1 && exact[0] == 'a' && exact[3] == '#');
    int source[2] = {1, 2}, target[2] = {0, 0};
    CHECK(memcpy_s(target, sizeof(target), source, sizeof(source)) == 0 && target[1] == 2);
    CHECK(memcpy_s(target, 4, source, 8) == ERANGE && target[0] == 0);
    CHECK(_stricmp("ABC", "abc") == 0 && _strnicmp("ABx", "abY", 2) == 0 && _atoi64("-9000000000") == -9000000000ll);
    CHECK(lstrlen("abc") == 3 && lstrlen(nullptr) == 0 && lstrcmpi("A", "a") == 0 && lstrcmp("a", "b") < 0);
#ifndef _AFX
    CHECK(false);
#endif
    const DWORD first = GetTickCount(); Sleep(2);
    CHECK(GetTickCount() - first >= 1);
}

static void files(const char* directory) {
    CString path; path.Format("%s\\Sub\\Data.BIN", directory);
    {
        CFile file;
        CHECK(file.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary));
        CArchive ar(&file, CArchive::store);
        ar << (BYTE)0x12 << (WORD)0x3456 << (DWORD)0x789ABCDEu << (int)-2 << 1.5f << (INT64)0x0102030405060708ll;
        ar << (long)-3 << (unsigned long)0xFFFFFFF0ul;
        ar << CString("abc") << CString() << CString('x', 300);
        CHECK(ar.IsStoring() && !ar.IsLoading());
    }
    // Known bytes of the MFC format.
    static const unsigned char expected[] = {
        0x12, 0x56, 0x34, 0xDE, 0xBC, 0x9A, 0x78, 0xFE, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xC0, 0x3F,
        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0xFD, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
        0x03, 'a', 'b', 'c', 0x00, 0xFF, 0x2C, 0x01};
    {
        // Different case and separators: the name resolves to the same file.
        CString other; other.Format("%s/sub/data.bin", directory);
        CFile file(other, CFile::modeRead | CFile::typeBinary);
        CHECK(file.GetLength() == sizeof(expected) + 300);
        unsigned char raw[sizeof(expected)];
        CHECK(file.Read(raw, sizeof(raw)) == sizeof(raw) && std::memcmp(raw, expected, sizeof(raw)) == 0);
        CHECK(file.GetPosition() == sizeof(expected));
        file.SeekToBegin();
        CArchive ar(&file, CArchive::load);
        BYTE b; WORD w; DWORD d; int i; float f; INT64 q; long l; unsigned long ul; CString s1, s2("old"), s3;
        ar >> b >> w >> d >> i >> f >> q >> l >> ul >> s1 >> s2 >> s3;
        CHECK(b == 0x12 && w == 0x3456 && d == 0x789ABCDEu && i == -2 && f == 1.5f && q == 0x0102030405060708ll);
        CHECK(l == -3 && ul == 0xFFFFFFF0ul && s1 == "abc" && s2.IsEmpty() && s3.GetLength() == 300 && s3[299] == 'x');
        bool end = false;
        try { ar >> b; } catch (CArchiveException* e) { end = e->m_cause == CArchiveException::endOfFile; e->Delete(); }
        CHECK(end);
        BYTE rest[4];
        CHECK(ar.Read(rest, 4) == 0);
        file.Close();
        CHECK(file.m_hFile == INVALID_HANDLE_VALUE);
    }
    {   // UTF-16 string marker and the 32-bit length form.
        CString wide; wide.Format("%s\\wide.bin", directory);
        FILE* raw = nullptr;
        CHECK(fopen_s(&raw, wide, "wb") == 0 && raw);
        static const unsigned char bytes[] = {0xFF, 0xFE, 0xFF, 0x02, 'H', 0x00, 0xAC, 0x20, 0xFF, 0xFF, 0xFF, 0x02, 0x00, 0x00, 0x00, 'o', 'k'};
        std::fwrite(bytes, 1, sizeof(bytes), raw); std::fclose(raw);
        CFile file(wide, CFile::modeRead | CFile::typeBinary);
        CArchive ar(&file, CArchive::load);
        CString a, b;
        ar >> a >> b;
        CHECK(a == "H?" && b == "ok");
    }
    {   // A damaged length prefix (1 GB in a 9-byte file) fails before any allocation.
        CString bad; bad.Format("%s\\bad.bin", directory);
        FILE* raw = nullptr;
        CHECK(fopen_s(&raw, bad, "wb") == 0 && raw);
        static const unsigned char bytes[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x00, 'x'};
        std::fwrite(bytes, 1, sizeof(bytes), raw); std::fclose(raw);
        CFile file(bad, CFile::modeRead | CFile::typeBinary);
        CArchive ar(&file, CArchive::load);
        CString text; bool end = false;
        try { ar >> text; } catch (CArchiveException* e) { end = e->m_cause == CArchiveException::endOfFile; e->Delete(); }
        CHECK(end && text.IsEmpty());
    }
    {   // Pointer-sized integers keep 64 bits in an archive.
        CString wide; wide.Format("%s\\ptr.bin", directory);
        { CFile file(wide, CFile::modeCreate | CFile::modeWrite); CArchive ar(&file, CArchive::store); ar << (DWORD_PTR)0x200000001ull << (INT_PTR)-2; }
        CFile file(wide, CFile::modeRead); CArchive ar(&file, CArchive::load);
        DWORD_PTR a = 0; INT_PTR b = 0; ar >> a >> b;
        CHECK(file.GetLength() == 16 && a == 0x200000001ull && b == -2);
    }
    bool thrown = false;
    TRY { CFile missing(CString(directory) + "\\nothing.bin", CFile::modeRead); }
    CATCH(CFileException, e) { thrown = e->m_cause == CFileException::fileNotFound; e->Delete(); }
    END_CATCH
    CHECK(thrown);
    CFile quiet; CFileException error;
    CHECK(!quiet.Open(CString(directory) + "\\nothing.bin", CFile::modeRead, &error) && error.m_cause == CFileException::fileNotFound);
    FILE* none = (FILE*)1;
    CHECK(fopen_s(&none, CString(directory) + "\\nothing.bin", "rb") == ENOENT && none == nullptr);
    CHECK(fourstory::compat::native_path("a\\b\\new.txt") == "a/b/new.txt");
}

static void geometry() {
    CRect r(10, 20, 30, 60);
    CHECK(r.Width() == 20 && r.Height() == 40 && r.PtInRect(CPoint(10, 20)) && !r.PtInRect(CPoint(30, 60)) && !r.IsRectEmpty());
    CHECK(r.CenterPoint() == CPoint(20, 40) && r.TopLeft() == CPoint(10, 20) && r.Size() == CSize(20, 40));
    r.OffsetRect(5, -5); CHECK(r == CRect(15, 15, 35, 55));
    r.InflateRect(1, 2); CHECK(r == CRect(14, 13, 36, 57));
    r.DeflateRect(1, 2); CHECK(r == CRect(15, 15, 35, 55));
    CRect i; CRect a(0, 0, 10, 10), b(5, 5, 20, 20), c(10, 0, 20, 10);
    CHECK(i.IntersectRect(&a, &b) && i == CRect(5, 5, 10, 10));
    CHECK(!i.IntersectRect(&a, &c) && i.IsRectNull());
    CHECK(i.UnionRect(&a, &b) && i == CRect(0, 0, 20, 20));
    CRect n(30, 40, 10, 20); n.NormalizeRect(); CHECK(n == CRect(10, 20, 30, 40));
    CPoint p(1, 2); p += CSize(3, 4); CHECK(p == CPoint(4, 6) && (p - CSize(1, 1)) == CPoint(3, 5));
    CSize d = p - CPoint(1, 1); CHECK(d == CSize(3, 5) && CPoint(d) == CPoint(3, 5) && CSize(p) == CSize(4, 6) && -d == CSize(-3, -5));
    D3DMATRIX m; m._11 = 1; m._44 = 4; CHECK(m.m[0][0] == 1 && m.m[3][3] == 4);
    TRACE("%s %d\n", CString("unused"), 1); ASSERT(false); int kept = 0; VERIFY(++kept); CHECK(kept == 1);
    void* stack = alloca(8); CHECK(stack != nullptr);
}

// On a volume that distinguishes case, names resolve through the directory scan.
static void case_sensitive(const char* directory) {
    const std::string root(directory);
    mkdir((root + "/Data").c_str(), 0755); mkdir((root + "/Data/Tcd").c_str(), 0755);
    FILE* made = std::fopen((root + "/Data/Tcd/TItem.tcd").c_str(), "wb");
    CHECK(made != nullptr); if (made) { std::fputc('x', made); std::fclose(made); }
    struct stat info;
    CHECK(stat((root + "/data/tcd/titem.TCD").c_str(), &info) != 0); // the volume distinguishes case
    CHECK(fourstory::compat::native_path((root + "\\data\\TCD\\titem.TCD").c_str()) == root + "/Data/Tcd/TItem.tcd");
    CHECK(fourstory::compat::native_path((root + "\\DATA\\tcd\\New.bin").c_str()) == root + "/Data/Tcd/New.bin");
    CHECK(fourstory::compat::native_path((root + "\\none\\Sub\\x.bin").c_str()) == root + "/none/Sub/x.bin");
    CFile file;
    CHECK(file.Open((root + "\\data\\tcd\\TITEM.tcd").c_str(), CFile::modeRead) && file.GetLength() == 1);
    CFile created;
    CHECK(created.Open((root + "\\data\\Tcd\\made.bin").c_str(), CFile::modeCreate | CFile::modeWrite));
    CHECK(stat((root + "/Data/Tcd/made.bin").c_str(), &info) == 0);
}

// GetLastError reports the Win32/Winsock code of the last failure.
static void last_error() {
    SetLastError(1234); CHECK(GetLastError() == 1234 && WSAGetLastError() == 1234);
    CHECK(::open("/nonexistent/fourstory", O_RDONLY) < 0 && GetLastError() == ERROR_FILE_NOT_FOUND);
    SetLastError(0); CHECK(GetLastError() == 0);
    errno = EINPROGRESS; CHECK(GetLastError() == WSAEWOULDBLOCK);
    errno = ECONNREFUSED; CHECK(WSAGetLastError() == WSAECONNREFUSED);
    errno = EDOM; CHECK(GetLastError() == (0x20000000u | EDOM));
    char* message = nullptr;
    CHECK(FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 5,
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&message, 0, NULL) == 10);
    CHECK(message && std::strcmp(message, "Error 5.\r\n") == 0); LocalFree(message);
    char small[2]; DWORD size = sizeof(small);
    CHECK(!GetComputerName(small, &size) && GetLastError() == ERROR_BUFFER_OVERFLOW && size > 2);
    char name[256]; size = sizeof(name);
    CHECK(GetComputerNameEx(ComputerNameDnsFullyQualified, name, &size) && size == std::strlen(name) && size > 0);
}

static std::string hex(const BYTE* data, std::size_t count) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    for (std::size_t i = 0; i < count; ++i) { out += digits[data[i] >> 4]; out += digits[data[i] & 15]; }
    return out;
}
// The call sequence of EncryptBuffer in CryptographyExt.cpp.
static bool encrypt_buffer(BYTE* data, DWORD& length, const BYTE* key, DWORD key_length) {
    bool ok = false; DWORD count = length;
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0; HCRYPTKEY derived = 0;
    if (CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(provider, CALG_MD5, 0, 0, &hash)) {
            if (CryptHashData(hash, key, key_length, 0) && CryptDeriveKey(provider, CALG_RC4, hash, CRYPT_EXPORTABLE, &derived)) {
                if (CryptEncrypt(derived, 0, TRUE, 0, data, &count, length)) { length = count; ok = true; }
                CryptDestroyKey(derived);
            }
            CryptDestroyHash(hash);
        }
        CryptReleaseContext(provider, 0);
    }
    return ok;
}
static std::string md5_hex(const std::string& text, std::size_t split) {
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0; BYTE digest[16]; DWORD size = sizeof(digest);
    CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(provider, CALG_MD5, 0, 0, &hash);
    const BYTE* bytes = reinterpret_cast<const BYTE*>(text.data());
    const std::size_t first = split < text.size() ? split : text.size();
    CryptHashData(hash, bytes, static_cast<DWORD>(first), 0);
    CryptHashData(hash, bytes + first, static_cast<DWORD>(text.size() - first), 0);
    const bool ok = CryptGetHashParam(hash, HP_HASHVAL, digest, &size, 0) && size == 16;
    CryptDestroyHash(hash); CryptReleaseContext(provider, 0);
    return ok ? hex(digest, 16) : std::string("failed");
}
static void crypto(const char* fixtures) {
    // RFC 1321 test suite, also with the input split across two calls.
    CHECK(md5_hex("", 0) == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5_hex("abc", 1) == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5_hex("message digest", 7) == "f96b697d7cb7938d525a2f31aaf161d0");
    const std::string digits = "12345678901234567890123456789012345678901234567890123456789012345678901234567890";
    CHECK(md5_hex(digits, 63) == "57edf4a22be3c955ac49da2e2107b67a" && md5_hex(digits, 64) == "57edf4a22be3c955ac49da2e2107b67a");
    // Windows CryptoAPI vectors.
    const BYTE game[] = {0x41,0x35,0x24,0x24,0x38,0x41,0x46,0x53,0x31,0x33,0x41,0x31,0x3a,0x3a,0x2d,0x31,0x31,0x23,0x21,0x2e,
                         0x2e,0x27,0xa7,0x31,0x39,0x37,0x31,0x36,0x41,0x43,0x26,0xa7,0x2f,0x44,0x31,0x3b,0x3b,0x31,0x23,0x00};
    std::ifstream in(std::string(fixtures) + "/rc4-cryptoapi.txt");
    CHECK(in.good());
    int vectors = 0, streams = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream fields(line);
        std::string first; fields >> first;
        if (first == "stream") {
            std::string mode, a, b; fields >> mode >> a >> b;
            const BOOL final = mode == "final=1";
            HCRYPTPROV provider = 0; HCRYPTHASH hash = 0; HCRYPTKEY derived = 0;
            CHECK(CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) && CryptCreateHash(provider, CALG_MD5, 0, 0, &hash));
            CHECK(CryptHashData(hash, reinterpret_cast<const BYTE*>("abc"), 4, 0) && CryptDeriveKey(provider, CALG_RC4, hash, CRYPT_EXPORTABLE, &derived));
            BYTE one[8] = {}, two[8] = {}; DWORD n1 = 8, n2 = 8;
            CHECK(CryptEncrypt(derived, 0, final, 0, one, &n1, 8) && CryptEncrypt(derived, 0, final, 0, two, &n2, 8));
            CHECK(hex(one, 8) == a && hex(two, 8) == b);
            CryptDestroyKey(derived); CryptDestroyHash(hash); CryptReleaseContext(provider, 0);
            ++streams; continue;
        }
        const DWORD key_length = static_cast<DWORD>(std::stoul(first));
        DWORD length = 0; std::string expected; fields >> length >> expected;
        const BYTE* key = key_length == 40 ? game : key_length == 4 ? reinterpret_cast<const BYTE*>("abc") : reinterpret_cast<const BYTE*>("");
        std::vector<BYTE> data(length);
        for (DWORD i = 0; i < length; ++i) data[i] = static_cast<BYTE>(i * 7 + 3);
        DWORD out = length;
        CHECK(encrypt_buffer(data.data(), out, key, key_length) && out == length && hex(data.data(), out) == expected);
        ++vectors;
    }
    CHECK(vectors == 15 && streams == 2);
    // Unsupported requests fail with a Win32 code; nothing is done silently.
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0; HCRYPTKEY derived = 0;
    CHECK(!CryptAcquireContext(&provider, NULL, NULL, 24, CRYPT_VERIFYCONTEXT) && GetLastError() == NTE_BAD_PROV_TYPE && provider == 0);
    CHECK(!CryptAcquireContext(&provider, "container", NULL, PROV_RSA_FULL, 0) && GetLastError() == NTE_BAD_FLAGS);
    CHECK(CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT));
    CHECK(!CryptCreateHash(provider, CALG_SHA1, 0, 0, &hash) && GetLastError() == NTE_BAD_ALGID && hash == 0);
    CHECK(CryptCreateHash(provider, CALG_MD5, 0, 0, &hash) && CryptHashData(hash, reinterpret_cast<const BYTE*>("k"), 1, 0));
    CHECK(!CryptDeriveKey(provider, CALG_3DES, hash, 0, &derived) && GetLastError() == NTE_BAD_ALGID);
    CHECK(!CryptDeriveKey(provider, CALG_RC4, hash, 40u << 16, &derived) && GetLastError() == NTE_BAD_FLAGS);
    CHECK(CryptDeriveKey(provider, CALG_RC4, hash, (128u << 16) | CRYPT_EXPORTABLE, &derived));
    CHECK(!CryptHashData(hash, reinterpret_cast<const BYTE*>("k"), 1, 0) && GetLastError() == NTE_BAD_HASH_STATE);
    BYTE buffer[4] = {}; DWORD count = 4;
    CHECK(!CryptEncrypt(derived, 0, TRUE, 0, buffer, &count, 3) && GetLastError() == ERROR_MORE_DATA);
    CHECK(CryptEncrypt(derived, 0, TRUE, 0, nullptr, &count, 0) && count == 4);
    BYTE round[5] = {1, 2, 3, 4, 5}; DWORD five = 5;
    CHECK(CryptEncrypt(derived, 0, TRUE, 0, round, &five, 5) && CryptDecrypt(derived, 0, TRUE, 0, round, &five) && round[0] == 1 && round[4] == 5);
    CHECK(!CryptEncrypt(hash, 0, TRUE, 0, buffer, &count, 4) && GetLastError() == ERROR_INVALID_HANDLE);
    CHECK(CryptDestroyKey(derived) && CryptDestroyHash(hash) && CryptReleaseContext(provider, 0));
}

// Winsock names over BSD sockets, with a loopback listener.
static void sockets() {
    WSADATA data; CHECK(WSAStartup(MAKEWORD(2, 2), &data) == 0 && data.wVersion == MAKEWORD(2, 2));
    CHECK(WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 1) == INVALID_SOCKET && WSAGetLastError() == WSAEINVAL);
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN address; std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_addr.s_addr = inet_addr("127.0.0.1"); address.sin_port = 0;
    socklen_t length = sizeof(address);
    CHECK(::bind(listener, (SOCKADDR*)&address, sizeof(address)) == 0 && ::listen(listener, 1) == 0);
    CHECK(::getsockname(listener, (SOCKADDR*)&address, &length) == 0);
    hostent* host = gethostbyname("127.0.0.1");
    CHECK(host && std::strcmp(inet_ntoa(*((in_addr*)*host->h_addr_list)), "127.0.0.1") == 0 && host->h_addr_list[1] == nullptr);
    hostent* local = gethostbyname("localhost");
    CHECK(local && local->h_length == 4 && local->h_addrtype == AF_INET && local->h_addr_list[0]);
    CHECK(!gethostbyname("no-such-host.invalid") && WSAGetLastError() == WSAHOST_NOT_FOUND);

    SOCKET client = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
    CHECK(client != INVALID_SOCKET);
    unsigned long on = 1;
    CHECK(ioctlsocket(client, FIONBIO, &on) == 0 && (::fcntl(client, F_GETFL, 0) & O_NONBLOCK));
    CHECK(ioctlsocket(client, 0x1234, &on) == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL);
    int nSIZE = 512 * 1024;
    CHECK(setsockopt(client, SOL_SOCKET, SO_RCVBUF, (const char*)&nSIZE, sizeof(int)) != SOCKET_ERROR);
    const int connected = connect(client, (SOCKADDR*)&address, sizeof(SOCKADDR_IN));
    CHECK(connected == 0 || GetLastError() == WSAEWOULDBLOCK);
    const int server = ::accept(listener, nullptr, nullptr);
    CHECK(server >= 0);
    pollfd ready = {client, POLLOUT, 0};
    CHECK(::poll(&ready, 1, 2000) == 1);
    char received[16] = {};
    CHECK(recv(client, received, sizeof(received), 0) == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
    CHECK(send(client, (const char*)"hello", 5, 0) == 5);
    CHECK(::recv(server, received, sizeof(received), 0) == 5 && std::memcmp(received, "hello", 5) == 0);
    ::close(server);
    // A send to a closed peer fails with a Winsock code; it does not raise SIGPIPE.
    char block[4096] = {}; int result = 0;
    for (int i = 0; i < 1000 && result != SOCKET_ERROR; ++i) { result = send(client, block, sizeof(block), 0); if (result == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) result = 0; }
    CHECK(result == SOCKET_ERROR && (WSAGetLastError() == WSAESHUTDOWN || WSAGetLastError() == WSAECONNRESET));
    CHECK(closesocket(client) == 0 && closesocket(client) == SOCKET_ERROR && WSAGetLastError() == WSAENOTSOCK);
    ::close(listener);
    CHECK(WSACleanup() == 0);
}

static std::string file_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}
static void stdio_and_time(const char* directory) {
    const std::string path = std::string(directory) + "/LOL.txt";
    CStdioFile file;
    CHECK(file.Open(path.c_str(), CStdioFile::typeText | CStdioFile::modeCreate | CStdioFile::modeWrite));
    CString message; message.Format("A5$$8AFS13A1::-11#!..'\xa7" "19716AC&\xa7/D1;;1#");
    file.WriteString(message); file.WriteString("\nsecond\nlast"); file.Close();
    CHECK(file_bytes(path) == std::string("A5$$8AFS13A1::-11#!..'\xa7" "19716AC&\xa7/D1;;1#\r\nsecond\r\nlast"));
    CStdioFile reader(path.c_str(), CFile::modeRead | CFile::typeText);
    CString line;
    CHECK(reader.ReadString(line) && line == message);
    CHECK(reader.ReadString(line) && line == "second");
    CHECK(reader.ReadString(line) && line == "last");
    CHECK(!reader.ReadString(line) && line.IsEmpty());
    reader.SeekToBegin();
    CHECK(reader.GetPosition() == 0 && reader.ReadString(line) && line == message && reader.GetPosition() == message.GetLength() + 2u);
    CHECK(reader.ReadString(line) && line == "second" && reader.Seek(-4, CFile::current) == message.GetLength() + 6u);
    CHECK(reader.ReadString(line) && line == "nd" && reader.ReadString(line) && line == "last");
    reader.Close();
    CStdioFile binary;
    CHECK(binary.Open(path.c_str(), CFile::modeCreate | CFile::modeWrite | CFile::typeBinary));
    binary.WriteString("x\ny"); binary.Close();
    CHECK(file_bytes(path) == "x\ny");
    CStdioFile empty;
    CHECK(empty.Open(path.c_str(), CFile::modeCreate | CFile::modeReadWrite) && !empty.ReadString(line));

    const CTime fixed(2026, 10, 14, 12, 34, 56);
    CHECK(fixed.GetYear() == 2026 && fixed.GetMonth() == 10 && fixed.GetDay() == 14);
    CHECK(fixed.GetHour() == 12 && fixed.GetMinute() == 34 && fixed.GetSecond() == 56 && fixed.GetDayOfWeek() == 4);
    const CTime now = CTime::GetCurrentTime();
    CHECK(now.GetTime() - static_cast<__time64_t>(std::time(nullptr)) <= 0 && now.GetSecond() >= 0 && now.GetSecond() < 61);
    const CTimeSpan span = CTime(2026, 10, 15, 13, 35, 57) - fixed;
    CHECK(span.GetDays() == 1 && span.GetHours() == 1 && span.GetMinutes() == 1 && span.GetSeconds() == 1);
    CHECK(fixed + span == CTime(2026, 10, 15, 13, 35, 57) && fixed < now && CTimeSpan(1, 2, 3, 4).GetTotalSeconds() == 93784);
}

// Critical sections are recursive and exclude other threads; CObject is a base.
struct Derived : CObject { int* flag; explicit Derived(int* f) : flag(f) {} ~Derived() override { *flag = 1; } };
static CRITICAL_SECTION shared_section;
static int shared_counter = 0;
static void* add_many(void*) {
    for (int i = 0; i < 100000; ++i) { EnterCriticalSection(&shared_section); EnterCriticalSection(&shared_section); ++shared_counter; LeaveCriticalSection(&shared_section); LeaveCriticalSection(&shared_section); }
    return nullptr;
}
static void windows_arithmetic() {
    DWORD wide = 5; BYTE narrow = 7; int negative = -1;
    CHECK(max(narrow, wide) == 7 && min(narrow, wide) == 5 && max(3, 4) == 4);
    CHECK(max(negative, 2.5f) == 2.5f && min(negative, 2.5f) == -1.0f);
    CHECK(MulDiv(10, 3, 4) == 8 && MulDiv(-10, 3, 4) == -8 && MulDiv(5, 1, 2) == 3 && MulDiv(-5, 1, 2) == -3);
    CHECK(MulDiv(1, 1, 0) == -1 && MulDiv(0x40000000, 4, 1) == -1 && MulDiv(0x40000000, 4, 2) == -1 && MulDiv(0x3fffffff, 4, 2) == 0x7ffffffe);
    CRect r(4, 8, 12, 16); r.MulDiv(3, 4); CHECK(r.left == 3 && r.top == 6 && r.right == 9 && r.bottom == 12);
}
static void sections_and_objects() {
    InitializeCriticalSection(&shared_section);
    CHECK(TryEnterCriticalSection(&shared_section)); EnterCriticalSection(&shared_section);
    LeaveCriticalSection(&shared_section); LeaveCriticalSection(&shared_section);
    pthread_t a, b;
    pthread_create(&a, nullptr, add_many, nullptr); pthread_create(&b, nullptr, add_many, nullptr);
    pthread_join(a, nullptr); pthread_join(b, nullptr);
    CHECK(shared_counter == 200000);
    DeleteCriticalSection(&shared_section);
    int destroyed = 0; CObject* object = new Derived(&destroyed); delete object;
    CHECK(destroyed == 1 && sizeof(CObject) == sizeof(void*));
}

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) { std::printf("usage: compat_test <empty work directory> [<empty directory on a case-sensitive volume>]\n"); return 2; }
    mkdir((std::string(argv[1]) + "/Sub").c_str(), 0755);
    widths(); strings(); crt(); files(argv[1]); geometry();
    last_error(); crypto(FOURSTORY_COMPAT_FIXTURES); sockets(); stdio_and_time(argv[1]); sections_and_objects(); windows_arithmetic();
    if (argc == 3) case_sensitive(argv[2]);
    std::printf(failures ? "FAILED %d checks\n" : "PASS compat_test\n", failures);
    return failures ? 1 : 0;
}
