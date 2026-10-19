#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace search_engine::common {

// Strict UTF-8 decoding shared by validation and character-safe generation.
inline std::optional<uint32_t> nextSlugCodePoint(std::string_view text, size_t& offset) {
    if (offset >= text.size()) return std::nullopt;
    const auto first = static_cast<unsigned char>(text[offset++]);
    uint32_t point = first;
    unsigned tails = 0;
    if (first >= 0xC2 && first <= 0xDF) { point &= 31; tails = 1; }
    else if (first >= 0xE0 && first <= 0xEF) { point &= 15; tails = 2; }
    else if (first >= 0xF0 && first <= 0xF4) { point &= 7; tails = 3; }
    else if (first >= 0x80) return std::nullopt;
    for (unsigned j = 0; j < tails; ++j) {
        if (offset == text.size()) return std::nullopt;
        const auto next = static_cast<unsigned char>(text[offset++]);
        if ((next & 0xC0) != 0x80) return std::nullopt;
        point = (point << 6) | (next & 63);
    }
    if ((tails == 1 && point < 0x80) || (tails == 2 && point < 0x800) ||
        (tails == 3 && point < 0x10000) || point > 0x10FFFF ||
        (point >= 0xD800 && point <= 0xDFFF)) return std::nullopt;
    return point;
}

inline bool isProfileSlugLetterOrDigit(uint32_t point) {
    if ((point >= 'a' && point <= 'z') || (point >= 'A' && point <= 'Z') ||
        (point >= '0' && point <= '9') || (point >= 0x06F0 && point <= 0x06F9)) return true;
    // Persian alphabet, including alef with madda. Arabic punctuation, marks,
    // Arabic-only variants and Arabic-Indic digits are not identifier letters.
    switch (point) {
        case 0x0622: case 0x0627: case 0x0628: case 0x067E: case 0x062A:
        case 0x062B: case 0x062C: case 0x0686: case 0x062D: case 0x062E:
        case 0x062F: case 0x0630: case 0x0631: case 0x0632: case 0x0698:
        case 0x0633: case 0x0634: case 0x0635: case 0x0636: case 0x0637:
        case 0x0638: case 0x0639: case 0x063A: case 0x0641: case 0x0642:
        case 0x06A9: case 0x06AF: case 0x0644: case 0x0645: case 0x0646:
        case 0x0648: case 0x0647: case 0x06CC: return true;
        default: return false;
    }
}

inline std::string profileSlugValidationError(std::string_view slug) {
    if (slug.empty()) return "Slug must contain 1 to 100 characters. شناسه باید ۱ تا ۱۰۰ کاراکتر باشد.";
    if (slug.front() == '.' || slug.back() == '.')
        return "Slug cannot start or end with a dot. نقطه در ابتدا یا انتهای شناسه مجاز نیست.";
    for (auto invalid : {"..", ".-", "-.", "._", "_."})
        if (slug.find(invalid) != std::string_view::npos)
            return "Consecutive dots or dots adjacent to other separators are not allowed. نقطهٔ متوالی یا مجاور خط تیره و آندرلاین مجاز نیست.";
    size_t count = 0;
    for (size_t offset = 0; offset < slug.size();) {
        auto point = nextSlugCodePoint(slug, offset);
        if (!point || (*point != '.' && !isProfileSlugLetterOrDigit(*point)))
            return "Use Persian or English letters, Persian or English digits, and dots only. فقط حروف فارسی یا انگلیسی، اعداد فارسی یا انگلیسی و نقطه مجاز است.";
        if (++count > 100) return "Slug must contain 1 to 100 characters. شناسه باید ۱ تا ۱۰۰ کاراکتر باشد.";
    }
    return "";
}

inline bool isValidProfileSlug(std::string_view slug) {
    return profileSlugValidationError(slug).empty();
}

// Only legacy word separators are normalized here. Invalid dot combinations
// are rejected before normalization; name generation has its own cleanup policy.
inline std::optional<std::string> canonicalProfileSlug(std::string_view slug) {
    for (auto invalid : {"..", ".-", "-.", "._", "_."})
        if (slug.find(invalid) != std::string_view::npos) return std::nullopt;
    std::string canonical(slug);
    for (char& c : canonical) if (c == '-' || c == '_') c = '.';
    if (!isValidProfileSlug(canonical)) return std::nullopt;
    return canonical;
}

// Decode exactly once. Only routing may opt into legacy separator handling;
// validators and new stored identifiers always use the canonical grammar.
inline std::optional<std::string> decodeProfileSlug(std::string_view segment, bool allowLegacy = false) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string slug;
    for (size_t i = 0; i < segment.size(); ++i) {
        if (segment[i] != '%') { slug += segment[i]; continue; }
        if (i + 2 >= segment.size()) return std::nullopt;
        const int high = hex(segment[i + 1]), low = hex(segment[i + 2]);
        if (high < 0 || low < 0) return std::nullopt;
        slug += static_cast<char>((high << 4) | low);
        i += 2;
    }
    if (allowLegacy ? !canonicalProfileSlug(slug) : !isValidProfileSlug(slug)) return std::nullopt;
    return slug;
}

inline std::string encodeProfileSlug(std::string_view slug) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (unsigned char c : slug) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') encoded += static_cast<char>(c);
        else { encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15]; }
    }
    return encoded;
}

inline std::string truncateProfileSlug(std::string_view slug, size_t characters = 100) {
    size_t offset = 0, end = 0;
    while (offset < slug.size() && characters--) {
        if (!nextSlugCodePoint(slug, offset)) break;
        end = offset;
    }
    while (end && slug[end - 1] == '.') --end;
    return std::string(slug.substr(0, end));
}

} // namespace search_engine::common
