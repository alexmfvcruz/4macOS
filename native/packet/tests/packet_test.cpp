#include "../wire.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

void require(bool ok) { if (!ok) throw std::runtime_error("check failed"); }
template<class F> void rejected(F f) {
    bool caught = false;
    try { f(); } catch (const std::logic_error&) { caught = true; }
    require(caught);
}
int main(int argc, char** argv) {
    require(argc == 2);
    std::ifstream input(argv[1]); require(bool(input));
    std::string hex; input >> hex; require(hex.size() % 2 == 0);
    std::vector<std::uint8_t> golden;
    for (std::size_t i = 0; i < hex.size(); i += 2)
        golden.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    story::Writer out(0x1234);
    out.u8(0xab); out.u16(0xcdef); out.u32(0x12345678);
    out.windows_long(-2); out.f32(1.5f); out.i64(0x0102030405060708);
    out.string_bytes("4Story");
    require(out.finish() == golden);
    story::Reader in(golden);
    require(in.u8() == 0xab && in.u16() == 0xcdef && in.u32() == 0x12345678);
    require(in.windows_long() == -2 && in.f32() == 1.5f);
    require(in.i64() == 0x0102030405060708 && in.string_bytes() == "4Story");
    rejected([&] { in.u8(); });
    auto short_packet = golden; short_packet.pop_back();
    rejected([&] { story::Reader r(short_packet); });
    story::Writer huge(1);
    rejected([&] { huge.string_bytes(std::string(65516, 'x')); });
    rejected([&] { huge.string_bytes(std::string("a\0b", 3)); });
    story::Writer empty(0); empty.string_bytes("");
    auto bytes = empty.finish(); story::Reader e(bytes); require(e.string_bytes().empty());
    // Negative legacy string length appears as a huge unsigned value: reject it.
    for (std::size_t i = 16; i < 20; ++i) bytes[i] = 0xff;
    rejected([&] { story::Reader r(bytes); r.string_bytes(); });
    std::cout << "PASS: source-derived golden bytes, scalar/string decoding, bounds; host long="
              << sizeof(long) << " bytes\n";
}
