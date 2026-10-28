#!/usr/bin/env python3
"""Compile focused C++ probes extracted from the real 64-bit source branches.

This is a macOS boundary check. It does not compile the Windows client.
"""
from pathlib import Path
import subprocess
import tempfile
import re

ROOT = Path(__file__).resolve().parents[2]
ENGINE = ROOT / 'source/4Story 5.0 Source Files/Lib/Own/Engine Lib/Engine Lib'


def source(path):
    return path.read_bytes().decode('latin1')


def resource_probe():
    text = source(ENGINE / 'TachyonRes.cpp')
    helper = text.split('#ifdef FOURSTORY_CLIENT_64', 1)[1].split('#endif', 1)[0]
    return r'''
#include <cstdint>
#include <cstring>
#include <cassert>
#include <map>
using DWORD = uint32_t;
using UINT_PTR = uintptr_t;
struct Texture { int value; };
struct Reader {
    unsigned char bytes[8];
    size_t pos = 0;
    void Read(void* out, size_t size) {
        assert(size == 4);
        assert(pos + size <= 8);
        std::memcpy(out, bytes + pos, size);
        pos += size;
    }
};
''' + helper + r'''
int main() {
    static_assert(sizeof(void*) == 8, "64-bit probe required");
    for (DWORD id : {0u, 1u, 0x80000001u, 0xffffffffu}) {
        Reader reader{};
        DWORD next = 0x12345678;
        std::memcpy(reader.bytes, &id, 4);
        std::memcpy(reader.bytes + 4, &next, 4);
        Texture* ptr = reinterpret_cast<Texture*>(UINT_PTR(0xaaaaaaaabbbbbbbb));
        ReadResourceID(reader, ptr);
        assert(reader.pos == 4);
        assert(reinterpret_cast<UINT_PTR>(ptr) == id);
        assert(ResourceID(ptr) == id);
        DWORD remaining = 0;
        reader.Read(&remaining, sizeof(DWORD));
        assert(remaining == next);
        Texture real{42};
        std::map<DWORD, Texture*> resources{{id, &real}};
        ptr = resources.at(ResourceID(ptr));
        assert(ptr == &real && ptr->value == 42);
    }
}
'''


def expanded(path):
    result = subprocess.run(['unifdef', '-DFOURSTORY_CLIENT_64', str(path)],
                            capture_output=True, check=False)
    if result.returncode not in (0, 1):
        raise RuntimeError(result.stderr.decode())
    return result.stdout.decode('latin1')


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def packet_probe():
    text = expanded(ENGINE / 'Packet.cpp')
    funcs = '\n'.join(function(text, sig) for sig in [
        'CPacket& CPacket::operator << ( long param)',
        'CPacket& CPacket::operator >> ( long& param)'])
    return r'''
#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>
using INT = int32_t;
class CPacket {
public:
    std::vector<unsigned char> bytes;
    size_t pos = 0;
    void Write(const void* ptr, size_t count) {
        const auto* data = static_cast<const unsigned char*>(ptr);
        bytes.insert(bytes.end(), data, data + count);
    }
    bool CanRead(size_t count) { return count <= bytes.size() - pos; }
    void Read(void* ptr, size_t count) {
        if (!CanRead(count)) return;
        std::memcpy(ptr, bytes.data() + pos, count);
        pos += count;
    }
    CPacket& operator << (long);
    CPacket& operator >> (long&);
};
''' + funcs + r'''
int main() {
    static_assert(sizeof(long) == 8, "LP64 required");
    CPacket packet;
    packet << long(-1) << long(-2147483647L - 1) << long(2147483647L);
    const std::vector<unsigned char> expected{
        0xff,0xff,0xff,0xff, 0,0,0,0x80, 0xff,0xff,0xff,0x7f};
    assert(packet.bytes == expected);
    long a = 123, b = 123, c = 123;
    packet >> a >> b >> c;
    assert(a == -1 && b == -2147483647L - 1 && c == 2147483647L);
    assert(packet.pos == 12);
    for (size_t size = 0; size < 4; ++size) {
        CPacket shortPacket;
        shortPacket.bytes.resize(size, 0xff);
        long unchanged = 0x1ffffffffL;
        shortPacket >> unchanged;
        assert(unchanged == 0x1ffffffffL && shortPacket.pos == 0);
    }
    long unchanged = 123;
    packet >> unchanged;
    assert(unchanged == 123 && packet.pos == 12);
}
'''


def font_probe():
    path = ROOT / 'source/4Story 5.0 Source Files/Lib/Own/TCML/TCML'
    text = expanded(path / 'TCMLParser.cpp')
    header = source(path / 'cinterf.h')
    struct = header[header.index('typedef struct _tagTcmlLogFont'):]
    struct = struct[:struct.index('}TCML_LOGFONT,* LP_TCML_LOGFONT;') + len('}TCML_LOGFONT,* LP_TCML_LOGFONT;')]
    reader = function(text, 'static void ReadFontPayload(')
    copy = function(expanded(ROOT / 'source/4Story 5.0 Source Files/Lib/Own/TComp/TComp/TComp.cpp'), 'static void CopyFontPayload(')
    return r'''
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cassert>
using INT = int32_t;
using DWORD = uint32_t;
''' + struct + r'''
using LONG = int32_t;
using BYTE = unsigned char;
struct LOGFONT {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet, lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    char lfFaceName[32];
};
''' + reader + copy + r'''
int main() {
    static_assert(sizeof(long) == 8, "LP64 required");
    FILE* file = tmpfile();
    assert(file);
    for (int record = 0; record < 2; ++record) {
        const INT signedFields[] = {-17, 12, 3, 4, 700};
        const unsigned char flags[] = {1,2,3,4,5,6,7,8};
        char face[32] = "Reference font";
        const DWORD ids[] = {0x80000001u + DWORD(record), 0xffaabbccu};
        const DWORD oldPointer = 0xdddddddd;
        fwrite(signedFields, 4, 5, file);
        fwrite(flags, 1, 8, file);
        fwrite(face, 1, 32, file);
        fwrite(ids, 4, 2, file);
        fwrite(&oldPointer, 4, 1, file);
    }
    rewind(file);
    for (int record = 0; record < 2; ++record) {
        TCML_LOGFONT font{};
        ReadFontPayload(file, font);
        assert(ftell(file) == record * 72 + 68);
        assert(font.tlfHeight == -17 && font.tlfWeight == 700);
        assert(font.tlfItalic == 1 && font.tlfPitchAndFamily == 8);
        assert(std::strcmp(font.tlfFaceName, "Reference font") == 0);
        assert(font.tlfId == 0x80000001u + DWORD(record));
        assert(font.tlfColor == 0xffaabbccu);
        LOGFONT lf{};
        CopyFontPayload(font, lf);
        assert(lf.lfHeight == -17 && lf.lfWidth == 12 && lf.lfWeight == 700);
        assert(lf.lfItalic == 1 && lf.lfPitchAndFamily == 8);
        assert(std::strcmp(lf.lfFaceName, "Reference font") == 0);
        fseek(file, 4, SEEK_CUR);
    }
    assert(ftell(file) == 144);
    fclose(file);
}
'''


def zlib_probe():
    text = expanded(ENGINE / 'TachyonCompressor.cpp')
    funcs = '\n'.join(function(text, sig) for sig in [
        'void CTachyonCompressor::Compress()',
        'void CTachyonCompressor::Release()',
        'void CTachyonUncompressor::Uncompress(',
        'void CTachyonUncompressor::Release()'])
    return r'''
#include <zlib.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cassert>
using DWORD = uint32_t;
using UINT = uint32_t;
using BYTE = unsigned char;
using LPCVOID = const void*;
class CTachyonCompressor {
public:
    std::vector<BYTE> m_vData;
    DWORD m_dwCompressedSize = 0;
    BYTE* m_pCompressedData = nullptr;
    void Compress();
    void Release();
};
class CTachyonUncompressor {
public:
    DWORD m_dwSize = 0;
    BYTE* m_pData = nullptr;
    DWORD m_dwPos = 0;
    void Uncompress(LPCVOID, DWORD, DWORD);
    void Release();
};
''' + funcs + r'''
int main() {
    static_assert(sizeof(uLongf) == 8, "LP64 zlib required");
    CTachyonCompressor compressor;
    for (unsigned i = 0; i < 4096; ++i) compressor.m_vData.push_back((i * 17) % 251);
    compressor.Compress();
    CTachyonUncompressor reader;
    reader.Uncompress(compressor.m_pCompressedData, compressor.m_dwCompressedSize, 4096);
    assert(reader.m_dwSize == 4096);
    assert(std::memcmp(reader.m_pData, compressor.m_vData.data(), 4096) == 0);
    reader.Release();
    compressor.Release();
}
'''


def list_probe():
    path = ROOT / 'source/4Story 5.0 Source Files/Lib/Own/TComp/TComp'
    text = expanded(path / 'TList.cpp')
    funcs = '\n'.join(function(text, sig) for sig in [
        'DWORD_PTR TList::GetHitItemData(', 'int TList::AddItem(',
        'BOOL TList::SetItemData(', 'BOOL TList::SetItemDataAllColumn(',
        'DWORD_PTR TList::GetItemData('])
    command = function(expanded(ENGINE / 'T3D.h'), 'struct tagCOMMAND') + ';'
    command = command.replace('struct tagCOMMAND(', 'tagCOMMAND(')  # MSVC ctor spelling
    combo_text = expanded(path / 'TCombo.cpp')
    combo_funcs = '\n'.join(function(combo_text, sig) for sig in [
        'int TCombo::AddItem(', 'DWORD_PTR TCombo::GetItemData(', 'BOOL TCombo::SetItemData('])
    combo_signatures = [line.strip() for line in expanded(path / 'TCombo.h').splitlines()
                        if 'DWORD_PTR' in line and any(name in line for name in
                            ['AddItem', 'GetItemData', 'SetItemData'])]
    field = re.search(r'^\s*DWORD_PTR m_param;', expanded(path / 'TListItem.h'), re.M).group()
    signatures = [line.strip() for line in expanded(path / 'TList.h').splitlines()
                  if ('DWORD_PTR' in line and any(name + '(' in line for name in
                      ['GetHitItemData', 'AddItem', 'SetItemData', 'SetItemDataAllColumn', 'GetItemData']))]
    return r'''
#include <cstdint>
#include <map>
#include <vector>
#include <cassert>
using DWORD_PTR = uintptr_t;
using DWORD = uint32_t;
using LPCTSTR = const char*;
using BOOL = int;
constexpr BOOL FALSE = 0, TRUE = 1;
using CPoint = int;
struct TListItem {
''' + field + r'''
};
using MAP_LISTITEM = std::map<int, TListItem*>;
class TList {
public:
    std::vector<MAP_LISTITEM*> m_items;
    int m_nColumnCount = 2;
    bool IsTypeOf(int) { return true; }
    int AddString(LPCTSTR, int) {
        auto* row = new MAP_LISTITEM;
        (*row)[0] = new TListItem{DWORD_PTR(-1)};
        (*row)[1] = new TListItem{DWORD_PTR(-1)};
        m_items.push_back(row);
        return m_items.size() - 1;
    }
    int GetItemCount() { return m_items.size(); }
    TListItem* GetHitItem(const CPoint&) { return m_items.empty() ? nullptr : (*m_items[0])[0]; }
    ~TList() { for (auto* row : m_items) { for (auto entry : *row) delete entry.second; delete row; } }
''' + '\n'.join(signatures) + r'''
};
''' + funcs + r'''
constexpr int TCML_TYPE_LIST = 1;
class TCombo {
public:
    TList* m_pDownList = nullptr;
''' + '\n'.join(combo_signatures) + r'''
};
''' + combo_funcs + command + r'''
int main() {
    int object = 42;
    const auto pointer = reinterpret_cast<DWORD_PTR>(&object);
    assert(pointer > 0xffffffff);
    tagCOMMAND command(7, pointer);
    assert(command.m_dwID == 7 && command.m_dwParam == pointer);
    TList list;
    assert(list.GetHitItemData(0) == 0);
    assert(list.GetItemData(-1, 0) == DWORD_PTR(-1));
    int row = list.AddItem("quest", pointer);
    assert(list.GetItemData(row, 0) == pointer);
    assert(list.GetHitItemData(0) == pointer);
    assert(*reinterpret_cast<int*>(list.GetItemData(row, 0)) == 42);
    assert(list.SetItemDataAllColumn(row, pointer));
    assert(list.GetItemData(row, 1) == pointer);
    assert(!list.SetItemData(-1, 0, pointer));
    assert(list.SetItemData(row, 1, 0xffffffff));
    assert(list.GetItemData(row, 1) == 0xffffffff);
    assert(list.SetItemData(row, 1, DWORD_PTR(-1)));
    assert(list.GetItemData(row, 1) == DWORD_PTR(-1));
    TCombo combo;
    assert(combo.GetItemData(0, 0) == DWORD_PTR(-1));
    combo.m_pDownList = &list;
    int second = combo.AddItem("combo", pointer);
    assert(combo.GetItemData(second, 0) == pointer);
    assert(combo.SetItemData(second, 1, pointer));
    assert(combo.GetItemData(second, 1) == pointer);
}
'''


def main():
    with tempfile.TemporaryDirectory(prefix='4story-client-widths-') as folder:
        folder = Path(folder)
        for name, code in [('resource_ids', resource_probe()),
                           ('list_data', list_probe()),
                           ('packet_long', packet_probe()),
                           ('tcml_font', font_probe()),
                           ('zlib_lengths', zlib_probe())]:
            src = folder / (name + '.cpp')
            binary = folder / name
            src.write_text(code)
            subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            '-Wno-sign-compare', '-fsanitize=address,undefined',
                            str(src), '-o', str(binary)] + (['-lz'] if name == 'zlib_lengths' else []),
                           check=True)
            subprocess.run([str(binary)], check=True)
            print('PASS ' + name + ' (source-extracted, macOS 64-bit, ASan/UBSan)')


if __name__ == '__main__':
    main()
