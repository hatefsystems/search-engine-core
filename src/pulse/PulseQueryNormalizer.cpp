#include "../../include/search_engine/pulse/PulseQueryNormalizer.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <regex>
#include <sstream>
#include <vector>

namespace search_engine {
namespace pulse {

std::string PulseQueryNormalizer::normalize(const std::string& query) {
    std::string value = query;
    value = normalizeCharacters(value);
    value = normalizeDigits(value);
    value = cleanupPunctuation(value);
    value = lowercaseAscii(value);
    value = cleanupWhitespace(value);
    return value;
}

std::string PulseQueryNormalizer::estimateLanguage(const std::string& query) {
    int arabicCount = 0;
    int persianHintCount = 0;
    int latinCount = 0;

    for (size_t i = 0; i < query.size();) {
        unsigned char c = static_cast<unsigned char>(query[i]);
        uint32_t codepoint = 0;
        size_t length = 1;

        if (c < 0x80) {
            codepoint = c;
            length = 1;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < query.size()) {
            codepoint = ((c & 0x1F) << 6) | (static_cast<unsigned char>(query[i + 1]) & 0x3F);
            length = 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < query.size()) {
            codepoint = ((c & 0x0F) << 12) |
                        ((static_cast<unsigned char>(query[i + 1]) & 0x3F) << 6) |
                        (static_cast<unsigned char>(query[i + 2]) & 0x3F);
            length = 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < query.size()) {
            codepoint = ((c & 0x07) << 18) |
                        ((static_cast<unsigned char>(query[i + 1]) & 0x3F) << 12) |
                        ((static_cast<unsigned char>(query[i + 2]) & 0x3F) << 6) |
                        (static_cast<unsigned char>(query[i + 3]) & 0x3F);
            length = 4;
        }

        if ((codepoint >= 0x0041 && codepoint <= 0x005A) ||
            (codepoint >= 0x0061 && codepoint <= 0x007A)) {
            latinCount++;
        }

        if (codepoint >= 0x0600 && codepoint <= 0x06FF) {
            arabicCount++;
            if (codepoint == 0x067E || codepoint == 0x0686 || codepoint == 0x0698 ||
                codepoint == 0x06AF || codepoint == 0x06CC || codepoint == 0x06A9 ||
                (codepoint >= 0x06F0 && codepoint <= 0x06F9)) {
                persianHintCount++;
            }
        }

        i += length;
    }

    if (arabicCount > 0 && latinCount > 0) {
        return "mixed";
    }
    if (arabicCount > 0) {
        return persianHintCount > 0 ? "fa" : "ar";
    }
    if (latinCount > 0) {
        return "en";
    }
    return "unknown";
}

std::string PulseQueryNormalizer::hashNormalizedQuery(const std::string& normalizedQuery) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : normalizedQuery) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool PulseQueryNormalizer::isPublicSafeQuery(const std::string& rawQuery, const std::string& normalizedQuery) {
    if (normalizedQuery.size() < 2 || normalizedQuery.size() > 80) {
        return false;
    }

    std::string rawLower = lowercaseAscii(rawQuery);
    std::string normalizedLower = lowercaseAscii(normalizedQuery);

    if (rawLower.find('@') != std::string::npos) {
        return false;
    }
    if (rawLower.find("http://") != std::string::npos ||
        rawLower.find("https://") != std::string::npos ||
        rawLower.find("www.") != std::string::npos) {
        return false;
    }

    int digitCount = 0;
    int consecutiveAsciiWord = 0;
    int maxConsecutiveAsciiWord = 0;
    for (unsigned char c : normalizedLower) {
        if (std::isdigit(c)) {
            digitCount++;
        }
        if (std::isalnum(c)) {
            consecutiveAsciiWord++;
            maxConsecutiveAsciiWord = std::max(maxConsecutiveAsciiWord, consecutiveAsciiWord);
        } else {
            consecutiveAsciiWord = 0;
        }
    }

    if (digitCount >= 7 || maxConsecutiveAsciiWord >= 24) {
        return false;
    }

    static const std::regex emailLike(R"(([A-Za-z0-9._%+\-]+)\s+(at|@)\s+([A-Za-z0-9.\-]+))", std::regex_constants::icase);
    if (std::regex_search(rawQuery, emailLike)) {
        return false;
    }

    return true;
}

std::string PulseQueryNormalizer::replaceAll(std::string value, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return value;
    }

    size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return value;
}

std::string PulseQueryNormalizer::normalizeCharacters(std::string value) {
    const std::vector<std::pair<std::string, std::string>> replacements = {
        {"ي", "ی"}, {"ى", "ی"}, {"ك", "ک"}, {"ة", "ه"},
        {"أ", "ا"}, {"إ", "ا"}, {"آ", "ا"}, {"ؤ", "و"}, {"ئ", "ی"},
        {"\xE2\x80\x8C", " "}, // ZWNJ
        {"\xE2\x80\x8D", " "}, // ZWJ
        {"\xE2\x80\x8B", ""},  // Zero-width space
        {"\xEF\xBB\xBF", ""},  // BOM
        {"\xC2\xAD", ""}       // Soft hyphen
    };

    for (const auto& [from, to] : replacements) {
        value = replaceAll(value, from, to);
    }

    return value;
}

std::string PulseQueryNormalizer::normalizeDigits(std::string value) {
    const std::vector<std::pair<std::string, std::string>> digits = {
        {"٠", "0"}, {"١", "1"}, {"٢", "2"}, {"٣", "3"}, {"٤", "4"},
        {"٥", "5"}, {"٦", "6"}, {"٧", "7"}, {"٨", "8"}, {"٩", "9"},
        {"۰", "0"}, {"۱", "1"}, {"۲", "2"}, {"۳", "3"}, {"۴", "4"},
        {"۵", "5"}, {"۶", "6"}, {"۷", "7"}, {"۸", "8"}, {"۹", "9"}
    };

    for (const auto& [from, to] : digits) {
        value = replaceAll(value, from, to);
    }

    return value;
}

std::string PulseQueryNormalizer::cleanupPunctuation(std::string value) {
    const std::vector<std::string> punctuation = {
        "،", "؛", "؟", "٫", "٬", "«", "»", "“", "”", "‘", "’",
        ".", ",", "!", "?", ":", ";", "\"", "'", "`", "(", ")", "[", "]",
        "{", "}", "<", ">", "/", "\\", "|", "-", "_", "+", "=", "*", "&",
        "^", "%", "$", "#", "@", "~"
    };

    for (const auto& mark : punctuation) {
        value = replaceAll(value, mark, " ");
    }

    return value;
}

std::string PulseQueryNormalizer::cleanupWhitespace(std::string value) {
    std::string out;
    out.reserve(value.size());
    bool previousSpace = true;

    for (unsigned char c : value) {
        if (std::isspace(c)) {
            if (!previousSpace) {
                out.push_back(' ');
                previousSpace = true;
            }
        } else {
            out.push_back(static_cast<char>(c));
            previousSpace = false;
        }
    }

    if (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }

    return out;
}

std::string PulseQueryNormalizer::lowercaseAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    return value;
}

} // namespace pulse
} // namespace search_engine

