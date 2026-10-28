#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace story {
// Unencrypted packet framing only. Multibyte strings are preserved as bytes.
class Writer {
    std::vector<std::uint8_t> data_;
    void reserve_bytes(std::size_t n) {
        if (n > 65535 - data_.size()) throw std::length_error("packet too large");
    }
    void integer(std::uint64_t value, unsigned width) {
        reserve_bytes(width);
        for (unsigned i = 0; i < width; ++i)
            data_.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }
public:
    explicit Writer(std::uint16_t id) {
        integer(16, 2); integer(id, 2); integer(0, 4); integer(0, 8);
    }
    void u8(std::uint8_t n) { integer(n, 1); }
    void u16(std::uint16_t n) { integer(n, 2); }
    void u32(std::uint32_t n) { integer(n, 4); }
    void windows_long(std::int32_t n) { integer(static_cast<std::uint32_t>(n), 4); }
    void i64(std::int64_t n) { integer(static_cast<std::uint64_t>(n), 8); }
    void f32(float n) {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
        std::uint32_t bits; std::memcpy(&bits, &n, 4); u32(bits);
    }
    void string_bytes(const std::string& s) {
        if (s.find('\0') != std::string::npos)
            throw std::invalid_argument("LPCTSTR strings cannot contain embedded NUL");
        reserve_bytes(4 + s.size());
        u32(static_cast<std::uint32_t>(s.size()));
        data_.insert(data_.end(), s.begin(), s.end());
    }
    std::vector<std::uint8_t> finish() const {
        auto result = data_;
        result[0] = static_cast<std::uint8_t>(result.size());
        result[1] = static_cast<std::uint8_t>(result.size() >> 8);
        return result;
    }
};

class Reader {
    const std::vector<std::uint8_t>& data_;
    std::size_t cursor_ = 16;
    std::uint64_t integer(unsigned width) {
        if (width > data_.size() - cursor_) throw std::out_of_range("truncated payload");
        std::uint64_t result = 0;
        for (unsigned i = 0; i < width; ++i)
            result |= std::uint64_t(data_[cursor_++]) << (8 * i);
        return result;
    }
public:
    explicit Reader(const std::vector<std::uint8_t>& data) : data_(data) {
        if (data.size() < 16 || data.size() > 65535 ||
            (std::size_t(data[0]) | (std::size_t(data[1]) << 8)) != data.size())
            throw std::invalid_argument("invalid packet size");
    }
    std::uint8_t u8() { return static_cast<std::uint8_t>(integer(1)); }
    std::uint16_t u16() { return static_cast<std::uint16_t>(integer(2)); }
    std::uint32_t u32() { return static_cast<std::uint32_t>(integer(4)); }
    std::int32_t windows_long() {
        const auto bits = u32(); std::int32_t n; std::memcpy(&n, &bits, 4); return n;
    }
    std::int64_t i64() {
        const auto bits = integer(8); std::int64_t n; std::memcpy(&n, &bits, 8); return n;
    }
    float f32() { const auto bits = u32(); float n; std::memcpy(&n, &bits, 4); return n; }
    std::string string_bytes() {
        const auto length = u32();
        if (length > data_.size() - cursor_) throw std::out_of_range("invalid string length");
        std::string result(data_.begin() + cursor_, data_.begin() + cursor_ + length);
        cursor_ += length; return result;
    }
};
}
