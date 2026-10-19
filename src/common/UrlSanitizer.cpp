#include "../../include/search_engine/common/UrlSanitizer.h"

#include <sstream>
#include <cstdint>

namespace search_engine::common {

static inline bool isAsciiSpace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

std::string sanitizeUrl(const std::string& input) {
    if (input.empty()) return input;

    // Trim ASCII whitespace
    size_t start = 0;
    size_t end = input.size();
    while (start < end && isAsciiSpace(static_cast<unsigned char>(input[start]))) start++;
    while (end > start && isAsciiSpace(static_cast<unsigned char>(input[end - 1]))) end--;

    std::string s = input.substr(start, end - start);

    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        // ASCII fast path
        if ((c & 0x80) == 0) {
            // Skip control chars
            if (c < 0x20 || c == 0x7F) { i++; continue; }
            out.push_back(static_cast<char>(c));
            i++;
            continue;
        }

        // Decode minimal UTF-8 to inspect codepoint
        uint32_t cp = 0;
        size_t adv = 1;
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
            cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
            adv = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 2]) & 0x3F);
            adv = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < s.size()) {
            cp = ((c & 0x07) << 18) |
                 ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                 ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                 (static_cast<unsigned char>(s[i + 3]) & 0x3F);
            adv = 4;
        } else {
            // Invalid UTF-8 start, skip byte
            i++;
            continue;
        }

        // Filter zero-width/formatting and directional codepoints
        if (cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x2060 || cp == 0xFEFF ||
            cp == 0x200E || cp == 0x200F ||
            cp == 0x202A || cp == 0x202B || cp == 0x202C || cp == 0x202D || cp == 0x202E ||
            cp == 0x2066 || cp == 0x2067 || cp == 0x2068 || cp == 0x2069) {
            i += adv;
            continue;
        }

        // Re-emit original bytes for this sequence (we are not normalizing UTF-8 here)
        for (size_t k = 0; k < adv; ++k) {
            out.push_back(s[i + k]);
        }
        i += adv;
    }

    return out;
}

std::string hexDump(const std::string& input) {
    std::ostringstream oss;
    oss.setf(std::ios::hex, std::ios::basefield);
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned int v = static_cast<unsigned char>(input[i]);
        if (i) oss << ' ';
        if (v < 0x10) oss << '0';
        oss << v;
    }
    return oss.str();
}

} // namespace search_engine::common

