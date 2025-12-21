#include "../../include/search_engine/common/SlugGenerator.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <chrono>

namespace search_engine {
namespace common {

std::string SlugGenerator::generateSlug(const std::string& name) {
    if (name.empty()) {
        return "profile"; // Fallback for empty names
    }

    std::string normalized = normalizeUnicode(name);
    std::string transliterated = transliterate(normalized);
    std::string cleaned = cleanForSlug(transliterated);

    // Ensure minimum length
    if (cleaned.empty() || cleaned == "-") {
        return "profile";
    }

    // Truncate to maximum length
    if (cleaned.length() > 100) {
        cleaned = cleaned.substr(0, 100);
        // Remove trailing hyphen if truncation created one
        if (!cleaned.empty() && cleaned.back() == '-') {
            cleaned.pop_back();
        }
    }

    return cleaned;
}

std::string SlugGenerator::resolveSlugConflict(const std::string& baseSlug,
                                             const std::function<bool(const std::string&)>& exists) {
    // Check if base slug is available
    if (!exists(baseSlug)) {
        return baseSlug;
    }

    // Try numbered variations
    for (int i = 2; i <= 100; ++i) {
        std::string candidate = baseSlug + "-" + std::to_string(i);
        if (!exists(candidate)) {
            return candidate;
        }
    }

    // Fallback: use timestamp-based suffix
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return baseSlug + "-" + std::to_string(timestamp % 1000000);
}

bool SlugGenerator::isReservedSlug(const std::string& slug) {
    if (slug.empty()) return true;

    std::string lowerSlug = slug;
    std::transform(lowerSlug.begin(), lowerSlug.end(), lowerSlug.begin(), ::tolower);

    const auto& reserved = getReservedWords();
    return reserved.find(lowerSlug) != reserved.end();
}

std::string SlugGenerator::normalizeUnicode(const std::string& input) {
    if (input.empty()) return input;

    std::string result = input;

    // Convert Arabic-Indic numerals to ASCII
    result = convertArabicNumerals(result);

    // Remove combining marks (diacritics) - basic implementation
    result = removeCombiningMarks(result);

    // Normalize Persian/Arabic characters
    // This is a simplified implementation - in production you'd use ICU or similar
    std::unordered_map<std::string, std::string> persianNormalizations = {
        {"ي", "ی"}, // Arabic ye -> Persian ye
        {"ى", "ی"}, // Arabic alif maksura -> Persian ye
        {"ك", "ک"}, // Arabic kaf -> Persian kaf
        {"ة", "ه"}, // Arabic teh marbuta -> Persian he
        {"أ", "ا"}, // Hamza on alif -> alif
        {"إ", "ا"}, // Hamza below alif -> alif
        {"آ", "ا"}, // Alif madda -> alif
        {"ؤ", "و"}, // Hamza on waw -> waw
        {"ئ", "ی"}, // Hamza on ya -> Persian ye
    };

    for (const auto& [from, to] : persianNormalizations) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    return result;
}

std::string SlugGenerator::transliterate(const std::string& input) {
    if (input.empty()) return input;

    std::string result = input;

    // Convert to lowercase for ASCII characters
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Apply transliteration map
    const auto& translitMap = getTransliterationMap();
    for (const auto& [from, to] : translitMap) {
        size_t pos = 0;
        while ((pos = result.find(from, pos)) != std::string::npos) {
            result.replace(pos, from.length(), to);
            pos += to.length();
        }
    }

    return result;
}

std::string SlugGenerator::convertArabicNumerals(const std::string& input) {
    std::string result = input;

    // Arabic-Indic numerals (٠-٩) to ASCII (0-9)
    const std::string arabicNumerals = "٠١٢٣٤٥٦٧٨٩";
    const std::string asciiNumerals = "0123456789";

    for (size_t i = 0; i < arabicNumerals.length(); ++i) {
        std::replace(result.begin(), result.end(), arabicNumerals[i], asciiNumerals[i]);
    }

    return result;
}

std::string SlugGenerator::removeCombiningMarks(const std::string& input) {
    std::string result;

    // Basic combining mark removal - this is simplified
    // In production, you'd use proper Unicode libraries like ICU
    for (char c : input) {
        // Skip common combining marks (U+0300-U+036F range)
        // This is a very basic implementation
        if (static_cast<unsigned char>(c) < 0x80 ||
            (static_cast<unsigned char>(c) >= 0xC0)) { // Keep ASCII and UTF-8 start bytes
            result += c;
        }
        // Note: This is not a complete solution for combining marks
        // A proper implementation would use Unicode normalization libraries
    }

    return result;
}

std::string SlugGenerator::cleanForSlug(const std::string& input) {
    if (input.empty()) return "";

    std::string result = input;

    // Replace spaces and common separators with hyphens
    std::replace(result.begin(), result.end(), ' ', '-');
    std::replace(result.begin(), result.end(), '_', '-');
    std::replace(result.begin(), result.end(), '.', '-');
    std::replace(result.begin(), result.end(), ',', '-');

    // Remove other special characters, keep only letters, numbers, and hyphens
    result.erase(std::remove_if(result.begin(), result.end(),
                                [](char c) {
                                    return !(std::isalnum(static_cast<unsigned char>(c)) ||
                                           c == '-' ||
                                           static_cast<unsigned char>(c) >= 0x80); // Keep Unicode
                                }),
                 result.end());

    // Collapse multiple hyphens
    result = collapseHyphens(result);

    // Remove leading/trailing hyphens
    if (!result.empty() && result.front() == '-') {
        result.erase(0, 1);
    }
    if (!result.empty() && result.back() == '-') {
        result.pop_back();
    }

    return result;
}

std::string SlugGenerator::collapseHyphens(const std::string& input) {
    std::string result;
    bool lastWasHyphen = false;

    for (char c : input) {
        if (c == '-') {
            if (!lastWasHyphen) {
                result += c;
                lastWasHyphen = true;
            }
        } else {
            result += c;
            lastWasHyphen = false;
        }
    }

    return result;
}

const std::unordered_set<std::string>& SlugGenerator::getReservedWords() {
    static std::unordered_set<std::string> reserved = {
        // System routes
        "api", "admin", "test", "debug", "system", "config",

        // Common web paths
        "about", "contact", "privacy", "terms", "help", "support",
        "login", "logout", "register", "signup", "signin", "auth",
        "dashboard", "profile", "profiles", "user", "users", "account",
        "settings", "admin", "administrator", "root", "superuser",

        // HTTP methods and protocols
        "get", "post", "put", "delete", "patch", "options", "head",
        "http", "https", "ftp", "ssh", "git",

        // File extensions
        "html", "htm", "css", "js", "json", "xml", "txt", "md",
        "jpg", "jpeg", "png", "gif", "svg", "webp", "pdf",

        // Common directories
        "assets", "static", "public", "private", "temp", "tmp",
        "cache", "logs", "backup", "archive",

        // Search engine specific
        "search", "crawl", "index", "query", "results", "page",
        "site", "sites", "domain", "url", "link", "links",

        // Status and error pages
        "error", "404", "403", "500", "maintenance", "coming-soon",

        // Common names that might conflict
        "www", "mail", "email", "smtp", "ftp", "ssh", "ssl",

        // Reserved for future features
        "blog", "news", "feed", "rss", "atom", "api-docs",
        "documentation", "docs", "wiki", "forum", "community"
    };

    return reserved;
}

const std::unordered_map<std::string, std::string>& SlugGenerator::getTransliterationMap() {
    static std::unordered_map<std::string, std::string> translitMap = {
        // German umlauts
        {"ä", "ae"}, {"ö", "oe"}, {"ü", "ue"}, {"ß", "ss"},
        {"Ä", "ae"}, {"Ö", "oe"}, {"Ü", "ue"},

        // French accents
        {"à", "a"}, {"â", "a"}, {"ä", "a"}, {"é", "e"}, {"è", "e"},
        {"ê", "e"}, {"ë", "e"}, {"ï", "i"}, {"ô", "o"}, {"ö", "o"},
        {"ù", "u"}, {"û", "u"}, {"ü", "u"}, {"ÿ", "y"}, {"ç", "c"},
        {"À", "a"}, {"Â", "a"}, {"Ä", "a"}, {"É", "e"}, {"È", "e"},
        {"Ê", "e"}, {"Ë", "e"}, {"Ï", "i"}, {"Ô", "o"}, {"Ö", "o"},
        {"Ù", "u"}, {"Û", "u"}, {"Ü", "u"}, {"Ÿ", "y"}, {"Ç", "c"},

        // Spanish accents
        {"á", "a"}, {"é", "e"}, {"í", "i"}, {"ó", "o"}, {"ú", "u"},
        {"ü", "u"}, {"ñ", "n"}, {"Á", "a"}, {"É", "e"}, {"Í", "i"},
        {"Ó", "o"}, {"Ú", "u"}, {"Ü", "u"}, {"Ñ", "n"},

        // Other European characters
        {"å", "a"}, {"Å", "a"}, {"ø", "o"}, {"Ø", "o"}, {"æ", "ae"}, {"Æ", "ae"},
        {"š", "s"}, {"Š", "s"}, {"ž", "z"}, {"Ž", "z"}, {"č", "c"}, {"Č", "c"},
        {"ř", "r"}, {"Ř", "r"}, {"ť", "t"}, {"Ť", "t"}, {"ň", "n"}, {"Ň", "n"},

        // Cyrillic (basic mapping)
        {"а", "a"}, {"б", "b"}, {"в", "v"}, {"г", "g"}, {"д", "d"},
        {"е", "e"}, {"ё", "e"}, {"ж", "zh"}, {"з", "z"}, {"и", "i"},
        {"й", "y"}, {"к", "k"}, {"л", "l"}, {"м", "m"}, {"н", "n"},
        {"о", "o"}, {"п", "p"}, {"р", "r"}, {"с", "s"}, {"т", "t"},
        {"у", "u"}, {"ф", "f"}, {"х", "kh"}, {"ц", "ts"}, {"ч", "ch"},
        {"ш", "sh"}, {"щ", "sch"}, {"ъ", ""}, {"ы", "y"}, {"ь", ""},
        {"э", "e"}, {"ю", "yu"}, {"я", "ya"},

        // Uppercase Cyrillic
        {"А", "a"}, {"Б", "b"}, {"В", "v"}, {"Г", "g"}, {"Д", "d"},
        {"Е", "e"}, {"Ё", "e"}, {"Ж", "zh"}, {"З", "z"}, {"И", "i"},
        {"Й", "y"}, {"К", "k"}, {"Л", "l"}, {"М", "m"}, {"Н", "n"},
        {"О", "o"}, {"П", "p"}, {"Р", "r"}, {"С", "s"}, {"Т", "t"},
        {"У", "u"}, {"Ф", "f"}, {"Х", "kh"}, {"Ц", "ts"}, {"Ч", "ch"},
        {"Ш", "sh"}, {"Щ", "sch"}, {"Ъ", ""}, {"Ы", "y"}, {"Ь", ""},
        {"Э", "e"}, {"Ю", "yu"}, {"Я", "ya"},

        // Chinese characters (common names) - very limited mapping
        {"李", "li"}, {"王", "wang"}, {"张", "zhang"}, {"刘", "liu"},
        {"陈", "chen"}, {"杨", "yang"}, {"黄", "huang"}, {"赵", "zhao"},

        // Japanese characters (very basic)
        {"あ", "a"}, {"い", "i"}, {"う", "u"}, {"え", "e"}, {"お", "o"},
        {"か", "ka"}, {"き", "ki"}, {"く", "ku"}, {"け", "ke"}, {"こ", "ko"},
        {"さ", "sa"}, {"し", "shi"}, {"す", "su"}, {"せ", "se"}, {"そ", "so"},
        {"た", "ta"}, {"ち", "chi"}, {"つ", "tsu"}, {"て", "te"}, {"と", "to"},
        {"な", "na"}, {"に", "ni"}, {"ぬ", "nu"}, {"ね", "ne"}, {"の", "no"},
        {"は", "ha"}, {"ひ", "hi"}, {"ふ", "fu"}, {"へ", "he"}, {"ほ", "ho"},
        {"ま", "ma"}, {"み", "mi"}, {"む", "mu"}, {"め", "me"}, {"も", "mo"},
        {"や", "ya"}, {"ゆ", "yu"}, {"よ", "yo"},
        {"ら", "ra"}, {"り", "ri"}, {"る", "ru"}, {"れ", "re"}, {"ろ", "ro"},
        {"わ", "wa"}, {"を", "wo"}, {"ん", "n"}
    };

    return translitMap;
}

} // namespace common
} // namespace search_engine
