#pragma once

#include <string>

namespace search_engine {
namespace pulse {

class PulseQueryNormalizer {
public:
    static std::string normalize(const std::string& query);
    static std::string estimateLanguage(const std::string& query);
    static std::string hashNormalizedQuery(const std::string& normalizedQuery);
    static bool isPublicSafeQuery(const std::string& rawQuery, const std::string& normalizedQuery);

private:
    static std::string replaceAll(std::string value, const std::string& from, const std::string& to);
    static std::string normalizeCharacters(std::string value);
    static std::string normalizeDigits(std::string value);
    static std::string cleanupPunctuation(std::string value);
    static std::string cleanupWhitespace(std::string value);
    static std::string lowercaseAscii(std::string value);
};

} // namespace pulse
} // namespace search_engine

