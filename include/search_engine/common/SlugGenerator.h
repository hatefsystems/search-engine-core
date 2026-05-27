#pragma once

#include <string>
#include <functional>
#include <vector>
#include <unordered_set>

namespace search_engine {
namespace common {

/**
 * SlugGenerator - Utility class for generating clean, URL-friendly slugs
 *
 * Features:
 * - Unicode normalization (NFKD) with diacritic removal
 * - Persian/Arabic character preservation and normalization
 * - Automatic transliteration for non-ASCII characters
 * - Special character and punctuation handling
 * - Collision resolution with automatic numbering
 * - Reserved word checking
 */
class SlugGenerator {
public:
    /**
     * Generate a clean slug from a name
     * @param name Input name to convert to slug
     * @return URL-friendly slug
     */
    static std::string generateSlug(const std::string& name);

    /**
     * Resolve slug conflicts by adding numbers
     * @param baseSlug Base slug to resolve
     * @param exists Function that checks if a slug exists
     * @return Available slug (baseSlug, baseSlug-2, baseSlug-3, etc.)
     */
    static std::string resolveSlugConflict(const std::string& baseSlug,
                                          const std::function<bool(const std::string&)>& exists);

    /**
     * Check if a slug is reserved
     * @param slug Slug to check
     * @return true if slug is reserved
     */
    static bool isReservedSlug(const std::string& slug);

    /**
     * Normalize Unicode text (NFKD normalization with diacritic removal)
     * @param input Input text
     * @return Normalized text
     */
    static std::string normalizeUnicode(const std::string& input);

    /**
     * Transliterate non-ASCII characters to ASCII equivalents
     * @param input Input text
     * @return Transliterated text
     */
    static std::string transliterate(const std::string& input);

private:
    /**
     * Convert Arabic-Indic numerals to ASCII
     * @param input Input text
     * @return Text with converted numerals
     */
    static std::string convertArabicNumerals(const std::string& input);

    /**
     * Clean and normalize text for slug generation
     * @param input Input text
     * @return Cleaned text ready for slug generation
     */
    static std::string cleanForSlug(const std::string& input);

    /**
     * Remove combining marks (diacritics) from Unicode text
     * @param input Input text
     * @return Text without combining marks
     */
    static std::string removeCombiningMarks(const std::string& input);

    /**
     * Collapse multiple hyphens and clean up
     * @param input Input text
     * @return Cleaned text
     */
    static std::string collapseHyphens(const std::string& input);

    /**
     * Get reserved slug words
     * @return Set of reserved words
     */
    static const std::unordered_set<std::string>& getReservedWords();

    /**
     * Transliteration mapping for common characters
     * @return Map of character to transliteration
     */
    static const std::unordered_map<std::string, std::string>& getTransliterationMap();
};

} // namespace common
} // namespace search_engine
