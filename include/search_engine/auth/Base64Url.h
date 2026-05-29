#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace search_engine { namespace auth {

/**
 * @brief Base64URL (RFC 4648 §5) encoding/decoding — no padding.
 *
 * Used for JWT segment encoding and for storing salt/hash bytes in the
 * compact password hash string. Header-only; depends on nothing.
 */
namespace Base64Url {

inline const char* alphabet() {
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
}

inline std::string encode(const unsigned char* data, size_t len) {
    static const char* alpha = alphabet();
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 3 <= len) {
        uint32_t triple = (uint32_t(data[i]) << 16) | (uint32_t(data[i+1]) << 8) | uint32_t(data[i+2]);
        out.push_back(alpha[(triple >> 18) & 0x3F]);
        out.push_back(alpha[(triple >> 12) & 0x3F]);
        out.push_back(alpha[(triple >>  6) & 0x3F]);
        out.push_back(alpha[(triple      ) & 0x3F]);
        i += 3;
    }
    if (i < len) {
        uint32_t rem = uint32_t(data[i]) << 16;
        if (i + 1 < len) rem |= uint32_t(data[i+1]) << 8;
        out.push_back(alpha[(rem >> 18) & 0x3F]);
        out.push_back(alpha[(rem >> 12) & 0x3F]);
        if (i + 1 < len) {
            out.push_back(alpha[(rem >>  6) & 0x3F]);
        }
    }
    return out;
}

inline std::string encode(const std::string& bytes) {
    return encode(reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
}

inline int charIdx(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return 26 + (c - 'a');
    if (c >= '0' && c <= '9') return 52 + (c - '0');
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

// Returns empty vector on invalid input.
inline std::vector<unsigned char> decode(const std::string& in) {
    std::vector<unsigned char> out;
    out.reserve((in.size() * 3) / 4);
    uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=') break;
        int v = charIdx(c);
        if (v < 0) return {};   // invalid char
        buf = (buf << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

inline std::string decodeToString(const std::string& in) {
    auto bytes = decode(in);
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace Base64Url
}}  // namespace
